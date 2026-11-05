"""Cross-platform checks for native ESC compositing (experimental, opt-in).

CI does not validate D3D7 stateblock support or visual order under Wine
and Windows; those MUST be tested on real game hardware.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def policy(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("C compiler needed to verify native compositing policy")
    folder=tmp_path_factory.mktemp("ui_comp")
    source=folder/"test.c"
    out=folder/"test.so"
    source.write_text(
        '#include "ui_compositor_policy.h"\n'
        'int eligible(int e,int open,int bailout,int layer,unsigned count,unsigned fvf){'
        'return SpiderUIComp_ShouldDefer(e,open,bailout,layer,count,fvf);'
        '}\n'
        'unsigned tint(unsigned argb){return SpiderUIComp_Tint(argb);}\n'
    )
    subprocess.run(
        [cc,"-std=c11","-Wall","-Wextra","-Werror","-fPIC","-shared",
         "-I",str(ROOT),str(source),"-o",str(out)],
        capture_output=True,text=True,check=True,
    )
    lib=ct.CDLL(str(out))
    lib.eligible.argtypes=[ct.c_int]*4+[ct.c_uint]*2
    lib.eligible.restype=ct.c_int
    lib.tint.argtypes=[ct.c_uint]
    lib.tint.restype=ct.c_uint
    return lib


@pytest.mark.parametrize("layer", [3,4])
def test_menu_and_decoration_are_replayed_only_inside_verified_scene(policy,layer):
    assert policy.eligible(1,1,0,layer,4,0x144)==1
    for args in [(0,1,0),(1,0,0),(1,1,1)]:
        assert policy.eligible(*args,layer,4,0x144)==0


@pytest.mark.parametrize("layer", [0,1,2,-1,5])
def test_world_hud_and_fullscreen_overlay_are_never_queued_as_menu(policy,layer):
    assert policy.eligible(1,1,0,layer,4,0x144)==0


def test_non_screen_space_vertex_formats_and_counts_fail_closed(policy):
    assert policy.eligible(1,1,0,3,8,0x144)==0
    assert policy.eligible(1,1,0,4,4,0x112)==0


def test_trial_tint_uses_source_color_without_blowing_up_opacity(policy):
    assert policy.tint(0x80041010)==0x50041010
    assert policy.tint(0x2078AA11)==0x2078AA11
    assert policy.tint(0x00041010)==0x00041010


def test_host_defers_native_esc_to_end_scene_with_original_state_restored():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    start=src.index("/* Reversible Direct3D7 pause compositor.")
    end=src.index("static HRESULT WINAPI hooked_draw_primitive(",start)
    compositor=src[start:end]
    for marker in (
        "ui_comp_capture(",
        "ui_comp_flush(",
        "ui_compositor_scene_start(",
        "ui_compositor_scene_finish(",
        "IDirect3DDevice7_CreateStateBlock(device,D3DSBT_ALL,",
        "IDirect3DDevice7_ApplyStateBlock(device,c->state_token)",
        "IDirect3DDevice7_GetTexture(device,0,&texture)",
        "IDirect3DDevice7_SetTexture(device,0,c->texture)",
        "IDirect3DDevice7_ApplyStateBlock(device,saved)",
        "IDirect3DDevice7_SetTexture(device,0,saved_texture)",
        "D3DRENDERSTATE_ZENABLE,D3DZB_FALSE",
        "SPIDER_UI_COMP_MAX_COMMANDS",
        "ui_comp_release(device)",
        "SpiderUIComp_Tint(",
    ):
        assert marker in compositor
    # Crucial order: tint must be drawn after 3D/HUD but before 4:3
    # native motif/menu, and all before original EndScene call.
    flush=compositor.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert flush.index("ui_comp_draw_screen_tint(")<flush.index(
        "for(layer=SPIDER_UI_COMP_PAUSE_DECOR;")
    assert compositor.index("ui_comp_flush((IDirect3DDevice7*)self);") < compositor.index(
        "return ui_original_end_scene(self);")

    assert 'SPIDERMAN2K1FIX_UI_COMPOSITOR_TEST' in src
    assert 'SPIDERMAN2K1FIX_UI_COMPOSITOR_TINT' in src
    assert 'vt[5]=(void*)ui_compositor_scene_start;' in src
    assert 'vt[6]=(void*)ui_compositor_scene_finish;' in src
    assert 'ui_comp_capture((IDirect3DDevice7*)self' in src
    assert "v=maybe_correct_native_pause43(" in src


def test_compositor_does_not_change_normal_main_menu_or_native_3d_path():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    assert "if(native_esc_primitive && v!=verts &&" in src
    assert "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)" in src
    assert "if(v==verts) v=maybe_correct_horplus(" in src
    assert "scale_menu_vertices(fvf,verts,count" in src
    assert 'UI-COMPOSITOR: OFF; existing UI canvas draw path unchanged' in src
    assert 'UI-COMPOSITOR: menu draw outside BeginScene, original draw retained' in src
    assert 'UI-COMPOSITOR: D3D7 stateblock/texture capture failed, native draw fallback' in src



def test_synthetic_tint_is_off_unless_explicitly_opted_in():
    core=(ROOT/"spiderman2k1fix.c").read_text()
    assert "static volatile LONG ui_compositor_tint_enabled=0;" in core
    assert "UI-COMPOSITOR-TINT: OFF - no synthetic fullscreen veil" in core
    assert "UI-COMPOSITOR-TINT: ON - A/B synthetic fullscreen veil" in core
    assert "tint[0]=='1'" in core
    assert "tint[0]=='0'" not in core[core.index("/* Separate compositor opt-in:"):]


def test_native_pause_motif_keeps_original_four_by_three_geometry():
    core=(ROOT/"spiderman2k1fix.c").read_text()
    marker="static const void *maybe_correct_native_motif43("
    src=core.split(marker,1)[1].split("static volatile LONG native_motif43_gutter_hits",1)[0]
    assert "center+(v[i].x-center)*native_esc43_scale()" in src
    # A full-screen synthetic overlay is a DISTINCT optional layer,
    # not a stretched version of the original motif geometry.
    assert "ui_compositor_trial_enabled,0,0)!=1" in src



def test_every_end_scene_has_bounded_compositor_status():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    assert "static volatile LONG ui_comp_flushed_frames=0" in src
    assert "InterlockedIncrement(&ui_comp_flushed_frames)" in src
    assert "frame<=3 || frame%180==0 || !all_ok" in src
    assert "UI-COMPOSITOR-FRAME: frame=" in src
    # With a varying number of commands per frame, testing the cumulative
    # primitive counter modulo 1200 could fail to log any successful replay.
    flush=src.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert "n%1200==0" not in flush


def test_background_coverage_probe_is_passive_and_scene_scoped():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    probe=src.split("static void ui_comp_probe_fullscreen_source(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_primitive(",1)[0]
    assert "UI-COMPOSITOR-COVERAGE-PROBE:" in probe
    assert "native_pause43_visible_near_flip(" in probe
    assert "!ui_comp_scene_open || ui_comp_bailout || native_esc" in probe
    assert "D3DRENDERSTATE_ALPHABLENDENABLE" in probe
    assert "IDirect3DDevice7_SetRenderState(" not in probe
    assert "original_draw_primitive(" not in probe
    assert "ui_comp_probe_fullscreen_source(self,type,fvf,verts,count,native_esc_primitive)" in src
