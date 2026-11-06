"""Native scene-iris full-screen coverage without stretching 4:3 art.

Test the actual C vertex classifier and polygon clipper. The host has no
Wine/D3D7 renderer, so these tests deliberately do not claim hardware
visual parity or correct game-owned mask detection in every scene.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]


class Vertex(ct.Structure):
    _fields_=[
        ("x",ct.c_float),("y",ct.c_float),("z",ct.c_float),
        ("rhw",ct.c_float),("diffuse",ct.c_uint32),
        ("u",ct.c_float),("v",ct.c_float),
    ]


def quad(l,t,r,b,*,z=0.0,rhw=27.6,diffuse=0x00000000):
    data=(Vertex*4)()
    for p,(x,y,u,v) in enumerate([
        (l,t,0.,0.),(r,t,1.,0.),(r,b,1.,1.),(l,b,0.,1.)]):
        a=data[p]
        a.x,a.y,a.z,a.rhw=x,y,z,rhw
        a.diffuse=diffuse
        a.u,a.v=u,v
    return data


@pytest.fixture(scope="module")
def native(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:pytest.skip("C compiler required for native geometry tests")
    folder=tmp_path_factory.mktemp("native_iris")
    p=folder/"test.c"
    libfile=folder/"test.so"
    p.write_text(
        '#include "ui_transition_gutters.h"\n'
        'int family(const SpiderMenuVertex*v,unsigned n,unsigned prim,'
        'unsigned fvf,unsigned rva,unsigned w,unsigned h){'
        'return SpiderTransition_IsNativeBlackMask(v,n,prim,fvf,rva,w,h);'
        '}\n'
        'unsigned clip(const SpiderMenuVertex*s,SpiderMenuVertex*d,'
        'unsigned side,unsigned w,unsigned h){'
        'return SpiderTransition_ClipToGutter(s,d,side,w,h);'
        '}\n'
        'unsigned extension(const SpiderMenuVertex*s,SpiderMenuVertex*d,'
        'unsigned side,unsigned w,unsigned h){'
        'return SpiderTransition_ExtendBlackEdge(s,d,side,w,h);'
        '}\n'
        'void inner(const SpiderMenuVertex*s,SpiderMenuVertex*d,'
        'unsigned w,unsigned h){'
        'SpiderTransition_MapInner43(s,d,w,h);'
        '}\n'
    )
    subprocess.run(
        [cc,"-std=c11","-Wall","-Wextra","-Werror",
         "-fPIC","-shared","-I",str(ROOT),str(p),"-o",str(libfile)],
        check=True,capture_output=True,text=True,
    )
    so=ct.CDLL(str(libfile))
    so.family.argtypes=[ct.POINTER(Vertex),ct.c_uint,ct.c_uint,
                        ct.c_uint,ct.c_uint,ct.c_uint,ct.c_uint]
    so.family.restype=ct.c_int
    so.clip.argtypes=[ct.POINTER(Vertex),ct.POINTER(Vertex),
                      ct.c_uint,ct.c_uint,ct.c_uint]
    so.clip.restype=ct.c_uint
    so.extension.argtypes=[ct.POINTER(Vertex),ct.POINTER(Vertex),
                           ct.c_uint,ct.c_uint,ct.c_uint]
    so.extension.restype=ct.c_uint
    so.inner.argtypes=[ct.POINTER(Vertex),ct.POINTER(Vertex),
                       ct.c_uint,ct.c_uint]
    so.inner.restype=None
    return so


def clipped(native,src,side,resolution=(1920,1080)):
    out=(Vertex*8)()
    n=native.clip(src,out,side,*resolution)
    assert 0<=n<=8
    return list(out)[:n]


@pytest.mark.parametrize("resolution",[
    (1920,1080),(1920,1200),(2560,1080),(3440,1440)
])
def test_physical_mask_exterior_is_exact_and_aspect_independent(native,resolution):
    w,h=resolution
    g=(w-h*4/3)/2
    source=quad(0,0,w,h)
    assert native.family(source,4,6,0x144,0x00102C72,w,h)==1
    left=clipped(native,source,0,resolution)
    right=clipped(native,source,1,resolution)
    assert len(left)>=3 and len(right)>=3
    for v in left:
        assert -.01<=v.x<=g+.01
        assert 0<=v.y<=h
        assert v.diffuse==0x00000000
        assert v.z==0
        assert v.rhw==pytest.approx(27.6,abs=.001)
    for v in right:
        assert w-g-.01<=v.x<=w+.01
        assert 0<=v.y<=h
        assert v.diffuse==0x00000000
    assert min(v.x for v in left)==pytest.approx(0,abs=.01)
    assert max(v.x for v in left)==pytest.approx(g,abs=.01)
    assert min(v.x for v in right)==pytest.approx(w-g,abs=.01)
    assert max(v.x for v in right)==pytest.approx(w,abs=.01)


def test_diagonal_spider_edges_are_clipped_not_stretched(native):
    v=(Vertex*4)()
    for i,(x,y) in enumerate([(0,0),(360,160),(600,860),(0,1080)]):
        v[i].x,v[i].y,v[i].z,v[i].rhw=x,y,0,27.6
        v[i].diffuse=0x00000000
        v[i].u,v[i].v=(i%2),float(i>=2)
    left=clipped(native,v,0)
    right=clipped(native,v,1)
    assert len(left)>=4
    assert right==[]
    assert all(0<=p.x<=240 for p in left)
    # Boundary crossing interpolates *both* y and texture coordinates:
    # no constant-width enlargement of the jagged silhouette.
    boundary=[p for p in left if abs(p.x-240)<.01]
    assert len(boundary)==2
    assert boundary[0].y!=boundary[1].y
    assert all(0<=p.u<=1 for p in boundary)


def test_no_fragment_for_iris_shape_entirely_inside_canvas(native):
    source=quad(620,280,1320,820)
    assert clipped(native,source,0)==[]
    assert clipped(native,source,1)==[]


@pytest.mark.parametrize("bad",[
    {"z":.00098,"rhw":12.1569,"diffuse":0xFF000000},
    {"z":0.,"rhw":27.6,"diffuse":0x80000000},
    {"z":0.,"rhw":27.6,"diffuse":0xFFFFFFFF},
    {"z":.00146,"rhw":9.499,"diffuse":0xFF000000},
])
def test_game_hud_pause_background_and_translucent_layers_are_not_masks(native,bad):
    src=quad(0,100,500,600,**bad)
    assert native.family(src,4,6,0x144,0x00102C72,1920,1080)==0


def test_wrong_draw_caller_or_original_4_3_does_not_activate(native):
    src=quad(0,0,640,480)
    assert native.family(src,4,6,0x144,0x00102C72,640,480)==0
    assert clipped(native,src,0,(640,480))==[]
    assert native.family(src,4,6,0x144,0x00102C71,1920,1080)==0
    assert native.family(src,4,4,0x144,0x00102C72,1920,1080)==0
    assert native.family(src,4,6,0x112,0x00102C72,1920,1080)==0


def test_original_inner_silhouette_preserves_four_by_three_proportions(native):
    w,h=1920,1080
    source=quad(0,50,1920,950)
    dst=(Vertex*4)()
    native.inner(source,dst,w,h)
    assert dst[0].x==pytest.approx(240,abs=.02)
    assert dst[1].x==pytest.approx(1680,abs=.02)
    assert dst[2].x-dst[3].x==pytest.approx(1440,abs=.03)
    for a,b in zip(source,dst):
        assert bytes(a)[4:]==bytes(b)[4:]


def test_runtime_rolls_back_without_extra_environment_flag():
    source=(ROOT/"spiderman2k1fix.c").read_text()
    h=(ROOT/"ui_transition_gutters.h").read_text()
    assert 'SPIDERMAN2K1FIX_UI_TRANSITION_TEST' in source
    assert "static volatile LONG ui_transition_trial_enabled=0;" in source
    assert "SpiderTransition_IsNativeBlackMask(" in source
    assert "SpiderTransition_ExtendBlackEdge(" in source
    assert "SpiderTransition_MapInner43(" in source
    assert "UI-TRANSITION-GUTTERS: OFF" in source
    assert "IDirect3DDevice7_GetTexture(" in source
    assert "IDirectDrawSurface7_Release(texture);" in source
    assert "SPIDER_TRANSITION_MAX_VERTS 8u" in h
    assert "for(side=0u;side<2u;side++)" in source
    # Additional gutter draw uses original D3D state; no altered source
    # geometry and no extra artificial black fullscreen rectangle.
    start=source.index("static void maybe_restore_native_transition_gutters(")
    end=source.index("static HRESULT WINAPI hooked_draw_primitive(",start)
    fn=source[start:end]
    assert "original_draw_primitive(device,type,fvf,(LPVOID)extended," in fn
    assert "SpiderTransition_ClipToGutter(" not in fn
    assert "SpiderTransition_ExtendBlackEdge(" in fn
    assert "D3DRENDERSTATE_Z" not in fn
    assert "memcpy(" not in fn
    assert "SetRenderState" not in fn



def test_rejected_source_probe_is_read_only_and_bounded():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    marker="static void ui_transition_probe_rejected_source("
    body=src.split(marker,1)[1].split(
        "static void maybe_restore_native_transition_gutters(",1)[0]
    assert "UI-TRANSITION-PROBE:" in body
    assert "ui_transition_trial_enabled,0,0)!=1" in body
    assert "if(hit>8) return;" in body
    assert "SpiderTransition_IsNativeBlackMask(" in body
    assert "IDirect3DDevice7_GetTexture(" in body
    assert "IDirectDrawSurface7_Release(texture)" in body
    assert "original_draw_primitive(" not in body
    assert "SetRenderState" not in body
    assert "SetTexture" not in body
    assert "memcpy(" not in body
    assert src.index("ui_transition_probe_rejected_source(self,type,fvf,verts,count,") < src.index(
        "maybe_restore_native_transition_gutters(self,type,fvf,verts,count,")


def test_native_spider_draws_zero_alpha_rgb_black_from_real_hardware_trace(native):
    # Wine log SpiderMan2k1Fix(20261106-011326).log:
    # source x=(0..930) and (990..1920), alpha-byte=0x00, RGB=0.
    # Earlier classifier erroneously required FF000000.
    for l,r in [(0,930),(990,1920),(960,971.2)]:
        source=quad(l,0,r,1080,diffuse=0x00000000)
        assert native.family(source,4,6,0x144,0x00102C72,1920,1080)==1
    # FF000000 remains supported if another legitimate transition
    # rendering mode supplies opaque black.
    source=quad(0,0,930,1080,diffuse=0xFF000000)
    assert native.family(source,4,6,0x144,0x00102C72,1920,1080)==1


def test_native_spider_mask_requires_uniform_rgba_and_never_accepts_other_colors(native):
    # Do not confuse this with per-vertex alpha/color gradients.
    source=quad(0,0,930,1080,diffuse=0x00000000)
    source[1].diffuse=0xFF000000
    assert native.family(source,4,6,0x144,0x00102C72,1920,1080)==0
    for color in (0xFFFFFFFF,0x80000000,0x01000000,0x00000001,0xFF010101):
        source=quad(0,0,930,1080,diffuse=color)
        assert native.family(source,4,6,0x144,0x00102C72,1920,1080)==0


def test_zero_alpha_mask_replay_is_conditioned_on_disabled_native_blending():
    source=(ROOT/"spiderman2k1fix.c").read_text()
    start=source.index("static void maybe_restore_native_transition_gutters(")
    end=source.index("static HRESULT WINAPI hooked_draw_primitive(",start)
    code=source[start:end]
    assert "D3DRENDERSTATE_ALPHABLENDENABLE,&alpha_blend" in code
    assert "D3DRENDERSTATE_ALPHATESTENABLE,&alpha_test" in code
    assert "if(FAILED(hb) || FAILED(ht) || alpha_blend || alpha_test)" in code
    assert "UI-TRANSITION-GUTTERS: SOURCE ACCEPTED=" in code
    assert "UI-TRANSITION-GUTTERS: zero-alpha native black rejected" in code
    assert code.index("if(FAILED(hb) || FAILED(ht) || alpha_blend || alpha_test)") < code.index(
        "for(side=0u;side<2u;side++)")
    assert "SetRenderState" not in code
    assert "SetTexture" not in code



def extended(native,source,side,resolution=(1920,1080)):
    out=(Vertex*4)()
    n=native.extension(source,out,side,*resolution)
    assert n in (0,4)
    return list(out)[:n]


@pytest.mark.parametrize("res",[
    (1920,1080),(1920,1200),(2560,1080),(3440,1440)
])
def test_expanding_spider_outside_uses_only_center_canvas_edge(native,res):
    w,h=res
    g=(w-h*4/3)/2
    left=extended(native,quad(0,10,w*.49,h-10),0,res)
    right=extended(native,quad(w*.51,40,w,h-40),1,res)
    assert len(left)==4
    assert len(right)==4
    assert {round(v.x,3) for v in left}=={0,round(g,3)}
    assert {round(v.x,3) for v in right}=={round(w-g,3),w}
    assert {v.y for v in left}=={10,h-10}
    assert {v.y for v in right}=={40,h-40}
    assert all(v.diffuse==0x00000000 and v.rhw==pytest.approx(27.6) for v in left+right)


def test_iris_end_does_not_draw_disconnected_native_gutter_triangles(native):
    # Core V5 end-of-iris defect: native black triangles at x=150..260
    # may lie inside the ORIGINAL physical gutter, but map into the
    # CENTER (>240px) under the proportional 4:3 X correction.
    # Clipping native x=150..260 would leave a detached black triangle.
    # We must only paint gutters where the centered mask touches
    # its ACTUAL edge (source x=0 / source x=W).
    for source in [
        quad(50,100,300,700),
        quad(150,400,270,870),
        quad(450,20,940,1000),
    ]:
        assert clipped(native,source,0)
        assert extended(native,source,0)==[]
    for source in [
        quad(1650,100,1800,800),
        quad(1730,50,1850,700),
    ]:
        assert clipped(native,source,1)
        assert extended(native,source,1)==[]


def test_iris_edge_band_reveals_world_with_proportional_opening(native):
    # When the spider reaches only the middle 2/3 of the screen edge,
    # the gutter stays black ONLY over the source's actual border span;
    # the uncovered Y rows show the world just like the adjacent
    # boundary of the centered-4:3 scene. No constant fullscreen veil.
    src=quad(0,240,600,840)
    band=extended(native,src,0)
    assert len(band)==4
    assert set(v.y for v in band)=={240,840}
    assert set(v.x for v in band)=={0,240}


def test_only_original_left_and_right_screen_edges_may_extend(native):
    assert extended(native,quad(0,0,1920,1080),0)
    assert extended(native,quad(0,0,1920,1080),1)
    assert extended(native,quad(0,0,700,1080),1)==[]
    assert extended(native,quad(1150,0,1920,1080),0)==[]
    assert extended(native,quad(0,0,600,1080),0)
    assert extended(native,quad(1300,0,1920,1080),1)


def test_diagonal_mask_contour_keeps_y_span_at_source_screen_edge(native):
    src=(Vertex*4)()
    for i,(x,y) in enumerate([(0,100),(500,200),(500,950),(0,900)]):
        src[i].x,src[i].y,src[i].z,src[i].rhw=x,y,0,27.6
        src[i].diffuse=0
    band=extended(native,src,0)
    assert len(band)==4
    assert {p.y for p in band}=={100,900}
    assert set(p.x for p in band)=={0,240}
    assert extended(native,src,1)==[]


def test_cross_screen_edge_interpolated_and_concavity_fails_closed(native):
    src=(Vertex*4)()
    for i,(x,y) in enumerate([(-10,100),(250,150),(250,850),(-10,900)]):
        src[i].x,src[i].y,src[i].z,src[i].rhw=x,y,0,27.6
        src[i].diffuse=0
    band=extended(native,src,0)
    assert len(band)==4
    assert min(p.y for p in band)==pytest.approx(101.923,abs=.02)
    assert max(p.y for p in band)==pytest.approx(898.077,abs=.02)
    # Concave quadrilateral would have disconnected spans, must not
    # paint a rectangular envelope into the margin.
    for i,(x,y) in enumerate([(0,100),(400,100),(100,400),(0,850)]):
        src[i].x,src[i].y=x,y
    assert extended(native,src,0)==[]


def test_runtime_uses_edge_extension_not_old_source_screen_clip():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    section=src.split("static void maybe_restore_native_transition_gutters(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_primitive(",1)[0]
    assert "SpiderTransition_ExtendBlackEdge(" in section
    assert "SpiderTransition_ClipToGutter(" not in section
    assert "UI-TRANSITION-GUTTERS: edge-extended=" in section
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_TEST" in src
