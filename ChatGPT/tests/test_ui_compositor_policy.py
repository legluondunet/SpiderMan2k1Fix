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
        '#include "native_pause_43_policy.h"\n'
        'int eligible(int e,int open,int bailout,int layer,unsigned count,unsigned fvf){'
        'return SpiderUIComp_ShouldDefer(e,open,bailout,layer,count,fvf);'
        '}\n'
        'int replay_pass(int layer,unsigned source_color,int proven,'
        'int drawn,unsigned background){'
        'return SpiderUIComp_ReplayPassForDraw(layer,source_color,proven,'
        'drawn,background);}\n'
        'int coverage(float h,const SpiderUICompYSpan *sp,unsigned n){'
        'return SpiderUIComp_CoversFullHeight(sp,n,h);}\n'
        'int native_backdrop(const SpiderUICompBackdropSample *s,'
        'unsigned n,float w,float h,unsigned *color){'
        'return SpiderUIComp_IdentifyNativeBackdrop(s,n,w,h,color);}\n'
        'int dynamic_ribbon(float z,float rhw,unsigned color){'
        'return SpiderNativePause43_CompositorRibbon(z,rhw,color);}\n'
        'int dynamic_motif(float z,float rhw,unsigned color){'
        'return SpiderNativePause43_CompositorMotif(z,rhw,color);}\n'
        'int dynamic_text(float z,float rhw){'
        'return SpiderNativePause43_CompositorText(z,rhw);}\n'

    )
    subprocess.run(
        [cc,"-std=c11","-Wall","-Wextra","-Werror","-fPIC","-shared",
         "-I",str(ROOT),str(source),"-o",str(out)],
        capture_output=True,text=True,check=True,
    )
    lib=ct.CDLL(str(out))
    lib.eligible.argtypes=[ct.c_int]*4+[ct.c_uint]*2
    lib.eligible.restype=ct.c_int
    lib.replay_pass.argtypes=[ct.c_int,ct.c_uint,ct.c_int,ct.c_int,ct.c_uint]
    lib.replay_pass.restype=ct.c_int
    class YSpan(ct.Structure):
        _fields_=[("top",ct.c_float),("bottom",ct.c_float)]
    lib.YSpan=YSpan
    lib.coverage.argtypes=[ct.c_float,ct.POINTER(YSpan),ct.c_uint]
    lib.coverage.restype=ct.c_int
    class BackdropSample(ct.Structure):
        _fields_=[("left",ct.c_float),("right",ct.c_float),
                  ("top",ct.c_float),("bottom",ct.c_float),
                  ("color",ct.c_uint)]
    lib.BackdropSample=BackdropSample
    lib.native_backdrop.argtypes=[ct.POINTER(BackdropSample),ct.c_uint,
                                 ct.c_float,ct.c_float,ct.POINTER(ct.c_uint)]
    lib.native_backdrop.restype=ct.c_int
    for name in ("dynamic_ribbon","dynamic_motif"):
        fn=getattr(lib,name)
        fn.argtypes=[ct.c_float,ct.c_float,ct.c_uint]
        fn.restype=ct.c_int
    lib.dynamic_text.argtypes=[ct.c_float,ct.c_float]
    lib.dynamic_text.restype=ct.c_int
    return lib


@pytest.mark.parametrize("layer", [3,4])
def test_menu_and_decoration_are_replayed_only_inside_verified_scene(policy,layer):
    assert policy.eligible(1,1,0,layer,4,0x144)==1
    for args in [(0,1,0),(1,0,0),(1,1,1)]:
        assert policy.eligible(*args,layer,4,0x144)==0


@pytest.mark.parametrize("layer", [0,2,-1,5])
def test_world_and_fullscreen_overlay_are_never_queued_as_menu(policy,layer):
    assert policy.eligible(1,1,0,layer,4,0x144)==0


def test_hud_layer_is_eligible_only_with_native_context_and_valid_quad(policy):
    assert policy.eligible(1,1,0,1,4,0x144)==1
    for args in ((0,1,0),(1,0,0),(1,1,1)):
        assert policy.eligible(*args,1,4,0x144)==0
    assert policy.eligible(1,1,0,1,3,0x144)==0
    assert policy.eligible(1,1,0,1,4,0x112)==0


def test_non_screen_space_vertex_formats_and_counts_fail_closed(policy):
    assert policy.eligible(1,1,0,3,8,0x144)==0
    assert policy.eligible(1,1,0,4,4,0x112)==0


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
    ):
        assert marker in compositor
    # Source-backed gutters must be drawn before any 4:3 HUD/decoration
    # replay, all before the game's original EndScene call.
    flush=compositor.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert flush.index("ui_comp_draw_native_exterior_gutters(")<flush.index(
        "for(layer=SPIDER_UI_COMP_HUD;")
    assert compositor.index("ui_comp_flush((IDirect3DDevice7*)self);") < compositor.index(
        "return ui_original_end_scene(self);")

    assert 'SPIDERMAN2K1FIX_UI_COMPOSITOR_TEST' in src
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



def test_native_pause_motif_keeps_original_four_by_three_geometry():
    core=(ROOT/"spiderman2k1fix.c").read_text()
    marker="static const void *maybe_correct_native_motif43("
    src=core.split(marker,1)[1].split("static volatile LONG native_motif43_gutter_hits",1)[0]
    assert "center+(v[i].x-center)*native_esc43_scale()" in src
    # Source motif geometry must retain its native 4:3 proportional map;
    # the physical outer margins are independent and do not stretch it.
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
        "/* Final V19/V25:",1)[0]
    assert "UI-COMPOSITOR-COVERAGE-PROBE:" in probe
    assert "native_pause43_visible_near_flip(" in probe
    assert "!ui_comp_scene_open || ui_comp_bailout || native_esc" in probe
    assert "D3DRENDERSTATE_ALPHABLENDENABLE" in probe
    assert "IDirect3DDevice7_SetRenderState(" not in probe
    assert "original_draw_primitive(" not in probe
    assert "ui_comp_probe_fullscreen_source(self,type,fvf,verts,count,native_esc_primitive)" in src



def test_native_spider_source_geometry_probe_is_one_shot_and_passive():
    core=(ROOT/"spiderman2k1fix.c").read_text()
    assert "SpiderMenuVertex source_v[4];" in core
    assert "UI-COMPOSITOR-MOTIF-SOURCE:" in core
    assert "UI-COMPOSITOR-MOTIF-SUMMARY:" in core
    assert "InterlockedCompareExchange(&ui_comp_source_dump_once,1,0)==0" in core
    assert "InterlockedCompareExchange(&ui_comp_flushed_frames,0,0)>=59" in core
    assert "memcpy(c->source_v,source_vertices,4u*sizeof(SpiderMenuVertex))" in core
    assert "ui_comp_capture((IDirect3DDevice7*)self,type,fvf,v,verts,count,flags" in core
    # The original geometry is never submitted as a new rendering layer.
    flush=core.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert "original_draw_primitive(device,c->type,c->fvf,c->v,4,c->flags)" in flush
    assert "original_draw_primitive(device,c->type,c->fvf,c->source_v" not in flush
    assert "ui_compositor_trial_enabled,0,0)==1" in core



def test_native_source_exterior_y_coverage_requires_no_gaps(policy):
    Y=policy.YSpan
    # Based on the native source geometry logged at 1920x1080, the
    # original exterior rectangles overlap vertically to cover both
    # edge bands; there is no guessed independent fullscreen colour.
    complete=(Y*5)(Y(0,234),Y(0,333),Y(306,657),
                   Y(625.5,904.5),Y(904.5,1080))
    assert policy.coverage(1080,complete,5)==1
    missing=(Y*2)(Y(0,390),Y(402,1080))
    assert policy.coverage(1080,missing,2)==0
    beginning=(Y*1)(Y(20,1080))
    assert policy.coverage(1080,beginning,1)==0
    ending=(Y*2)(Y(0,400),Y(400,1030))
    assert policy.coverage(1080,ending,2)==0


def test_native_backdrop_is_derived_from_four_edge_source_not_spider(policy):
    B=policy.BackdropSample
    # Recorded screen dimensions 1920x1080. Native perimeter palette
    # 80868620; interior spider palette 80208686, from F9/Log=1.
    # A small equivalent tessellation tests the resolution-independent
    # colour/coverage policy without storing a hardcoded level pattern.
    original=[
        (0,281.2,0,333,0x80868620),
        (0,281.2,300,700,0x80868620),
        (0,281.2,660,1080,0x80868620),
        (1638.8,1920,0,333,0x80868620),
        (1638.8,1920,300,700,0x80868620),
        (1638.8,1920,660,1080,0x80868620),
        (281.2,1638.8,0,150,0x80868620),
        (281.2,1638.8,900,1080,0x80868620),
        (800,1200,200,800,0x80208686),
    ]
    def classify(boxes,w=1920,h=1080):
        array=(B*len(boxes))(*(B(*v) for v in boxes))
        result=ct.c_uint(0)
        ok=policy.native_backdrop(array,len(boxes),w,h,ct.byref(result))
        return ok,result.value
    assert classify(original)==(1,0x80868620)
    # The exact source colour may change by game version/level.
    recolored=[(*v[:4],(0x80662299 if v[4]==0x80868620 else 0x80445588))
               for v in original]
    assert classify(recolored)==(1,0x80662299)
    # A mere left/right band does not prove an independent full veil.
    assert classify(original[:-2]+original[-1:])[0]==0
    # Distinct edge palette, alpha=0 or three colour families reject.
    mismatch=original.copy()
    mismatch[3]=(*mismatch[3][:4],0x80EEFFFF)
    assert classify(mismatch)[0]==0
    transparent=original.copy()
    transparent[0]=(*transparent[0][:4],0x00868620)
    assert classify(transparent)[0]==0
    tri_color=original.copy()
    tri_color[-1]=(*tri_color[-1][:4],0x80334455)
    tri_color.append((700,300,900,700,0x80556677))
    assert classify(tri_color)[0]==0
    assert classify(original,1440,1080)[0]==0
    assert classify(original,1920,2000)[0]==0


def test_native_exterior_preserves_all_160_source_triangles():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    fn=src.split("static int ui_comp_draw_native_exterior_gutters(",1)[1].split(
        "/* Flush before native EndScene",1)[0]
    for fragment in (
        "IDirect3DDevice7_ApplyStateBlock(device,sample->state_token)",
        "for(k=0u;k<4u;k++)v[k]=sample->source_v[0]",
        "for(side=0u;side<2u && SUCCEEDED(draw_hr);side++)",
        "left=side==0u ? 0.0f:(float)right_begin",
        "right=side==0u ? (float)margin:(float)w",
        "D3DPT_TRIANGLEFAN,0x144UL,v,4,sample->flags",
        "if(FAILED(draw_hr) || done!=2u)return 0;",
    ):
        assert fragment in fn
    assert "0x80868620" not in fn
    assert "ui_comp_draw_native_fullscreen_backdrop(" not in src
    flush=src.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert flush.index("ui_comp_draw_native_exterior_gutters(") < flush.index(
        "for(layer=SPIDER_UI_COMP_HUD;")
    assert "0,0,(unsigned)native_backdrop_argb" in flush
    assert "original_draw_primitive(device,c->type,c->fvf,c->v,4,c->flags)" in flush
    assert "ALL 160 NATIVE TRIANGLES REPLAYED" in flush
    assert "UI-COMPOSITOR-NATIVE-BACKDROP:" in flush


def test_non_pause_spider_geometry_and_hud_remain_native():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    flush=src.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    # The only opt-out in replay is specific to a currently-verified
    # PAUSE_DECOR backdrop tint. No HUD, selector, text or world layer.
    assert "for(pass=SPIDER_UI_COMP_REPLAY_BACKDROP;" in flush
    assert "pass<=SPIDER_UI_COMP_REPLAY_MENU;pass++)" in flush
    assert "SpiderUIComp_ReplayPassForDraw(" in flush
    assert "native_backdrop_drawn" in flush
    assert "for(layer=SPIDER_UI_COMP_HUD;" in flush
    assert "layer<=SPIDER_UI_COMP_PAUSE_MENU" in flush
    assert "maybe_correct_aim_reticle(" in src
    assert "ui_transition_native43_handle_draw(" in src


def test_exterior_only_viewport_restore():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    fn=src.split("static int ui_comp_draw_native_exterior_gutters(",1)[1].split(
        "/* Flush before native EndScene",1)[0]
    for fragment in (
        "IDirect3DDevice7_GetViewport(device,&previous)",
        "previous.dwX==margin",
        "previous.dwWidth==center_w",
        "physical.dwX=0u;physical.dwY=0u",
        "physical.dwWidth=w;physical.dwHeight=h",
        "original_set_viewport(device,&physical)",
        "restore_hr=original_set_viewport(device,&previous)",
        "if(FAILED(restore_hr))",
        "if(FAILED(draw_hr) || done!=2u)return 0;",
    ):
        assert fragment in fn
    assert fn.index("restore_hr=original_set_viewport(device,&previous)") > fn.index(
        "draw_hr=original_draw_primitive")
    assert "SetDisplayMode(" not in fn
    assert "hooked_set_viewport(" not in fn


def test_new_levels_are_recognized_without_new_depth_whitelist(policy):
    # The F9 screenshot scrn0099 is the first Pause encounter where
    # these source RHW values are NOT in the five fixed-depth variants.
    assert policy.dynamic_ribbon(0.00122,13.1453,0x7F3232A0)==1
    assert policy.dynamic_motif(0.00146,11.8990,0x80208686)==1
    assert policy.dynamic_motif(0.00146,11.8990,0x80868620)==1
    assert policy.dynamic_text(0.00098,14.6833)==1
    # Values from other observed levels and hypothetical camera
    # placements work with the same Z/source material conditions.
    assert policy.dynamic_ribbon(0.00122,6.8041,0x7F3232A0)==1
    assert policy.dynamic_ribbon(0.00122,18.25,0x6F4545B5)==1
    assert policy.dynamic_motif(0.00146,20.7,0x80868620)==1
    assert policy.dynamic_text(0.00122,9.9878)==1
    # The other guards remain strict. This predicate cannot itself
    # authorize a draw: the host also checks exact native caller,
    # untextured material, original geometry and recent ESC ribbon.
    assert policy.dynamic_ribbon(0.00146,13.1453,0x7F3232A0)==0
    assert policy.dynamic_ribbon(0.00122,13.1453,0x7FFF2020)==0
    assert policy.dynamic_ribbon(0.00122,13.1453,0x000000C0)==0
    assert policy.dynamic_ribbon(0.00122,-1.0,0x7F3232A0)==0
    assert policy.dynamic_ribbon(0.00122,129.0,0x7F3232A0)==0
    assert policy.dynamic_motif(0.00146,11.8990,0xFF208686)==0
    assert policy.dynamic_motif(0.00246,11.8990,0x80208686)==0
    assert policy.dynamic_text(0.00146,14.6833)==0
    assert policy.dynamic_text(0.00098,0.0)==0


def test_host_activates_compositor_before_generic_canvas_for_unknown_pause_depth():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    pane=src.split("static const void *maybe_correct_native_pause43(",1)[1].split(
        "/* Native ESC translucent geometric motif correction",1)[0]
    motif=src.split("static const void *maybe_correct_native_motif43(",1)[1].split(
        "/* Preserve native full-width scene tint",1)[0]
    assert "SpiderNativePause43_CompositorRibbon(z,rhw,v[0].diffuse)" in pane
    assert "SpiderNativePause43_CompositorText(z,rhw)" in pane
    assert "SpiderNativePause43_CompositorMotif(" in motif
    assert "native_pause43_visible_near_flip(" in motif
    assert "ui_compositor_trial_enabled" in pane
    assert "ui_compositor_trial_enabled" in motif
    # The same original COM material and FVF/caller signature are still
    # checked before anything can be classified as an ESC-owned draw.
    assert "rva!=0x00102C72UL" in pane
    assert "rva!=0x00102C72UL" in motif
    assert "IDirect3DDevice7_GetTexture(" in pane
    assert "IDirect3DDevice7_GetTexture(" in motif
    hook=src.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert hook.index("v=maybe_correct_native_motif43(")<hook.index(
        "candidate=maybe_correct_unified_ui_canvas(")
    assert hook.index("v=maybe_correct_native_pause43(")<hook.index(
        "candidate=maybe_correct_unified_ui_canvas(")
    assert hook.index("ui_comp_capture((IDirect3DDevice7*)self")<hook.index(
        "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)")


@pytest.mark.parametrize("background,spider",[
    (0x80868620,0x80208686),  # native source palette from Log=1
    (0x80662299,0x80445588),  # changed native palette, same policy
])
def test_source_verified_spider_has_its_own_pass_above_veil_and_hud(
        policy,background,spider):
    replay=policy.replay_pass
    # 0=backdrop, 1=HUD, 2=spider, 3=menu, -1=skip.
    # Successful fullscreen veil: skip original backdrop tiles,
    # preserve all inner spider pieces and foreground native menu.
    assert replay(3,background,1,1,background)==-1
    assert replay(1,spider,1,1,background)==1
    assert replay(3,spider,1,1,background)==2
    assert replay(4,0xFFFFFFFF,1,1,background)==3
    # Fullscreen draw failed but source identity is proven: original
    # backdrop quads remain, underneath the independently replayed art.
    assert replay(3,background,1,0,background)==0
    assert replay(3,spider,1,0,background)==2
    assert replay(4,0xFF000000,1,0,background)==3
    # Unknown shape/material => do not guess or discard its pieces.
    assert replay(3,background,0,0,background)==2
    assert replay(3,spider,0,0,background)==2
    assert replay(4,spider,0,0,background)==3
    assert replay(0,spider,1,1,background)==-1
    assert replay(1,spider,1,1,background)==1
    assert replay(2,spider,1,1,background)==-1


def test_compositor_keeps_spider_above_game_hud_and_below_title():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    flush=src.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert flush.index("ui_comp_draw_native_exterior_gutters(") < flush.index(
        "for(pass=SPIDER_UI_COMP_REPLAY_BACKDROP;")
    assert flush.index("for(pass=SPIDER_UI_COMP_REPLAY_BACKDROP;") < flush.index(
        "for(layer=SPIDER_UI_COMP_HUD;")
    assert "SPIDER_UI_COMP_REPLAY_HUD" in flush
    assert "SPIDER_UI_COMP_REPLAY_MOTIF" in flush
    assert "SPIDER_UI_COMP_REPLAY_MENU" in flush
    assert "owner!=(int)pass" in flush
    assert "D3DRENDERSTATE_ZENABLE,D3DZB_FALSE" in flush
    assert "IDirect3DDevice7_ApplyStateBlock(device,c->state_token)" in flush
    assert "IDirect3DDevice7_SetTexture(device,0,c->texture)" in flush
    assert "original_draw_primitive(device,c->type,c->fvf,c->v,4,c->flags)" in flush
    assert "UI-COMPOSITOR-ORDER:" in flush
    # The game owns the 4:3 geometry and alpha of its spider.
    assert "backdrop_v[k].x" not in flush
    assert "SPIDERMAN2K1FIX_UI_COMPOSITOR_NATIVE_BACKDROP" in src
    # The foreground menubar and PAUSED title retain their native
    # PAUSE_MENU queue, above every PAUSE_DECOR primitive.
    assert "SPIDER_UI_COMP_PAUSE_MENU" in flush
    assert "exterior_quads,paint_spider,paint_hud,paint_menu" in flush


def test_late_hud_draws_are_detected_without_touching_the_original_draw_path():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    assert "ui_comp_last_overlay_flip=-1" in src
    assert "ui_comp_late_draw_samples=0" in src
    flush=src.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert "InterlockedExchange(&ui_comp_last_overlay_flip" in flush
    assert "if(paint_spider && paint_menu && all_ok)" in flush
    hook=src.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert "UI-COMPOSITOR-LATE-DRAW:" in hook
    assert "sample<=16" in hook
    assert "late_rva==0x00102C72UL" in hook
    assert "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)" in hook
    # This path must NEVER issue an extra draw or skip native HUD.
    diag=hook.split("/* Check for a separate native HUD pass",1)[1].split(
        "/* Deferred ESC composition is active",1)[0]
    assert "original_draw_primitive(" not in diag
    assert "ui_comp_capture(" not in diag
    assert "log_line(msg)" in diag


def test_native_pause_hud_is_replayed_once_after_veil_before_spider():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    policy=(ROOT/"ui_compositor_policy.h").read_text()
    assert "SPIDER_UI_COMP_REPLAY_HUD = 1" in policy
    assert "SPIDER_UI_COMP_REPLAY_MOTIF = 2" in policy
    assert "SPIDER_UI_COMP_REPLAY_MENU = 3" in policy
    assert "native_layer == SPIDER_UI_COMP_HUD" in policy
    assert "SPIDER_UI_COMP_MAX_COMMANDS 384u" in policy
    assert "UI-COMPOSITOR-HUD-ORDER: ON" in src
    assert "SPIDERMAN2K1FIX_UI_COMPOSITOR_HUD_OVER_VEIL" in src
    hook=src.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert "verified_hud_canvas=1" in hook
    assert "native_pause43_visible_near_flip(" in hook
    assert "native_esc_primitive && v!=verts" in hook
    assert "SPIDER_UI_COMP_HUD" in hook
    assert "SPIDER_UI_COMP_HUD))" in hook
    assert hook.index("verified_hud_canvas &&") < hook.index(
        "native_esc_primitive && v!=verts")
    # The successful capture returns D3D_OK rather than drawing HUD early,
    # avoiding unwanted dimming AND double alpha blending.
    before=hook.split("if(!native_esc_primitive && verified_hud_canvas &&",1)[1].split(
        "else if(native_esc_primitive && v!=verts",1)[0]
    assert "hr=D3D_OK;" in before
    assert "original_draw_primitive" not in before
    flush=src.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert "for(layer=SPIDER_UI_COMP_HUD;" in flush
    assert "SPIDER_UI_COMP_REPLAY_HUD" in flush
    assert "paint_hud++" in flush
    assert flush.index("ui_comp_draw_native_exterior_gutters(") < flush.index(
        "for(pass=SPIDER_UI_COMP_REPLAY_BACKDROP;")
    assert "UI-COMPOSITOR-ORDER:" in flush
    assert "native-decor=%u hud=%u menu=%u" in flush
    # Preserve motif geometry and original HUD texture/material per draw.
    assert "original_draw_primitive(device,c->type,c->fvf,c->v,4,c->flags)" in flush
    assert "IDirect3DDevice7_ApplyStateBlock(device,c->state_token)" in flush
    assert "IDirect3DDevice7_SetTexture(device,0,c->texture)" in flush


def test_hud_only_scene_is_not_misidentified_as_pause():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    capture=src.split("static int ui_comp_capture(",1)[1].split(
        "static HRESULT WINAPI ui_compositor_scene_start(",1)[0]
    assert "else if(layer==SPIDER_UI_COMP_PAUSE_MENU) ui_comp_ribbon=1;" in capture
    assert "if(layer==SPIDER_UI_COMP_PAUSE_DECOR)" in capture
    assert "ui_comp_ribbon=1;" in capture
    # Older code set ribbon on ANY non-decor command. Capturing the
    # gameplay HUD must not accidentally fabricate a native Pause state.
    assert "} else ui_comp_ribbon=1;" not in capture

def test_exterior_only_does_not_reclassify_or_drop_native_decor():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    flush=src.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert "native_backdrop_verified=ui_comp_native_backdrop_source(" in flush
    assert "ui_comp_draw_native_exterior_gutters(" in flush
    assert "0,0,(unsigned)native_backdrop_argb" in flush
    assert "c->source_v[0].diffuse==native_backdrop_argb" not in flush
    assert "original_draw_primitive(device,c->type,c->fvf,c->v,4,c->flags)" in flush
    assert "UI-COMPOSITOR-ORDER:" in flush

def test_exterior_gutters_support_odd_physical_screen_widths():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    fn=src.split("static int ui_comp_draw_native_exterior_gutters(",1)[1].split(
        "/* Flush before native EndScene",1)[0]
    assert "right_begin=margin+center_w" in fn
    assert "if(margin==0u || right_begin>=w)return 0;" in fn
    assert "float left=side==0u ? 0.0f:(float)right_begin;" in fn
    assert "float right=side==0u ? (float)margin:(float)w;" in fn
    assert "(w-center_w)!=2u*margin" not in fn


def test_retired_veil_trials_cannot_change_validated_palette():
    policy=(ROOT/"ui_compositor_policy.h").read_text()
    src=(ROOT/"spiderman2k1fix.c").read_text()
    for retired in (
        "SpiderUIComp_Tint(",
        "SpiderUIComp_GutterWidth(",
        "SpiderUIComp_EdgeTileCoversGutter(",
        "SpiderUIComp_TriangleEdgeSpan(",
        "SpiderUIComp_BackdropMapX(",
        "ui_comp_draw_screen_tint(",
        "ui_comp_draw_native_gutters(",
        "SPIDERMAN2K1FIX_UI_COMPOSITOR_TINT",
        "SPIDERMAN2K1FIX_UI_COMPOSITOR_NATIVE_GUTTERS",
    ):
        assert retired not in policy
        assert retired not in src
    assert "ui_comp_draw_native_exterior_gutters(" in src
    assert "SPIDERMAN2K1FIX_UI_COMPOSITOR_NATIVE_BACKDROP" in src
    assert "0,0,(unsigned)native_backdrop_argb" in src


def test_unverified_hud_reorder_is_off_by_default_and_opt_in_only():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    assert "static volatile LONG ui_compositor_hud_over_veil_enabled=0;" in src
    assert 'SPIDERMAN2K1FIX_UI_COMPOSITOR_HUD_OVER_VEIL' in src
    assert "hud_over_veil[0]=='1'" in src
    assert "hud_over_veil[0]=='0'" not in src
    assert "HUD-ORDER: OFF (validated native HUD path, default)" in src
    assert "UI-COMPOSITOR-NATIVE-BACKDROP: source-verified OUTER GUTTERS enabled" in src
