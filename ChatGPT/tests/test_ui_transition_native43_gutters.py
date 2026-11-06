"""V20: native iris side-fill only where original source black reaches x=0/W.

This is a portable C geometry test. It does not certify the D3D7
hardware side margins or the full reveal sequence: verify under Wine.
The V19 original fan remains untouched; only 0..gutter and W-gutter..W
may receive supplemental native-black pixels.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]


class Vertex(ct.Structure):
    _fields_=[
        ("x",ct.c_float),("y",ct.c_float),
        ("z",ct.c_float),("rhw",ct.c_float),
        ("diffuse",ct.c_uint32),("u",ct.c_float),("v",ct.c_float),
    ]


def quad(points):
    return (Vertex*4)(*[Vertex(
        x,y,0.0,27.6,0,.25*i,.125*i
    ) for i,(x,y) in enumerate(points)])


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc: pytest.skip("No C compiler")
    folder=tmp_path_factory.mktemp("iris-v20-gutters")
    src=folder/"gutters.c";binary=folder/"gutters.so"
    src.write_text(
        '#include "ui_transition_native43_gutters.h"\n'
        'int build(const SpiderMenuVertex*s,unsigned w,unsigned h,'
        'SpiderMenuVertex out[4][4],unsigned*n,unsigned*l,unsigned*r){'
        'return SpiderNative43Gutter_Build(s,w,h,out,n,l,r);}\n'
    )
    subprocess.run([
        cc,"-std=c11","-Wall","-Wextra","-Werror",
        "-fPIC","-shared","-I",str(ROOT),str(src),
        "-o",str(binary),"-lm",
    ],check=True,capture_output=True,text=True)
    dll=ct.CDLL(str(binary))
    dll.build.argtypes=[
        ct.POINTER(Vertex),ct.c_uint,ct.c_uint,
        ct.POINTER(Vertex*4),ct.POINTER(ct.c_uint),
        ct.POINTER(ct.c_uint),ct.POINTER(ct.c_uint),
    ]
    dll.build.restype=ct.c_int
    return dll


def build(api,source,w=1920,h=1080):
    out=((Vertex*4)*4)()
    n,l,r=ct.c_uint(),ct.c_uint(),ct.c_uint()
    valid=api.build(source,w,h,out,ct.byref(n),ct.byref(l),
                    ct.byref(r))
    rects=[[(v.x,v.y,v.z,v.rhw,v.diffuse) for v in out[i]]
           for i in range(n.value)]
    return valid,n.value,l.value,r.value,rects


def test_initially_opaque_source_edges_fill_both_16by9_side_strips(api):
    left=quad([(0,0),(180,0),(180,1080),(0,1080)])
    right=quad([(1740,0),(1920,0),(1920,1080),(1740,1080)])
    l=build(api,left)
    r=build(api,right)
    assert l[:4]==(1,1,1,0)
    assert r[:4]==(1,1,0,1)
    assert {x for rect in l[4] for x,y,*_ in rect}=={0,240}
    assert {x for rect in r[4] for x,y,*_ in rect}=={1680,1920}
    assert {y for rect in l[4] for x,y,*_ in rect}=={0,1080}
    assert {y for rect in r[4] for x,y,*_ in rect}=={0,1080}
    assert {y for rect in r[4] for x,y,*_ in rect}=={0,1080}


def test_late_iris_reveals_side_strips_without_clock_or_frozen_shape(api):
    opening_center=quad([(300,200),(1000,200),
                         (1000,800),(300,800)])
    valid,n,l,r,rects=build(api,opening_center)
    assert (valid,n,l,r)==(1,0,0,0)
    assert rects==[]
    # The native black on an edge can cover only some heights as its
    # hooks leave the 4:3 source region. No full-screen black fill.
    partial=quad([(0,270),(80,270),(80,540),(0,540)])
    valid,n,l,r,rects=build(api,partial)
    assert (valid,n,l,r)==(1,1,1,0)
    assert {y for rec in rects for x,y,*_ in rec}=={270,540}
    assert {x for rec in rects for x,y,*_ in rec}=={0,240}


def test_no_invented_gutter_when_triangle_only_touches_side_at_point(api):
    touches=quad([(0,330),(130,200),(250,420),(300,500)])
    valid,n,l,r,_=build(api,touches)
    assert (valid,n,l,r)==(1,0,0,0)


def test_native_fan_winding_and_material_remain_unchanged(api):
    original=quad([(0,270),(80,270),(80,540),(0,540)])
    before=bytes(original)
    reversed_source=quad([(0,270),(0,540),(80,540),(80,270)])
    a=build(api,original)
    b=build(api,reversed_source)
    assert a[:4]==(1,1,1,0)
    assert b[:4]==(1,1,1,0)
    assert bytes(original)==before
    assert a[4][0][0][-1]==0 and b[4][0][0][-1]==0
    def winding(rect):
        (ax,ay,*_),(bx,by,*_),(cx,cy,*_)=rect[:3]
        return (bx-ax)*(cy-ay)-(by-ay)*(cx-ax)
    assert winding(a[4][0])>0
    assert winding(b[4][0])<0
    for rec in a[4]+b[4]:
        assert all(z==0.0 and abs(rhw-27.6)<.001 and color==0
                   for x,y,z,rhw,color in rec)


@pytest.mark.parametrize("width,height,expected",[
    (1920,1080,240),(2560,1080,560),(3440,1440,760),
])
def test_gutter_extents_calculated_from_physical_aspect(api,width,height,expected):
    left=quad([(0,0),(140,0),(140,height),(0,height)])
    valid,n,l,r,rects=build(api,left,width,height)
    assert (valid,n,l,r)==(1,1,1,0)
    assert {x for rec in rects for x,y,*_ in rec}=={0,expected}


def test_invalid_source_dimensions_do_not_draw_or_recompose(api):
    source=quad([(0,0),(140,0),(140,1080),(0,1080)])
    assert build(api,source,640,480)[:4]==(0,0,0,0)
    assert build(api,source,320,240)[:4]==(0,0,0,0)


def test_v20_only_supplements_verified_v19_native_draws():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    header=(ROOT/"ui_transition_native43_gutters.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_GUTTERS_TEST" in src
    assert "ui_transition_native43_gutters_enabled=0" in src
    assert "UI-TRANSITION-GUTTER20: V20 ON" in src
    assert "UI-TRANSITION-GUTTER20: FRAME=" in src
    assert "UI-TRANSITION-GUTTER%s: DRAW FAILURE" in src
    assert "UI-TRANSITION-GUTTER20: GEOMETRY REJECTED" in src
    assert "SpiderNative43Gutter_Build(v," in src
    h=src[src.index("static int ui_transition_native43_handle_draw("):
          src.index("static HRESULT WINAPI hooked_draw_primitive(")]
    assert h.index("*result=original_draw_primitive(") < \
           h.index("SpiderNative43Gutter_Build(v,")
    assert "if(SUCCEEDED(*result)" in h
    assert "original_draw_primitive(" in h
    assert "SPIDER_TRANSITION_CALLER_RVA" in h
    assert "v[0].diffuse!=0x00000000UL" in h
    assert "blend||atest" in h
    assert "ui_transition_native43_extended_enabled,0,0)==1" in src
    assert "Never modify the input vertices" in header
