"""V24 regression: current-frame, not previous-frame, mask side curtains.

V23c draws side strips as soon as the first nondegenerate original black
fan is seen, BEFORE the remaining 92/84 iris fans have updated their
current opening radius. That produces a real one-native-frame phase lag.

V24 must collect all original fans first, compute V23c width once from
the exact same-frame radius, and draw only two black physical side
rectangles in D3D7 EndScene with full state restoration. Original V19
DrawPrimitive runs independently and is never suppressed/replayed.
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
        ("x",ct.c_float),("y",ct.c_float),("z",ct.c_float),
        ("rhw",ct.c_float),("diffuse",ct.c_uint32),
        ("u",ct.c_float),("v",ct.c_float),
    ]


class Frame(ct.Structure):
    _fields_=[
        ("prototype",Vertex*4),("radius",ct.c_float),
        ("native_fans",ct.c_uint),("anchors",ct.c_uint),
        ("ready",ct.c_uint),
    ]


def source(points):
    return (Vertex*4)(*[
        Vertex(x,y,0,27.6,0,.125*i,.375*i)
        for i,(x,y) in enumerate(points)
    ])


def true_native_center_fan():
    for line in FIXTURE.read_text().splitlines():
        if line and not line.startswith("#"):
            xy=[float(x) for x in line.split(",")]
            return [(xy[k],xy[k+1]) for k in (0,2,4,6)]
    raise AssertionError("missing native verified fixture")


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("Native C compiler unavailable")
    folder=tmp_path_factory.mktemp("spider-v24-sync")
    c=folder/"sync24.c";dll=folder/"sync24.so"
    c.write_text(
        '#include "ui_transition_native43_sync.h"\n'
        'void reset(SpiderNativeSync24Frame*f){SpiderNativeSync24_Reset(f);}\n'
        'void observe(SpiderNativeSync24Frame*f,const SpiderMenuVertex*v,'
        'unsigned w,unsigned h){SpiderNativeSync24_Observe(f,v,w,h);}\n'
        'int finish(const SpiderNativeSync24Frame*f,unsigned w,unsigned h,'
        'SpiderMenuVertex sides[2][4],float*width){'
        'return SpiderNativeSync24_Finish(f,w,h,sides,width);}\n'
    )
    subprocess.run([
        cc,"-std=c11","-Wall","-Wextra","-Werror",
        "-fPIC","-shared","-I",str(ROOT),str(c),
        "-o",str(dll),"-lm",
    ],capture_output=True,text=True,check=True)
    lib=ct.CDLL(str(dll))
    lib.reset.argtypes=[ct.POINTER(Frame)]
    lib.observe.argtypes=[
        ct.POINTER(Frame),ct.POINTER(Vertex),ct.c_uint,ct.c_uint]
    lib.finish.argtypes=[
        ct.POINTER(Frame),ct.c_uint,ct.c_uint,
        ct.POINTER(Vertex*4),ct.POINTER(ct.c_float)]
    lib.finish.restype=ct.c_int
    return lib


def finish(api,frame,w=1920,h=1080):
    bands=((Vertex*4)*2)()
    width=ct.c_float()
    code=api.finish(ct.byref(frame),w,h,bands,ct.byref(width))
    return code,width.value,bands


def test_current_frame_complete_source_corrects_one_frame_lag(api):
    frame=Frame()
    first=source([(0,0),(320,0),(320,1080),(0,1080)])
    anchor=true_native_center_fan()
    before=bytes(first)
    api.reset(ct.byref(frame))
    api.observe(ct.byref(frame),first,1920,1080)
    assert frame.native_fans==1 and frame.anchors==0
    # V23c erroneously DREW 240px at this point on every frame.
    earlier=finish(api,frame)
    assert earlier[0]==1 and earlier[1]==pytest.approx(240,abs=.001)
    large=source([
        (960+(x-960)*6.5,540+(y-540)*6.5)
        for x,y in anchor
    ])
    api.observe(ct.byref(frame),large,1920,1080)
    assert frame.native_fans==2 and frame.anchors==1
    code,current_width,bands=finish(api,frame)
    assert code==1 and current_width<240
    assert current_width==pytest.approx(179.14,abs=1.0)
    assert sorted({v.x for v in bands[0]})==pytest.approx(
        [0,current_width],abs=.001)
    assert sorted({v.x for v in bands[1]})==pytest.approx(
        [1920-current_width,1920],abs=.001)
    assert bytes(first)==before
    # An EndScene pass always uses the actual CURRENT frame, no cached
    # previous frame's radius or timer/estimated FPS.


def test_early_degenerate_sources_do_not_abort_same_frame(api):
    frame=Frame()
    api.reset(ct.byref(frame))
    degenerate=source([(960,553.5)]*4)
    api.observe(ct.byref(frame),degenerate,1920,1080)
    code,band,_=finish(api,frame)
    assert code==0 and band==0
    assert frame.native_fans==1 and frame.ready==0
    good=source([(0,0),(320,0),(320,1080),(0,1080)])
    api.observe(ct.byref(frame),good,1920,1080)
    code,band,_=finish(api,frame)
    assert code==1 and band==pytest.approx(240,abs=.001)
    assert frame.native_fans==2 and frame.ready==1


def test_end_of_animation_removes_full_gutters_no_final_pop(api):
    native=true_native_center_fan()
    frame=Frame()
    api.reset(ct.byref(frame))
    api.observe(ct.byref(frame),
                source([(0,0),(320,0),(320,1080),(0,1080)]),
                1920,1080)
    big=source([
        (960+(x-960)*12,540+(y-540)*12) for x,y in native
    ])
    api.observe(ct.byref(frame),big,1920,1080)
    code,band,_=finish(api,frame)
    assert code==2 and band==0.0
    assert frame.ready==1 and frame.anchors==1


def test_scene_reset_and_no_original_iris_never_paints(api):
    frame=Frame()
    api.reset(ct.byref(frame))
    code,band,_=finish(api,frame)
    assert code==0 and band==0
    original=source([(0,0),(320,0),(320,1080),(0,1080)])
    api.observe(ct.byref(frame),original,1920,1080)
    assert frame.native_fans==1
    api.reset(ct.byref(frame))
    assert frame.native_fans==0 and frame.ready==0 and frame.radius==0
    assert finish(api,frame)[0]==0


def test_strict_only_d3d7_current_end_scene_draw_and_state_restore():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    hdr=(ROOT/"ui_transition_native43_sync.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_SYNC_TEST" in src
    assert "UI-TRANSITION-SYNC24: V24 ON" in src
    assert "ui_transition_sync24_enabled=0" in src
    assert "SpiderNativeSync24_Reset(&ui_transition_sync24_frame)" in src
    assert "SpiderNativeSync24_Observe(&ui_transition_sync24_frame,v," in src
    assert "ui_transition_sync24_finish((IDirect3DDevice7*)self)" in src
    assert "D3DSBT_ALL" in src
    assert "IDirect3DDevice7_ApplyStateBlock(device,saved)" in src
    assert "IDirect3DDevice7_SetTexture(device,0,NULL)" in src
    assert "D3DRENDERSTATE_ALPHABLENDENABLE,FALSE" in src
    assert "D3DRENDERSTATE_ZENABLE,D3DZB_FALSE" in src
    assert "UI-TRANSITION-SYNC24: FRAME=" in src
    assert "ui_transition_sync24_frame.native_fans" in src
    handle=src[src.index("static int ui_transition_native43_handle_draw("):
               src.index("static HRESULT WINAPI hooked_draw_primitive(")]
    assert handle.index("*result=original_draw_primitive(") < \
           handle.index("SpiderNativeSync24_Observe(")
    assert "ui_transition_sync24_enabled,0,0)!=1" in handle
    end=src[src.index("static HRESULT WINAPI ui_compositor_scene_finish("):
            src.index("/* Read-only candidate scan:",src.index(
                "static HRESULT WINAPI ui_compositor_scene_finish("))]
    assert end.index("ui_transition_sync24_finish(") < end.index(
        "unsigned frame=++ui_transition_hold22_frames;")
    assert "FIRST SOURCE DrawPrimitive" not in hdr
