"""V25: physical native silhouette extrema, no fixed-resolution or clock.

A previous side-mask experiment (V24) used an ease curve and two full-
height black rectangles which could cover visible tips and leave a 1px
seam at the static 4:3 boundary. V25 measures the ACTUAL onscreen X
extrema of V19-mapped hooked source triangles (excluding only the four
large outer mattes), clips them to the physical Y viewport and builds
two independently sized opaque black side curtains.

All tests compile and execute the actual portable C routines. Windows
x86 PE/D3D7 compile and in-game Wine visual acceptance are separate.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]
FIXTURE=ROOT/"ChatGPT/tests/fixtures/spider_iris_native_20261106.csv"


class Vertex(ct.Structure):
    _fields_=[
        ("x",ct.c_float),("y",ct.c_float),
        ("z",ct.c_float),("rhw",ct.c_float),
        ("diffuse",ct.c_uint32),("u",ct.c_float),("v",ct.c_float),
    ]


class Frame(ct.Structure):
    _fields_=[
        ("min_x",ct.c_float),("max_x",ct.c_float),
        ("eligible",ct.c_uint),("excluded_matte",ct.c_uint),
        ("offscreen",ct.c_uint),("observed",ct.c_uint),
    ]


def original_quads():
    rows=[]
    for l in FIXTURE.read_text().splitlines():
        if l and not l.startswith("#"):
            a=[float(x) for x in l.split(",")]
            assert len(a)==8
            rows.append([(a[j],a[j+1]) for j in range(0,8,2)])
    assert len(rows)==46
    return rows


def quad(coords):
    return (Vertex*4)(*[Vertex(
        x,y,0.0,27.6,0,i*.13,i*.27)
        for i,(x,y) in enumerate(coords)
    ])


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("No host C compiler")
    folder=tmp_path_factory.mktemp("native-iris-v25")
    source=folder/"bounds.c";dll=folder/"bounds.so"
    source.write_text(
        '#include "ui_transition_native43_bounds25.h"\n'
        'void reset(SpiderNativeBounds25Frame*f){'
        'SpiderNativeBounds25_Reset(f);}\n'
        'int matte(const SpiderMenuVertex*v,unsigned w,unsigned h){'
        'return SpiderNativeBounds25_IsOuterMatte(v,w,h);}\n'
        'void observe(SpiderNativeBounds25Frame*f,const SpiderMenuVertex*v,'
        'unsigned w,unsigned h){SpiderNativeBounds25_Observe(f,v,w,h);}\n'
        'int widths(const SpiderNativeBounds25Frame*f,unsigned w,unsigned h,'
        'float*l,float*r){return SpiderNativeBounds25_Widths(f,w,h,l,r);}\n'
        'int build(const SpiderMenuVertex*p,unsigned w,unsigned h,'
        'float l,float r,SpiderMenuVertex out[2][4]){'
        'return SpiderNativeBounds25_Build(p,w,h,l,r,out);}\n'
    )
    result=subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror",
                    "-fPIC","-shared","-I",str(ROOT),
                    str(source),"-o",str(dll),"-lm"],
                   capture_output=True,text=True,check=False)
    assert result.returncode==0, (
        "V25 C header must compile cleanly:\n"+result.stderr)
    o=ct.CDLL(str(dll))
    o.reset.argtypes=[ct.POINTER(Frame)]
    o.matte.argtypes=[ct.POINTER(Vertex),ct.c_uint,ct.c_uint]
    o.matte.restype=ct.c_int
    o.observe.argtypes=[
        ct.POINTER(Frame),ct.POINTER(Vertex),ct.c_uint,ct.c_uint]
    o.widths.argtypes=[ct.POINTER(Frame),ct.c_uint,ct.c_uint,
                       ct.POINTER(ct.c_float),ct.POINTER(ct.c_float)]
    o.widths.restype=ct.c_int
    o.build.argtypes=[ct.POINTER(Vertex),ct.c_uint,ct.c_uint,
                      ct.c_float,ct.c_float,ct.POINTER(Vertex*4)]
    o.build.restype=ct.c_int
    return o


def geometry(api,w,h,zoom=1):
    f=Frame()
    api.reset(ct.byref(f))
    rects=original_quads()
    for pts in rects:
        v=quad([(
            w*.5+(x-960)*h/1080*zoom,
            h*.5+(y-540)*h/1080*zoom,
        ) for x,y in pts])
        before=bytes(v)
        api.observe(ct.byref(f),v,w,h)
        assert bytes(v)==before  # original V19 untouched
    a,b=ct.c_float(),ct.c_float()
    valid=api.widths(ct.byref(f),w,h,ct.byref(a),ct.byref(b))
    return valid,a.value,b.value,f


@pytest.mark.parametrize("width,height,gutter",[
    (1920,1080,240), (1280,720,160), (2560,1440,320),
    (1920,1200,160), (2560,1080,560),
    (3440,1440,760), (3840,1600,(3840-1600*4/3)/2),
    (5120,1440,1600),
    (1920,800,(1920-800*4/3)/2),
])
def test_initial_side_black_margins_derive_from_physical_resolution(
    api,width,height,gutter
):
    valid,l,r,frame=geometry(api,width,height)
    assert valid==1
    assert l==pytest.approx(gutter+1,abs=.015)
    assert r==pytest.approx(gutter+1,abs=.015)
    assert frame.observed==46
    assert frame.excluded_matte==4
    assert frame.eligible>=40


def test_real_native_spider_front_retracts_margins_before_touching_hooks(api):
    w,h=1920,1080
    values=[]
    for zoom in [1,2,3,4,4.5,5,5.5,6,7,8,9]:
        valid,l,r,frame=geometry(api,w,h,zoom)
        assert valid==1
        assert l==pytest.approx(r,abs=.05)
        assert frame.eligible>0
        values.append((zoom,l,r,frame.min_x,frame.max_x))
        # Unlike V24, no curtain can overpaint a visible native
        # hooked-tooth extremity, apart from the deliberate 1px seam.
        assert l<=max(0.0,frame.min_x+1.0)+.015
        assert r<=max(0.0,w-frame.max_x+1.0)+.015
    assert values[0][1]==pytest.approx(241,abs=.01)
    # Original V19 has hooked teeth that reach the physical edge
    # EARLIER than the naive 4:3 aperture radius predicted in V24.
    # Their actual onscreen XY extremities must win over that estimate.
    assert any(0.0 < v[1] < 241.0 for v in values)
    assert values[5][1]==0.0
    assert values[7][1]==0.0
    assert values[7][2]==0.0
    assert [v[1] for v in values]==sorted(
        [v[1] for v in values],reverse=True)


def test_four_original_mattes_excluded_by_shape_not_fan_number(api):
    w,h=1920,1080
    rows=original_quads()
    results=[]
    for i,poly in enumerate(rows):
        v=quad([(960+(x-960)*.75,y) for x,y in poly])
        results.append((i,api.matte(v,w,h)))
    assert [i for i,val in results if val]==[42,43,44,45]
    # Flat narrow genuine hook remains eligible, even when it lies
    # within one gutter at the end of the zoom.
    tooth=quad([(120,300),(160,300),(160,340),(120,340)])
    assert api.matte(tooth,w,h)==0


def test_on_screen_y_clip_ignores_offscreen_far_tooth_extrema(api):
    w,h=1920,1080
    f=Frame()
    api.reset(ct.byref(f))
    tooth=quad([(160,400),(280,400),(280,600),(160,600)])
    api.observe(ct.byref(f),tooth,w,h)
    right_before=f.max_x
    left_before=f.min_x
    # A physically invisible triangle with outrageous X should not
    # force the curtains to disappear before any on-screen tooth.
    outside=quad([(-9000,-1000),(-7000,-1000),
                  (-7000,-500),(-9000,-500)])
    api.observe(ct.byref(f),outside,w,h)
    assert f.min_x==pytest.approx(left_before)
    assert f.max_x==pytest.approx(right_before)
    a,b=ct.c_float(),ct.c_float()
    assert api.widths(ct.byref(f),w,h,
                      ct.byref(a),ct.byref(b))==1
    assert a.value==pytest.approx(161,abs=.01)
    assert b.value==pytest.approx(241,abs=.01)


def test_original_black_fans_winding_and_asymmetrical_widths_preserved(api):
    original=quad([(960,634.5),(982.5,630),
                   (982.5,657),(960,657)])
    before=bytes(original)
    out=((Vertex*4)*2)()
    assert api.build(original,1920,1080,121.0,37.0,out)==1
    assert {v.x for v in out[0]}=={0,121}
    assert {v.x for v in out[1]}=={1883,1920}
    assert all(v.y in (0.0,1080.0) for p in out for v in p)
    assert all(v.diffuse==0 and v.z==0.0 and
               abs(v.rhw-27.6)<.001 for p in out for v in p)
    assert bytes(original)==before
    def winding(points):
        a,b,c=points[:3]
        return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)
    for p in out:
        assert winding(p)*winding(original)>0


@pytest.mark.parametrize("w,h",[
    (1024,768),(1600,1200),(640,480),(1280,1024),
    (1080,1920),
])
def test_no_added_black_side_margins_on_4by3_or_narrower(api,w,h):
    v,l,r,frame=geometry(api,w,h)
    assert v==0 and l==0 and r==0
    out=((Vertex*4)*2)()
    assert api.build(quad([(960,634.5),(982.5,630),
                           (982.5,657),(960,657)]),
                     w,h,0,0,out)==0


def test_empty_or_invalid_geometry_does_not_expose_unverified_geometry(api):
    f=Frame()
    api.reset(ct.byref(f))
    l,r=ct.c_float(100),ct.c_float(100)
    assert api.widths(ct.byref(f),1920,1080,
                      ct.byref(l),ct.byref(r))==0
    assert l.value==r.value==0
    bad=quad([(float("nan"),500),(550,500),
              (550,600),(500,600)])
    api.observe(ct.byref(f),bad,1920,1080)
    assert f.eligible==0
    assert api.widths(ct.byref(f),1920,1080,
                      ct.byref(l),ct.byref(r))==0


def test_v25_is_the_only_permanent_native_mask_pipeline():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    hdr=(ROOT/"ui_transition_native43_bounds25.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_BOUNDS_TEST" not in src
    assert "InterlockedExchange(&ui_transition_bounds25_enabled,1);" in src
    assert "UI-TRANSITION-BOUNDS25: BUILT-IN" in src
    assert "UI-TRANSITION-BOUNDS25: FRAME=" in src
    assert "UI-TRANSITION-BOUNDS25: RELEASE" in src
    assert "SpiderNativeBounds25_Observe(&ui_transition_bounds25_frame,mapped" in src
    assert "SpiderNativeBounds25_Widths(" in src
    assert "SpiderNativeBounds25_Build(" in src
    assert "ui_transition_bounds25_prev_left" in src
    assert "ui_transition_bounds25_prev_right" in src
    assert "SPIDER_NATIVE_BOUNDS25_OVERLAP_PX 1.0f" in hdr
    assert "UI-TRANSITION-SYNC24: SOURCE COUNT MISMATCH" in src
    assert "uint64_t)width*3u<=(uint64_t)height*4u" in hdr
    for retired in (
        "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_GUTTERS_TEST",
        "SPIDERMAN2K1FIX_UI_TRANSITION_ATOMIC_RENDER_TEST",
        "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_SYNC_TEST"):
        assert retired not in src

def test_v25_original_iris_draw_is_before_safe_bounds_collection():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    a=src.index("static int ui_transition_native43_handle_draw(")
    b=src.index("static HRESULT WINAPI hooked_draw_primitive(",a)
    handle=src[a:b]
    assert handle.index("*result=original_draw_primitive(") < (
        handle.index("SpiderNativeBounds25_Observe("))
    assert "ui_transition_bounds25_enabled,0,0)!=1" in handle
    assert "SpiderNative43_MapBounded(v,mapped,4u" in handle
    assert "IDirect3DDevice7_CreateStateBlock(device,D3DSBT_ALL" in src
    assert "IDirect3DDevice7_ApplyStateBlock(device,saved)" in src
    assert "D3DRENDERSTATE_ZENABLE,D3DZB_FALSE" in src
