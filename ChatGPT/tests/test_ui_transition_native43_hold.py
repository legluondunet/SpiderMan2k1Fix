"""V22: hold both black widescreen gutters until native V19 iris stops.

The geometry tests exercise native D3D7 vertex preservation, original
winding, exact margins in 16:9/ultrawide, and numerical rejection.
The routing assertions ensure V20/V21 cannot create tooth rectangles
when V22 is on and an active scene produces only one gutter pair.
These tests cannot replace the requested full Wine visual acceptance.
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


def src(points,alpha=0):
    return (Vertex*4)(*[
        Vertex(x,y,0.0,27.6,alpha,i*.125,i*.25)
        for i,(x,y) in enumerate(points)
    ])


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc: pytest.skip("C compiler unavailable")
    d=tmp_path_factory.mktemp("iris-hold22")
    c=d/"test.c";out=d/"libtest.so"
    c.write_text(
        '#include "ui_transition_native43_hold.h"\n'
        'int hold(const SpiderMenuVertex*s,unsigned w,unsigned h,'
        'SpiderMenuVertex out[2][4]){'
        'return SpiderNativeHold_Build(s,w,h,out);}\n'
    )
    subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror",
                    "-fPIC","-shared","-I",str(ROOT),str(c),
                    "-o",str(out),"-lm"],
                   check=True,text=True,capture_output=True)
    lib=ct.CDLL(str(out))
    lib.hold.argtypes=[
        ct.POINTER(Vertex),ct.c_uint,ct.c_uint,
        ct.POINTER(Vertex*4),
    ]
    lib.hold.restype=ct.c_int
    return lib


def build(api,source,w=1920,h=1080):
    out=((Vertex*4)*2)()
    before=bytes(source)
    ok=api.hold(source,w,h,out)
    assert bytes(source)==before
    return ok,out


def winding(q):
    a,b,c=q[:3]
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)


@pytest.mark.parametrize("w,h,cut",[
    (1920,1080,240),(2560,1080,560),
    (3440,1440,760),(1920,1200,160),
])
def test_exact_16by9_side_widths_preserve_original_fan_winding(api,w,h,cut):
    original=src([(900,420),(1100,420),(1100,620),(900,620)])
    ok,out=build(api,original,w,h)
    assert ok==1
    assert {p.x for p in out[0]}=={0,cut}
    assert {p.x for p in out[1]}=={w-cut,w}
    for rectangle in out:
        assert {p.y for p in rectangle}=={0,h}
        assert winding(rectangle)*winding(original)>0
        assert all(p.diffuse==0 and p.z==0.0 and
                   abs(p.rhw-27.6)<.001 for p in rectangle)


def test_reverse_winding_same_fullscreen_black_strips(api):
    native=src([(920,520),(920,580),(1000,580),(1000,520)])
    ok,out=build(api,native)
    assert ok==1
    assert all(winding(row)<0 for row in out)
    assert {v.x for v in out[0]}=={0,240}
    assert {v.x for v in out[1]}=={1680,1920}


def test_unclipped_native_hooked_vertices_do_not_change_gutters(api):
    small=src([(960,634.5),(982.5,630),(982.5,657),(960,657)])
    huge=src([(-12000,-800),(20000,-1200),
              (22000,3000),(-10000,2000)])
    ok1,a=build(api,small)
    ok2,b=build(api,huge)
    assert ok1==ok2==1
    assert [[(p.x,p.y) for p in row] for row in a]==[
           [(p.x,p.y) for p in row] for row in b]
    # Gutter lifetime is scene native draw presence, not zoom/corner
    # extents. The original V19 geometry remains the only iris model.


def test_exact_real_source_fixture_remains_unchanged(api):
    original_lines=FIXTURE.read_text().splitlines()
    real=[v for v in original_lines if v and not v.startswith("#")]
    assert len(real)==46
    for line in real:
        xy=[float(v) for v in line.split(",")]
        polygon=src([(xy[i],xy[i+1]) for i in range(0,8,2)])
        ok,_=build(api,polygon)
        assert ok==1


def test_reject_nonblack_and_degenerate_input(api):
    good=src([(900,420),(1100,420),(1100,620),(900,620)])
    bad=src([(900,420),(1100,420),(1100,620),(900,620)],
            alpha=0xFF000000)
    degenerate=src([(900,420)]*4)
    assert build(api,good)[0]==1
    assert build(api,bad)[0]==0
    assert build(api,degenerate)[0]==0
    for width,height in ((640,480),(100,100),(1920,2400)):
        assert build(api,good,width,height)[0]==0


def test_internal_black_fan_builder_is_used_only_by_final_fallback():
    main=(ROOT/"spiderman2k1fix.c").read_text()
    sync=(ROOT/"ui_transition_native43_sync.h").read_text()
    hold=(ROOT/"ui_transition_native43_hold.h").read_text()
    assert "SpiderNativeHold_Build(source,width,height,gutters)" not in main
    assert "SpiderNativeHold_Build(source,width,height,gutters)" in (
        ROOT/"ui_transition_native43_shrink.h").read_text()
    assert "SpiderNativeHold_Build(source,width,height,sides)" in sync
    assert "No artificial teeth" in hold
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_HOLD_GUTTERS_TEST" not in main
