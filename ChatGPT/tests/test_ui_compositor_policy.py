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
        'float gutter(unsigned w,unsigned h){return SpiderUIComp_GutterWidth(w,h);}\n'
        'int coverage(float h,const SpiderUICompYSpan *sp,unsigned n){'
        'return SpiderUIComp_CoversFullHeight(sp,n,h);}\n'
        'int edge(float l,float r,float w,float g,int side){'
        'return SpiderUIComp_EdgeTileCoversGutter(l,r,w,g,side);}\n'
        'int segment(float x0,float y0,float x1,float y1,'
        'float x2,float y2,float edge,float h,SpiderUICompYSpan *out){'
        'return SpiderUIComp_TriangleEdgeSpan(x0,y0,x1,y1,'
        'x2,y2,edge,h,out);}\n'
        'int native_backdrop(const SpiderUICompBackdropSample *s,'
        'unsigned n,float w,float h,unsigned *color){'
        'return SpiderUIComp_IdentifyNativeBackdrop(s,n,w,h,color);}\n'
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
    lib.gutter.argtypes=[ct.c_uint,ct.c_uint]
    lib.gutter.restype=ct.c_float
    class YSpan(ct.Structure):
        _fields_=[("top",ct.c_float),("bottom",ct.c_float)]
    lib.YSpan=YSpan
    lib.coverage.argtypes=[ct.c_float,ct.POINTER(YSpan),ct.c_uint]
    lib.coverage.restype=ct.c_int
    lib.edge.argtypes=[ct.c_float]*4+[ct.c_int]
    lib.edge.restype=ct.c_int
    lib.segment.argtypes=[ct.c_float]*8+[ct.POINTER(YSpan)]
    lib.segment.restype=ct.c_int
    class BackdropSample(ct.Structure):
        _fields_=[("left",ct.c_float),("right",ct.c_float),
                  ("top",ct.c_float),("bottom",ct.c_float),
                  ("color",ct.c_uint)]
    lib.BackdropSample=BackdropSample
    lib.native_backdrop.argtypes=[ct.POINTER(BackdropSample),ct.c_uint,
                                 ct.c_float,ct.c_float,ct.POINTER(ct.c_uint)]
    lib.native_backdrop.restype=ct.c_int
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



@pytest.mark.parametrize("res,expected", [
    ((1920,1080),240.0), ((1920,1200),160.0),
    ((2560,1080),560.0), ((3440,1440),760.0),
    ((640,480),0.0), ((1024,768),0.0),
])
def test_native_gutters_are_only_physical_exterior(policy,res,expected):
    assert policy.gutter(*res)==pytest.approx(expected,abs=.03)


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


def test_compositor_extends_each_original_source_edge_instead_of_flat_bands():
    source=(ROOT/"spiderman2k1fix.c").read_text()
    fn=source.split("static int ui_comp_draw_native_gutters(",1)[1].split(
        "/* Flush before native EndScene",1)[0]
    assert "c->source_v" in fn
    assert "c->layer!=SPIDER_UI_COMP_PAUSE_DECOR" in fn
    assert "c->texture" in fn
    assert "v[k].diffuse!=v[0].diffuse" in fn
    assert "SpiderUIComp_CoversFullHeight(left_spans,left_count,h)" in fn
    assert "SpiderUIComp_CoversFullHeight(right_spans,right_count,h)" in fn
    assert "SpiderUIComp_TriangleEdgeSpan(" in fn
    assert "IDirect3DDevice7_ApplyStateBlock(device,c->state_token)" in fn
    assert "out[k]=*v0" in fn
    assert "out[0].x=xl;out[0].y=span.top;" in fn
    assert "out[2].x=xr;out[2].y=span.bottom;" in fn
    assert "original_draw_primitive(" in fn
    assert "vertices[0].x=xl;vertices[0].y=0.0f;" not in fn
    assert "palette!=(unsigned)v[0].diffuse" not in fn
    # Source triangles are replayed ONLY along the outer strips.
    assert "out[0].x=xl" in fn and "out[3].x=xl" in fn
    assert "c->v[k].x=" not in fn
    assert "c->source_v[k].x=" not in fn
    assert "0x80868620" not in fn
    assert "UI-COMPOSITOR-NATIVE-GUTTERS:" in source
    assert "source-edge-strips=%u" in source
    assert "SPIDERMAN2K1FIX_UI_COMPOSITOR_NATIVE_GUTTERS" in source
    assert "gutters[0]=='0'" in source
    assert "static volatile LONG ui_compositor_native_gutters_enabled=1;" in source
    assert "if(!needs_tint && ui_comp_motif &&" in source
    flush=source.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert flush.index("ui_comp_draw_native_gutters(device,&source_strips)") < (
        flush.index("for(layer=SPIDER_UI_COMP_PAUSE_DECOR;"))


def test_original_source_edge_triangle_preserves_vertical_segments(policy):
    Y=policy.YSpan

    def sample(vertices,edge,h=1080):
        v=list(vertices)
        out=Y()
        result=policy.segment(*v,edge,h,ct.byref(out))
        return result,out

    # One of the game's native left border triangles with a full
    # 0..234px source edge. Its exterior must not become 0..1080.
    found,span=sample((0,0,281.2,0,0,234),0)
    assert found==1
    assert span.top==pytest.approx(0)
    assert span.bottom==pytest.approx(234)

    # Complementary triangle only touches the edge at one point;
    # it must not paint the 240px exterior.
    found,_=sample((281.2,0,281.2,234,0,234),0)
    assert found==0

    # Right monitor edge, a different source segment, same algorithm.
    found,span=sample((1920,600,1638.8,810,1920,810),1920)
    assert found==1
    assert span.top==pytest.approx(600)
    assert span.bottom==pytest.approx(810)

    # Geometry between original 4:3/gutter boundaries is not an edge.
    found,_=sample((240,0,480,0,240,234),0)
    assert found==0

    # A degenerate triangle cannot establish an exterior backdrop.
    found,_=sample((0,0,0,0,0,234),0)
    assert found==0

    # Partial screen coverage cannot pass the full-height validator.
    v=(Y*2)(Y(0,320),Y(450,1080))
    assert policy.coverage(1080,v,2)==0



def test_full_physical_gutter_coverage_uses_source_geometry_not_artwork(policy):
    # Native Windows PC draw captured with the approved 1920x1080 Wine
    # screenshot: edge rectangles on both sides reach strictly past
    # the 240px complement of the centered 4:3 interface.
    assert policy.edge(0.0,281.2,1920,240,0)==1
    assert policy.edge(1638.8,1920.0,1920,240,1)==1
    # Critical false-positive previously accepted: a narrow opaque
    # tile touching the extreme display edge but stopping before the
    # entire 4:3 exterior must NOT be expanded into a whole strip.
    assert policy.edge(0.0,120.0,1920,240,0)==0
    assert policy.edge(1800.0,1920.0,1920,240,1)==0
    # A sprite not attached to the monitor edge cannot become a gutter.
    assert policy.edge(50.0,350.0,1920,240,0)==0
    assert policy.edge(1570.0,1870.0,1920,240,1)==0


@pytest.mark.parametrize("resolution",[
    (1920,1080),(1920,1200),(2560,1080),(3440,1440)
])
def test_edge_guard_adapts_to_all_widescreen_resolutions(policy,resolution):
    width,height=resolution
    gutter=policy.gutter(width,height)
    assert gutter>0
    assert policy.edge(0,gutter+2,width,gutter,0)==1
    assert policy.edge(width-gutter-2,width,width,gutter,1)==1
    assert policy.edge(0,gutter*.5,width,gutter,0)==0
    assert policy.edge(width-gutter*.5,width,width,gutter,1)==0


def test_runtime_refuses_partial_source_tile_before_material_and_y_checks():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    start=src.index("static int ui_comp_draw_native_gutters(")
    end=src.index("/* Flush before native EndScene",start)
    code=src[start:end]
    assert code.count("SpiderUIComp_EdgeTileCoversGutter(")==3
    assert code.index("SpiderUIComp_EdgeTileCoversGutter(") < code.index(
        "!c->state_token || c->texture")
    assert "if(!edge_left && !edge_right)continue;" in code
    assert "SpiderUIComp_CoversFullHeight(left_spans,left_count,h)" in code
    assert "SpiderUIComp_CoversFullHeight(right_spans,right_count,h)" in code
    # Render passes only after BOTH full-height checks.
    assert code.index("SpiderUIComp_CoversFullHeight(right_spans") < (
        code.index("IDirect3DDevice7_ApplyStateBlock(device,c->state_token)"))
    assert "return drawn!=0u;" in code


def test_native_backdrop_is_derived_from_four_edge_source_not_spider(policy):
    B=policy.BackdropSample
    # Recorded screen dimensions 1920x1080. Native perimeter palette
    # 80868620; interior spider palette 80208686, from F9/Log=1.
    # A small equivalent tessellation tests the resolution-independent
    # colour/coverage policy without storing a hardcoded level pattern.
    original=[
        (0,0,281.2,333,0x80868620),
        (0,300,281.2,700,0x80868620),
        (0,660,281.2,1080,0x80868620),
        (1638.8,0,1920,333,0x80868620),
        (1638.8,300,1920,700,0x80868620),
        (1638.8,660,1920,1080,0x80868620),
        (281.2,0,1638.8,150,0x80868620),
        (281.2,900,1638.8,1080,0x80868620),
        (800,200,1200,800,0x80208686),
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


def test_native_veil_is_one_original_alpha_blended_fullscreen_layer():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    policy_src=(ROOT/"ui_compositor_policy.h").read_text()
    assert "SpiderUIComp_IdentifyNativeBackdrop(" in policy_src
    assert "ui_comp_native_backdrop_source(" in src
    fn=src.split("static int ui_comp_draw_native_backdrop(",1)[1].split(
        "/* Flush before native EndScene",1)[0]
    assert "IDirect3DDevice7_ApplyStateBlock(device,sample->state_token)" in fn
    assert "corners[k]=sample->source_v[0]" in fn
    assert "corners[0].x=0.0f;corners[0].y=0.0f" in fn
    assert "corners[2].x=w;corners[2].y=h" in fn
    assert "original_draw_primitive(" in fn
    assert "0x80868620" not in fn  # never hardcode the logged palette
    flush=src.split("static void ui_comp_flush(",1)[1].split(
        "static int ui_comp_capture(",1)[0]
    assert flush.index("ui_comp_draw_native_backdrop(") < flush.index(
        "for(layer=SPIDER_UI_COMP_PAUSE_DECOR;")
    assert "native_backdrop_drawn && layer==SPIDER_UI_COMP_PAUSE_DECOR" in flush
    assert "c->source_v[0].diffuse==native_backdrop_argb" in flush
    assert "!native_backdrop_drawn && ui_comp_motif" in flush
    assert "UI-COMPOSITOR-NATIVE-BACKDROP:" in flush

