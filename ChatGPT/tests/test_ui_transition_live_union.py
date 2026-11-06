"""V15: live original polygons every EndScene, never a frozen template + zoom.

Tests run actual portable C algorithm on the 46 original 2001-source quads
captured from the user test, including nonuniform geometric motion.
The Windows D3D7 renderer still requires a real Wine video/log test.
"""
import ctypes as ct
import math
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "ChatGPT/tests/fixtures/spider_iris_native_20261106.csv"


class Quad(ct.Structure):
    _fields_ = [("x", ct.c_float * 4), ("y", ct.c_float * 4)]


class Point(ct.Structure):
    _fields_ = [("x", ct.c_float), ("y", ct.c_float)]


class Report(ct.Structure):
    _fields_ = [
        ("reason", ct.c_uint), ("seen", ct.c_uint),
        ("expected", ct.c_uint), ("unique", ct.c_uint),
        ("traps", ct.c_uint), ("union_reason", ct.c_uint),
        ("union_event", ct.c_uint), ("union_object", ct.c_uint),
        ("union_a", ct.c_float), ("union_b", ct.c_float),
        ("max_vertex_deviation", ct.c_float),
        ("rms_vertex_deviation", ct.c_float),
    ]


@pytest.fixture(scope="module")
def native(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if cc is None:
        pytest.skip("C compiler is unavailable")
    folder = tmp_path_factory.mktemp("spider-v15-live")
    source = folder / "perframe.c"
    libpath = folder / "perframe.so"
    source.write_text(
        '#include "ui_transition_live_union.h"\n'
        'static SpiderExactTemplate mesh;\n'
        'int prepare(const SpiderAtomicMaskQuad*live,unsigned n,'
        'unsigned expected,unsigned w,unsigned h,'
        'const SpiderAtomicMaskQuad*ref,unsigned refn,float zoom,'
        'SpiderLiveUnionReport*report){'
        'return SpiderLiveUnion_Prepare(live,n,expected,w,h,'
        'ref,refn,zoom,&mesh,report);}\n'
        'int build(unsigned w,unsigned h,float scale,'
        'SpiderAtomicVectorPoint*verts,unsigned cap,unsigned*n){'
        'return SpiderExact_Build(&mesh,w,h,scale,verts,cap,n);}\n'
    )
    subprocess.run([
        cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-fPIC", "-shared", "-I", str(ROOT),
        str(source), "-o", str(libpath), "-lm",
    ], check=True, text=True, capture_output=True)
    dll = ct.CDLL(str(libpath))
    dll.prepare.argtypes = [
        ct.POINTER(Quad), ct.c_uint, ct.c_uint, ct.c_uint,
        ct.c_uint, ct.POINTER(Quad), ct.c_uint,
        ct.c_float, ct.POINTER(Report),
    ]
    dll.prepare.restype = ct.c_int
    dll.build.argtypes = [
        ct.c_uint, ct.c_uint, ct.c_float, ct.POINTER(Point),
        ct.c_uint, ct.POINTER(ct.c_uint),
    ]
    dll.build.restype = ct.c_int
    return dll


def original_frames():
    quadrilaterals = []
    for line in FIXTURE.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        numbers = [float(v) for v in line.split(",")]
        assert len(numbers) == 8
        pts = [(numbers[2 * i], numbers[2 * i + 1]) for i in range(4)]
        quadrilaterals.extend((pts, [pts[j] for j in (0, 3, 2, 1)]))
    assert len(quadrilaterals) == 92
    return quadrilaterals


def make_quads(coords):
    return (Quad * len(coords))(*[
        Quad((ct.c_float * 4)(*[x for x, y in q]),
             (ct.c_float * 4)(*[y for x, y in q]))
        for q in coords
    ])


def moved_frame(amplitude):
    # Small plausible game-owned per-vertex movement that cannot be
    # represented by scaling the same reference around (960,540).
    # Both display edges still map to themselves, preserving the matte.
    return [
        [(x + amplitude * math.sin(math.pi * x / 1920),
          y + .55 * amplitude * math.sin(math.pi * y / 1080) *
          math.cos(math.pi * x / 1920))
         for x, y in quad]
        for quad in original_frames()
    ]


def evaluate(native, source, reference, estimated_scale):
    report = Report()
    assert native.prepare(source, len(source), len(reference),
                          1920, 1080, reference, len(reference),
                          estimated_scale, ct.byref(report)) == 1
    assert report.reason == 0
    assert report.seen == report.expected == 92
    assert report.unique == 46
    assert 0 < report.traps < 4096
    pts = (Point * 25000)()
    count = ct.c_uint()
    assert native.build(1920, 1080, 1.0, pts, len(pts),
                        ct.byref(count)) == 1
    assert count.value > 0 and count.value % 3 == 0
    return report, [(v.x, v.y) for v in pts[:count.value]]


def test_live_frame_geometry_not_replaced_by_static_reference(native):
    baseline = make_quads(original_frames())
    moved = make_quads(moved_frame(24))
    original_before = bytes(baseline)
    current_before = bytes(moved)
    a, mesh_a = evaluate(native, baseline, baseline, 1)
    b, mesh_b = evaluate(native, moved, baseline, 1)
    assert a.rms_vertex_deviation == pytest.approx(0, abs=.001)
    assert b.rms_vertex_deviation > 1
    assert b.max_vertex_deviation > 5
    assert len(mesh_b) > 0
    assert mesh_a != mesh_b
    assert bytes(baseline) == original_before
    assert bytes(moved) == current_before


@pytest.mark.parametrize("estimate", [.5, 1., 1.25, 3.79, 8.])
def test_scale_probe_does_not_reanimate_live_vertices(native, estimate):
    ref = make_quads(original_frames())
    live = make_quads(moved_frame(19))
    report, vertices = evaluate(native, live, ref, estimate)
    # Estimated scalar is diagnostic only. The output is IDENTICAL even
    # if the V14 probe guesses a wildly different scale for this frame.
    _, reference_vertices = evaluate(native, live, ref, 1.0)
    assert vertices == reference_vertices
    assert report.rms_vertex_deviation >= 0


def test_incomplete_native_scene_fails_closed_before_union(native):
    ref = make_quads(original_frames())
    frame = make_quads(original_frames()[:90])
    report = Report()
    assert native.prepare(frame, 90, 92, 1920, 1080,
                          ref, 92, 1.0, ct.byref(report)) == 0
    assert report.reason == 2
    assert report.seen == 90 and report.expected == 92
    assert native.prepare(frame, 90, 92, 1920, 1080,
                          ref, 92, 0.0, ct.byref(report)) == 0
    assert report.reason == 1


def test_v15_read_only_capture_and_same_scene_fallback_guards():
    source = (ROOT / "spiderman2k1fix.c").read_text()
    header = (ROOT / "ui_transition_live_union.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_PERFRAME_TEST" in source
    assert "ui_atomic_live_enabled=0" in source
    assert "ui_atomic_probe_raw_draw(type,fvf,verts,count" in source
    assert "ui_atomic_renderer_skip_native(" in source
    assert source.index("ui_atomic_probe_raw_draw(type,fvf,verts,count") < \
        source.index("if(ui_atomic_renderer_skip_native(", source.index(
            "static HRESULT WINAPI hooked_draw_primitive("))
    assert "ui_atomic_overflow==0u" in source
    assert "count==ui_atomic_scene_suppressed" in source
    assert "SpiderLiveUnion_Prepare(" in source
    assert "ui_atomic_renderer_draw_current_scene(device)" in source
    assert "UI-ATOMIC-PERFRAME: FALLBACK" in source
    assert "UI-ATOMIC-PERFRAME: COMPOSED" in source
    assert "return ui_atomic_renderer_draw_vector(device);" in source
    start = source.index("static int ui_atomic_renderer_draw_live(")
    end = source.index("static int ui_atomic_renderer_draw_current_scene(", start)
    fragment = source[start:end]
    assert "SpiderExact_Build(" in fragment
    assert "1.0f,points" in fragment
    assert "ui_atomic_scene_scale" not in fragment
    assert "D3DPT_TRIANGLELIST" in fragment
    assert "IDirect3DDevice7_ApplyStateBlock" in fragment
    assert "D3DRENDERSTATE_ZENABLE,D3DZB_FALSE" in fragment
    assert "uses CURRENT" in header
