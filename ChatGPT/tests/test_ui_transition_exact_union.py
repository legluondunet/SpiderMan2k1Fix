"""V14 exact native spider polygon union regression, real 46-quad fixture.

This is a math/source-geometry test, NOT proof of Direct3D7 hardware
rasterization or video parity. CI must not merge experimental PR #115.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ChatGPT/tests/fixtures/spider_iris_native_20261106.csv"


class Quad(ct.Structure):
    _fields_ = [("x", ct.c_float * 4), ("y", ct.c_float * 4)]


class Point(ct.Structure):
    _fields_ = [("x", ct.c_float), ("y", ct.c_float)]


class Diagnostic(ct.Structure):
    _fields_ = [
        ("reason", ct.c_uint), ("event", ct.c_uint),
        ("object", ct.c_uint), ("a", ct.c_float), ("b", ct.c_float),
    ]


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("native C compiler missing")
    folder = tmp_path_factory.mktemp("spider-exact-v14")
    source = folder / "union.c"
    shared = folder / "union.so"
    source.write_text(
        '#include "ui_transition_exact_union.h"\n'
        'static SpiderExactTemplate original;\n'
        'int prepare(const SpiderAtomicMaskQuad*quads,unsigned n,'
        'unsigned w,unsigned h,SpiderExactDiagnostic*d){'
        'return SpiderExact_Prepare(quads,n,w,h,&original,d);}\n'
        'int build(unsigned w,unsigned h,float s,'
        'SpiderAtomicVectorPoint*pts,unsigned cap,unsigned*n){'
        'return SpiderExact_Build(&original,w,h,s,pts,cap,n);}\n'
        'unsigned traps(void){return original.traps;}\n'
        'unsigned unique_quads(void){return original.unique_quads;}\n'
        'unsigned events(void){return original.events;}\n'
        'unsigned max_overlap(void){return original.largest_intervals;}\n'
    )
    subprocess.run(
        [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-fPIC", "-shared", "-I", str(ROOT), str(source),
         "-o", str(shared), "-lm"],
        check=True, text=True, capture_output=True,
    )
    dll = ct.CDLL(str(shared))
    dll.prepare.argtypes = [
        ct.POINTER(Quad), ct.c_uint, ct.c_uint, ct.c_uint,
        ct.POINTER(Diagnostic),
    ]
    dll.prepare.restype = ct.c_int
    dll.build.argtypes = [
        ct.c_uint, ct.c_uint, ct.c_float, ct.POINTER(Point),
        ct.c_uint, ct.POINTER(ct.c_uint),
    ]
    dll.build.restype = ct.c_int
    for method in ("traps", "unique_quads", "events", "max_overlap"):
        getattr(dll, method).restype = ct.c_uint
    return dll


def real_quads(duplicates=True):
    lines = SOURCE.read_text().splitlines()
    coords = []
    for line in lines:
        if line.startswith("#") or not line.strip():
            continue
        floats = [float(v) for v in line.split(",")]
        assert len(floats) == 8
        coords.append([(floats[2 * i], floats[2 * i + 1]) for i in range(4)])
    assert len(coords) == 46
    if duplicates:
        both = []
        for q in coords:
            both.extend((q, (q[0], q[3], q[2], q[1])))
        coords = both
    return (Quad * len(coords))(*[
        Quad((ct.c_float * 4)(*[x for x, _ in q]),
             (ct.c_float * 4)(*[y for _, y in q])) for q in coords
    ])


def make_mesh(api, source, scale):
    diagnostic = Diagnostic()
    assert api.prepare(source, len(source), 1920, 1080,
                       ct.byref(diagnostic)) == 1
    assert diagnostic.reason == 0
    points = (Point * 25000)()
    count = ct.c_uint()
    assert api.build(1920, 1080, scale, points, len(points),
                     ct.byref(count)) == 1
    assert count.value % 3 == 0
    return [(p.x, p.y) for p in points[:count.value]]


def point_in_triangle(x, y, tri):
    (ax, ay), (bx, by), (cx, cy) = tri
    cross = lambda vx, vy, wx, wy: vx * wy - vy * wx
    if abs(cross(bx - ax, by - ay, cx - ax, cy - ay)) < 1e-8:
        return False
    a = cross(bx - ax, by - ay, x - ax, y - ay)
    b = cross(cx - bx, cy - by, x - bx, y - by)
    c = cross(ax - cx, ay - cy, x - cx, y - cy)
    return (a >= -1e-5 and b >= -1e-5 and c >= -1e-5) or (
        a <= 1e-5 and b <= 1e-5 and c <= 1e-5
    )


def membership(points, x, y):
    return any(point_in_triangle(x, y, points[i:i + 3])
               for i in range(0, len(points), 3))


def source_black(quads, x, y):
    for q in quads:
        pts = [(q.x[i], q.y[i]) for i in range(4)]
        if point_in_triangle(x, y, [pts[i] for i in (0, 1, 2)]) or \
           point_in_triangle(x, y, [pts[i] for i in (0, 2, 3)]):
            return True
    return False


def test_real_captured_92_native_quads_reduce_to_exact_black_union(api):
    source = real_quads()
    before = bytes(source)
    mesh = make_mesh(api, source, 1.0)
    assert len(source) == 92
    assert api.unique_quads() == 46
    assert api.events() == 47
    assert api.traps() == 152
    assert api.max_overlap() == 12
    assert 0 < len(mesh) <= (152 + 4) * 6
    assert bytes(source) == before
    assert not membership(mesh, 960, 540)
    assert membership(mesh, 100, 540)


@pytest.mark.parametrize("scale", [0.5, 1.0, 3.79, 8.0])
def test_output_union_equals_original_native_black_for_nonstar_hooked_shape(
    api, scale
):
    source = real_quads()
    mesh = make_mesh(api, source, scale)
    # Verify every point on a deterministic sparse sample grid, avoiding
    # exact edge rasterization ambiguity. Compare original unmodified
    # black geometry in inverse display coordinates with the final mesh.
    for iy in range(19):
        y = (iy + .37) * 1080 / 19
        for ix in range(23):
            x = (ix + .41) * 1920 / 23
            ox = 960 + (x - 960) / (.75 * scale)
            oy = 540 + (y - 540) / scale
            expected = True if ox < 0 or ox > 1920 or oy < 0 or oy > 1080 \
                else source_black(source, ox, oy)
            result = membership(mesh, x, y)
            assert result == expected, (scale, x, y, expected, result)


def test_strict_reject_and_no_geometry_guess_when_source_is_incomplete(api):
    source = real_quads()
    diagnostic = Diagnostic()
    assert api.prepare(source, len(source), 1920, 1080,
                       ct.byref(diagnostic)) == 1
    # Lose full-height outer side matte: should not manufacture coverage.
    partial = (Quad * 84)(*list(source)[:84])
    assert api.prepare(partial, 84, 1920, 1080,
                       ct.byref(diagnostic)) == 0
    assert diagnostic.reason == 4
    # Center painted black is not a valid opening.
    blocker = Quad((900, 1020, 1020, 900), (490, 490, 590, 590))
    closed = (Quad * 93)(*list(source), blocker)
    assert api.prepare(closed, 93, 1920, 1080,
                       ct.byref(diagnostic)) == 0
    assert diagnostic.reason == 5


def test_v14_source_switch_keeps_v13_and_v12_unmodified():
    main = (ROOT / "spiderman2k1fix.c").read_text()
    header = (ROOT / "ui_transition_exact_union.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_EXACT_TEST" in main
    assert "ui_atomic_exact_enabled=0,ui_atomic_exact_ready=0" in main
    assert "SpiderExact_Prepare(" in main
    assert "SpiderExact_Build(" in main
    assert "UI-ATOMIC-EXACT: REJECTED" in main
    assert "UI-ATOMIC-EXACT: READY" in main
    assert "UI-ATOMIC-EXACT: COMPOSED" in main
    assert "immediate SAME FRAME V12 fallback" in main
    start = main.index("static int ui_atomic_renderer_draw_exact(")
    end = main.index("static int ui_atomic_renderer_draw_contour(", start)
    renderer = main[start:end]
    assert "D3DPT_TRIANGLELIST" in renderer
    assert "D3DCULL_NONE" in renderer
    assert "D3DRENDERSTATE_ALPHABLENDENABLE,FALSE" in renderer
    assert "IDirect3DDevice7_CreateStateBlock" in renderer
    assert "IDirect3DDevice7_ApplyStateBlock" in renderer
    assert "SpiderAtomicMask_CorrectReference43(" not in renderer
    assert "one CENTRAL opening" in header
