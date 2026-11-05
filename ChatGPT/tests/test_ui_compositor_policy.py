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
        "ui_compositor_hooked_begin_scene(",
        "ui_compositor_hooked_end_scene(",
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
    assert 'vt[5]=(void*)ui_compositor_hooked_begin_scene;' in src
    assert 'vt[6]=(void*)ui_compositor_hooked_end_scene;' in src
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
