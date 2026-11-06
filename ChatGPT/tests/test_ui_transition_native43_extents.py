"""V19: original iris remains in ONE 4:3 coordinate space when offscreen.

A strict V18 rejects some legitimate animated native vertices outside
+/-10% of the physical viewport and passes them through unchanged, while
other polygons are shrunk in X. V19 tests the controlled wider gate and
preserves the existing strict V18 API for immediate A/B.
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


def source(xs,ys=None):
    if ys is None: ys=[80,420,730,1070]
    return (Vertex*4)(*[Vertex(
        xs[i],ys[i],0,27.6,0,.125*i,.5)
        for i in range(4)])


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc: pytest.skip("Native C compiler unavailable")
    folder=tmp_path_factory.mktemp("iris-v19-extended")
    src=folder/"map.c";dllpath=folder/"map.so"
    src.write_text(
        '#include "ui_transition_native43.h"\n'
        'int map(const SpiderMenuVertex*a,SpiderMenuVertex*b,'
        'unsigned n,unsigned w,unsigned h,float ext){'
        'return SpiderNative43_MapBounded(a,b,n,w,h,ext);}\n'
        'int old(const SpiderMenuVertex*a,SpiderMenuVertex*b,'
        'unsigned n,unsigned w,unsigned h){'
        'return SpiderNative43_Map(a,b,n,w,h);}\n'
    )
    subprocess.run(
        [cc,"-std=c11","-Wall","-Wextra","-Werror","-shared",
         "-fPIC","-I",str(ROOT),str(src),"-o",str(dllpath),"-lm"],
        check=True,text=True,capture_output=True)
    dll=ct.CDLL(str(dllpath))
    dll.map.argtypes=[ct.POINTER(Vertex),ct.POINTER(Vertex),ct.c_uint,
                     ct.c_uint,ct.c_uint,ct.c_float]
    dll.map.restype=ct.c_int
    dll.old.argtypes=[ct.POINTER(Vertex),ct.POINTER(Vertex),ct.c_uint,
                     ct.c_uint,ct.c_uint]
    dll.old.restype=ct.c_int
    return dll


@pytest.mark.parametrize("xs",[
    [-200,300,900,2112],    # Original 10% rejection begins.
    [-700,130,1790,4000],   # Native legs expand far outside frame.
    [-12000,-1000,2400,22000],
])
def test_native_offscreen_limbs_share_same_affine_x_mapping(api,xs):
    a=source(xs)
    out=(Vertex*4)()
    original=bytes(a)
    assert api.old(a,out,4,1920,1080)==0
    assert api.map(a,out,4,1920,1080,32.0)==1
    for old,new in zip(a,out):
        assert new.x==pytest.approx(960+(old.x-960)*.75,abs=.001)
        assert new.y==old.y
        assert new.z==old.z and new.rhw==old.rhw
        assert new.diffuse==old.diffuse
        assert new.u==old.u and new.v==old.v
    assert bytes(a)==original


def test_v18_old_gate_still_rejects_too_far_original_vertices(api):
    a=source([-200,100,1850,2120])
    out=(Vertex*4)()
    assert api.old(a,out,4,1920,1080)==0
    assert api.map(a,out,4,1920,1080,.1)==0
    assert api.map(a,out,4,1920,1080,32)==1


def test_invalid_and_pathologically_large_source_stays_fail_closed(api):
    out=(Vertex*4)()
    for coords in [
        [-100000,0,1920,1920],
        [0,1920,100000,960],
        [float("nan"),0,1920,960],
        [float("inf"),0,1920,960],
    ]:
        assert api.map(source(coords),out,4,1920,1080,32)==0
    assert api.map(source([0,20,1800,1920]),out,4,1920,1080,100)==0


def test_v19_is_optional_only_for_exact_original_iris_not_bars_or_ui():
    main=(ROOT/"spiderman2k1fix.c").read_text()
    h=(ROOT/"ui_transition_native43.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_EXTENTS_TEST" in main
    assert "ui_transition_native43_extended_enabled=0" in main
    assert "SPIDER_TRANSITION_CALLER_RVA" in main
    assert "v[0].diffuse!=0x00000000UL" in main
    assert "SpiderNative43_MapBounded(v,mapped,4u" in main
    assert "SpiderNative43_Map(v,mapped,4u" in main
    assert "UI-TRANSITION-NATIVE43: NATIVE-UNCHANGED=" in main
    assert "reason=%d [1=scene 2=vertex 3=extents 4=material]" in main
    assert "opaque FF000000 cinematic bars unchanged" in main
    assert "return SpiderNative43_MapBounded(" in h
    assert "source,destination,count,width,height,.1f" in h
