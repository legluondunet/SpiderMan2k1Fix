"""V21 native full-height flank-only gutters, sourced from the original PC.

This isolates the two outer black-matte quads from all hooked spider teeth,
while leaving the V19 game-owned DrawPrimitive untouched. It does not claim
to have solved the TIME when the original outer matte disappears; the new
one-frame source dump is used to inspect that game behavior under Wine.
"""
import ctypes as ct
import math
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "ChatGPT/tests/fixtures/spider_iris_native_20261106.csv"


class Vertex(ct.Structure):
    _fields_ = [
        ("x", ct.c_float), ("y", ct.c_float), ("z", ct.c_float),
        ("rhw", ct.c_float), ("diffuse", ct.c_uint32),
        ("u", ct.c_float), ("v", ct.c_float),
    ]


def native_quad(coords):
    return (Vertex * 4)(*[
        Vertex(x, y, 0.0, 27.6, 0, i * 0.25, i * 0.125)
        for i, (x, y) in enumerate(coords)
    ])


def fixture():
    rows = []
    for line in NATIVE.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        nums = [float(n) for n in line.split(",")]
        assert len(nums) == 8
        q = [(nums[i * 2], nums[i * 2 + 1]) for i in range(4)]
        rows.extend([q, [q[j] for j in (0, 3, 2, 1)]])
    assert len(rows) == 92
    return rows


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("C compiler not installed")
    folder = tmp_path_factory.mktemp("spider-v21-flank")
    src, dll_path = folder / "flank.c", folder / "flank.so"
    src.write_text(
        '#include "ui_transition_native43_flank.h"\n'
        'int build(const SpiderMenuVertex*q,unsigned w,unsigned h,'
        'SpiderMenuVertex*rect,unsigned*side){'
        'return SpiderNativeFlank_Build(q,w,h,rect,side);}\n'
    )
    subprocess.run(
        [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-fPIC", "-shared", "-I", str(ROOT),
         str(src), "-o", str(dll_path), "-lm"],
        check=True, capture_output=True, text=True,
    )
    dll = ct.CDLL(str(dll_path))
    dll.build.argtypes = [
        ct.POINTER(Vertex), ct.c_uint, ct.c_uint,
        ct.POINTER(Vertex), ct.POINTER(ct.c_uint),
    ]
    dll.build.restype = ct.c_int
    return dll


def evaluate(api, coords, width=1920, height=1080):
    original = native_quad(coords)
    result = (Vertex * 4)()
    side = ct.c_uint()
    before = bytes(original)
    status = api.build(original, width, height, result, ct.byref(side))
    assert bytes(original) == before
    return status, side.value, result


def test_only_original_full_height_outer_quads_from_92_sources_qualify(api):
    all_quads = fixture()
    matched = []
    for i, coords in enumerate(all_quads):
        status, side, result = evaluate(api, coords)
        assert status == 1
        if side:
            matched.append((i, side, result))
    # Original quad #44 (and its opposite winding) covers the RIGHT
    # source exterior, #45 covers the LEFT. None of the 42 genuine
    # spider sub-polygons nor the original top/bottom matte qualifies.
    assert [(i, side) for i, side, _ in matched] == [
        (88, 2), (89, 2), (90, 1), (91, 1),
    ]
    for idx, side, pts in matched:
        xs = {p.x for p in pts}
        ys = {p.y for p in pts}
        assert xs == ({0, 240} if side == 1 else {1680, 1920})
        assert ys == {0, 1080}


def test_winding_of_original_both_sides_is_preserved(api):
    rows = fixture()
    for first in (88, 90):
        results = [evaluate(api, rows[i]) for i in (first, first + 1)]
        for i, (_, side, mapped) in enumerate(results):
            assert side in (1, 2)
            native = native_quad(rows[first + i])
            def signed_area(vertices):
                a,b,c = vertices[:3]
                return (b.x - a.x) * (c.y - a.y) - (
                    b.y - a.y) * (c.x - a.x)
            assert signed_area(native) * signed_area(mapped) > 0


def test_hooked_legs_and_partial_height_rectangles_never_fill_gutters(api):
    candidates = [
        [(0, 320), (110, 300), (150, 480), (0, 530)],
        [(0, 200), (0, 480), (150, 480), (150, 200)],
        [(0, 0), (210, 0), (90, 1080), (0, 1080)],
        [(0, 0), (0, 1080), (2000, 1080), (2000, 0)],
        [(0, 0), (0, 1080), (0, 1080), (200, 0)],
        [(-500, 100), (-500, 1000), (280, 1000), (280, 100)],
    ]
    for coords in candidates:
        status, side, _ = evaluate(api, coords)
        assert status == 1 and side == 0


@pytest.mark.parametrize("w,h,gutter", [
    (1920,1080,240), (2560,1080,560),
    (3440,1440,760), (1920,1200,160),
])
def test_side_fill_is_exclusively_outside_centered_4by3_native_canvas(
    api,w,h,gutter
):
    left = [(0,0),(0,h),(w*.39,h),(w*.39,0)]
    right = [(w,0),(w,h),(w*.61,h),(w*.61,0)]
    for coords, side, expected in [
        (left,1,{0,gutter}),
        (right,2,{w-gutter,w}),
    ]:
        status, observed, rect = evaluate(api,coords,w,h)
        assert status == 1 and observed == side
        assert {v.x for v in rect} == expected
        assert {v.y for v in rect} == {0,h}


def test_invalid_inputs_rejected_without_appending_black(api):
    source=native_quad([(0,0),(0,1080),(746.25,1080),(746.25,0)])
    rect=(Vertex*4)()
    side=ct.c_uint(123)
    assert api.build(source,640,480,rect,ct.byref(side))==0
    assert side.value==0
    bad=native_quad([(0,0),(0,float("nan")),(746.25,1080),(746.25,0)])
    assert api.build(bad,1920,1080,rect,ct.byref(side))==0
    assert side.value==0


def test_v21_is_an_explicit_opt_in_and_never_changes_v19_original_quads():
    main=(ROOT/"spiderman2k1fix.c").read_text()
    hdr=(ROOT/"ui_transition_native43_flank.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_FLANK_ONLY_TEST" in main
    assert "ui_transition_flank21_enabled=0" in main
    assert "UI-TRANSITION-FLANK21: V21 ON" in main
    assert "UI-TRANSITION-FLANK21: FRAME=" in main
    assert "UI-TRANSITION-FLANK21: OUTER-MATTE STATE" in main
    assert "UI-TRANSITION-FLANK21-SOURCE: BEGIN" in main
    assert "SpiderNativeFlank_Build(v," in main
    assert "SpiderNative43Gutter_Build(v," in main
    begin=main.index("static int ui_transition_native43_handle_draw(")
    end=main.index("static HRESULT WINAPI hooked_draw_primitive(",begin)
    hook=main[begin:end]
    assert hook.index("*result=original_draw_primitive") < \
        hook.index("SpiderNativeFlank_Build(v,")
    assert hook.index("SpiderNativeFlank_Build(v,") < \
        hook.index("SpiderNative43Gutter_Build(v,")
    assert "if(SUCCEEDED(*result)" in hook
    assert "No new artwork" in hdr
