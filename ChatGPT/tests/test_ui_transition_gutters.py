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


def quad(l,t,r,b,*,z=0.0,rhw=27.6,diffuse=0xFF000000):
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
        assert v.diffuse==0xFF000000
        assert v.z==0
        assert v.rhw==pytest.approx(27.6,abs=.001)
    for v in right:
        assert w-g-.01<=v.x<=w+.01
        assert 0<=v.y<=h
        assert v.diffuse==0xFF000000
    assert min(v.x for v in left)==pytest.approx(0,abs=.01)
    assert max(v.x for v in left)==pytest.approx(g,abs=.01)
    assert min(v.x for v in right)==pytest.approx(w-g,abs=.01)
    assert max(v.x for v in right)==pytest.approx(w,abs=.01)


def test_diagonal_spider_edges_are_clipped_not_stretched(native):
    v=(Vertex*4)()
    for i,(x,y) in enumerate([(0,0),(360,160),(600,860),(0,1080)]):
        v[i].x,v[i].y,v[i].z,v[i].rhw=x,y,0,27.6
        v[i].diffuse=0xFF000000
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
    assert "SpiderTransition_ClipToGutter(" in source
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
    assert "original_draw_primitive(device,type,fvf,(LPVOID)clipped," in fn
    assert "D3DRENDERSTATE_Z" not in fn
    assert "memcpy(" not in fn
    assert "SetRenderState" not in fn
