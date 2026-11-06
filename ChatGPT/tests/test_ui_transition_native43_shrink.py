"""V23 smooth original-source-driven 16:9 curtains.

The source spider stays untouched. The new helper measures one calibrated
central fan from native source geometry and gradually narrows BOTH
physical gutter rectangles. No timed interpolation and no V20/V21
individual-tooth gutters. Assert all source fields and original winding.
The actual perceived 16:9 transition still needs real Wine footage.
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


def vertices(xys):
    return (Vertex*4)(*[
        Vertex(x,y,0.0,27.6,0,.125*i,.375*i)
        for i,(x,y) in enumerate(xys)
    ])


@pytest.fixture(scope="module")
def dll(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("No native C compiler")
    d=tmp_path_factory.mktemp("iris-v23")
    src=d/"v23.c";library=d/"v23.so"
    src.write_text(
        '#include "ui_transition_native43_shrink.h"\n'
        'int measure(const SpiderMenuVertex*s,unsigned w,unsigned h,float*out){'
        'return SpiderNativeShrink_AnchorHalfWidth(s,w,h,out);}\n'
        'int width(unsigned w,unsigned h,float r,float*out){'
        'return SpiderNativeShrink_Width(w,h,r,out);}\n'
        'int build(const SpiderMenuVertex*s,unsigned w,unsigned h,float band,'
        'SpiderMenuVertex out[2][4]){'
        'return SpiderNativeShrink_Build(s,w,h,band,out);}\n'
    )
    subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror",
        "-fPIC","-shared","-I",str(ROOT),str(src),
        "-o",str(library),"-lm"],
        check=True,text=True,capture_output=True)
    api=ct.CDLL(str(library))
    api.measure.argtypes=[
        ct.POINTER(Vertex),ct.c_uint,ct.c_uint,ct.POINTER(ct.c_float)]
    api.measure.restype=ct.c_int
    api.width.argtypes=[ct.c_uint,ct.c_uint,ct.c_float,
                        ct.POINTER(ct.c_float)]
    api.width.restype=ct.c_int
    api.build.argtypes=[
        ct.POINTER(Vertex),ct.c_uint,ct.c_uint,ct.c_float,
        ct.POINTER(Vertex*4)]
    api.build.restype=ct.c_int
    return api


def one_sample_fan():
    for line in FIXTURE.read_text().splitlines():
        if line and not line.startswith("#"):
            vals=[float(x) for x in line.split(",")]
            assert len(vals)==8
            return vertices([(vals[j],vals[j+1])
                             for j in range(0,8,2)])
    raise AssertionError("missing verified source geometry")


def test_real_iris_4by3_source_reference_gives_calibrated_half_width(dll):
    orig=one_sample_fan()
    before=bytes(orig)
    estimated=ct.c_float()
    assert dll.measure(orig,1920,1080,ct.byref(estimated))==1
    # From real source: 22.5px central fan * 9.5 original
    # half-width ratio * 0.75 physical aspect correction.
    assert estimated.value==pytest.approx(160.3125,abs=.001)
    assert bytes(orig)==before


def test_reverse_winding_mirrored_center_fan_also_measures(dll):
    original=one_sample_fan()
    xyz=[(v.x,v.y) for v in original]
    mirrored=vertices([(1920-x,y) for x,y in xyz][::-1])
    estimated=ct.c_float()
    assert dll.measure(mirrored,1920,1080,ct.byref(estimated))==1
    assert estimated.value==pytest.approx(160.3125,abs=.001)


def test_original_outer_masks_and_oblique_teeth_are_not_size_anchors(dll):
    samples=[
        [(0,0),(0,1080),(746.25,1080),(746.25,0)],
        [(0,0),(1920,0),(1920,1080),(0,1080)],
        [(746.25,418.5),(847.5,432),(825,468),(746.25,486)],
        [(960,480),(1100,430),(1200,540),(980,600)],
        [(900,630),(1100,630),(1100,657),(900,657)],
    ]
    for coords in samples:
        x=ct.c_float()
        assert dll.measure(vertices(coords),1920,1080,
                           ct.byref(x))==0


@pytest.mark.parametrize("aperture,expected_band",[
    (0,240),
    (200,240),
    (518.4,240),
    (720,120),  # Approx midpoint of 518.4 .. 921.6.
    (921.6,0),
    (1200,0),
])
def test_smooth_progressive_band_width_tied_to_source_size(
    dll,aperture,expected_band
):
    band=ct.c_float()
    assert dll.width(1920,1080,aperture,ct.byref(band))==1
    assert band.value==pytest.approx(expected_band,abs=1.5)


def test_band_width_monotonically_diminishes_across_animation(dll):
    last=240.0
    for radius in range(0,1200,3):
        band=ct.c_float()
        assert dll.width(1920,1080,float(radius),ct.byref(band))==1
        assert band.value<=last+1e-4
        assert band.value>=0.0
        last=band.value
    assert last==0.0


@pytest.mark.parametrize("w,h",[
    (1920,1080),(2560,1080),(3440,1440),(1920,1200),
])
def test_both_black_strips_shrink_only_outside_original_canvas(dll,w,h):
    original=one_sample_fan()
    untouched=bytes(original)
    band=ct.c_float()
    assert dll.width(w,h,w*.38,ct.byref(band))==1
    max_band=(w-h*4/3)/2
    assert 0<=band.value<=max_band
    out=((Vertex*4)*2)()
    assert dll.build(original,w,h,band.value,out)==1
    assert sorted({v.x for v in out[0]})==pytest.approx(
        [0,band.value],abs=.001)
    assert sorted({v.x for v in out[1]})==pytest.approx(
        [w-band.value,w],abs=.001)
    assert {v.y for v in out[0]}=={0,h}
    assert {v.y for v in out[1]}=={0,h}
    assert all(v.diffuse==0 for q in out for v in q)
    assert bytes(original)==untouched


def test_invalid_aspect_or_source_fails_closed(dll):
    band=ct.c_float()
    for w,h in [(640,480),(100,100),(1920,1500)]:
        assert dll.width(w,h,900,ct.byref(band))==0
    original=one_sample_fan()
    out=((Vertex*4)*2)()
    assert dll.build(original,1920,1080,-1,out)==0
    assert dll.build(original,1920,1080,999,out)==0


def test_early_collapsed_source_is_nonfatal_and_next_source_recovers(dll):
    # Actual Wine 2026-11-06 14:34:15.362: first native DRAW had
    # x=960 y=553.50 and an early degenerate fan; old V23 did a
    # one-shot helper call, set hold22_failed=1 and never retried.
    collapsed=vertices([(960.0,553.5)]*4)
    valid=one_sample_fan()
    curtains=((Vertex*4)*2)()
    assert dll.build(collapsed,1920,1080,240.0,curtains)==0
    assert dll.build(valid,1920,1080,240.0,curtains)==1
    for v in curtains[0]:
        assert v.x in (0.0,240.0)
    for v in curtains[1]:
        assert v.x in (1680.0,1920.0)


def test_actual_original_fan_animation_can_drive_curtain_to_zero(dll):
    original=one_sample_fan()
    centerx,centery=960.0,540.0
    prev=240.0
    for scale in [0.25,0.5,1,2,3,4,5,6.5,8.0]:
        scaled=vertices([(
            centerx+(v.x-centerx)*scale,
            centery+(v.y-centery)*scale) for v in original])
        radius=ct.c_float()
        band=ct.c_float()
        assert dll.measure(scaled,1920,1080,ct.byref(radius))==1
        assert dll.width(1920,1080,radius,ct.byref(band))==1
        assert band.value<=prev+.001
        prev=band.value
    assert prev==0.0


def test_opt_in_and_native_v19_remain_untouched_by_shrink_geometry():
    s=(ROOT/"spiderman2k1fix.c").read_text()
    h=(ROOT/"ui_transition_native43_shrink.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_SHRINK_GUTTERS_TEST" in s
    assert "ui_transition_shrink23_enabled=0" in s
    assert "UI-TRANSITION-SHRINK23: V23 ON" in s
    assert "UI-TRANSITION-SHRINK23: FRAME=" in s
    assert "UI-TRANSITION-SHRINK23: RELEASE" in s
    assert "SpiderNativeShrink_AnchorHalfWidth(" in s
    assert "SpiderNativeShrink_Width(" in s
    assert "SpiderNativeShrink_Build(" in s
    assert "UI-TRANSITION-SHRINK23: WAIT VALID SOURCE FAN" in s
    assert "ui_transition_shrink23_scene_deferred" in s
    assert "wait_for_source=1;" in s
    assert "++ui_transition_hold22_scene_native;" in s
    assert s.index("++ui_transition_hold22_scene_native;") < s.index(
        "if(InterlockedCompareExchange(&ui_transition_hold22_failed,0,0)!=1)")
    section=s[s.index("/* V22 one opaque physical-margin pair"):
              s.index("/* Reuse the verified original black-material state",s.index(
                  "/* V22 one opaque physical-margin pair"))]
    assert section.index("wait_for_source=1;") < section.index(
        "if(valid_side && (!shrink || width>=.5f))")
    assert "ui_transition_shrink23_scene_handled=1u;" in section
    assert "UI-TRANSITION-SHRINK23: invalid original source/gutter geometry;" not in s
    assert "if(ui_transition_hold22_scene_gutters==0u &&" in s
    assert "ui_transition_shrink23_half_width=0.0f;" in s
    assert "ui_transition_hold22_scene_native>0u" in s
    assert "V20 and V21 remain bypassed by V22" in s
    assert "No synthetic teeth" in h
    handle=s[s.index("static int ui_transition_native43_handle_draw("):
             s.index("static HRESULT WINAPI hooked_draw_primitive(")]
    assert handle.index("*result=original_draw_primitive") < \
           handle.index("SpiderNativeShrink_AnchorHalfWidth(") < \
           handle.index("SpiderNativeShrink_Build(") < \
           handle.index("SpiderNative43Gutter_Build(v,")
