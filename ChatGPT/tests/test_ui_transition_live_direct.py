"""V17 live-direct iris geometry: native triangles and conditional gutters.

No static sprite, no full-edge proof and no uniform zoom. These portable
C tests validate math only; game-owned source capture and Direct3D7
hardware fidelity must be verified with a Wine/Windows recording.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "ChatGPT/tests/fixtures/spider_iris_native_20261106.csv"
MAX = 128 * 30


class Quad(ct.Structure):
    _fields_ = [("x", ct.c_float * 4), ("y", ct.c_float * 4)]


class Point(ct.Structure):
    _fields_ = [("x", ct.c_float), ("y", ct.c_float)]


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    compiler = shutil.which("cc") or shutil.which("gcc")
    if not compiler:
        pytest.skip("No native C compiler")
    folder = tmp_path_factory.mktemp("spider-direct-v17")
    src = folder / "direct.c"
    out = folder / "direct.so"
    src.write_text(
        '#include "ui_transition_live_direct.h"\n'
        'int build(const SpiderAtomicMaskQuad*q,unsigned count,'
        'unsigned w,unsigned h,SpiderAtomicVectorPoint*p,'
        'unsigned cap,unsigned*n,unsigned*l,unsigned*r){'
        'return SpiderLiveDirect_Build(q,count,w,h,p,cap,n,l,r);}\n'
    )
    subprocess.run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-fPIC", "-shared", "-I", str(ROOT), str(src),
        "-o", str(out), "-lm",
    ], check=True, capture_output=True, text=True)
    dll = ct.CDLL(str(out))
    dll.build.argtypes = [
        ct.POINTER(Quad), ct.c_uint, ct.c_uint, ct.c_uint,
        ct.POINTER(Point), ct.c_uint, ct.POINTER(ct.c_uint),
        ct.POINTER(ct.c_uint), ct.POINTER(ct.c_uint),
    ]
    dll.build.restype = ct.c_int
    return dll


def q(points):
    return Quad(
        (ct.c_float * 4)(*[xy[0] for xy in points]),
        (ct.c_float * 4)(*[xy[1] for xy in points]),
    )


def fixture():
    data = []
    for line in FIXTURE.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        coords = [float(x) for x in line.split(",")]
        p = [(coords[2*i], coords[2*i+1]) for i in range(4)]
        data.extend((p, [p[i] for i in (0,3,2,1)]))
    assert len(data) == 92
    return (Quad*92)(*[q(p) for p in data])


def build(api, source):
    out = (Point * MAX)()
    n, left, right = ct.c_uint(), ct.c_uint(), ct.c_uint()
    status = api.build(source, len(source), 1920, 1080,
                       out, MAX, ct.byref(n), ct.byref(left),
                       ct.byref(right))
    return status, [(p.x, p.y) for p in out[:n.value]], left.value, right.value


def test_original_92_quads_render_without_static_template(api):
    source = fixture()
    before = bytes(source)
    status, points, left, right = build(api, source)
    assert status == 1
    assert len(points) >= 92 * 6
    assert len(points) % 3 == 0
    # The original opening is black on both left/right screen borders.
    assert left > 0 and right > 0
    assert bytes(source) == before


def test_one_live_iris_tooth_not_forced_to_fill_whole_exterior(api):
    source = (Quad*1)(q([(900, 440), (1020, 440),
                         (1020, 520), (900, 520)]))
    status, points, left, right = build(api, source)
    assert status == 1
    assert len(points) == 6
    assert (left, right) == (0, 0)
    assert points[0] == pytest.approx((915, 440), abs=.001)
    assert points[1] == pytest.approx((1005, 440), abs=.001)
    assert points[2] == pytest.approx((1005, 520), abs=.001)
    # No second uniform zoom, no forced outer rectangle.


def test_exact_left_gutter_only_where_native_polygon_intersects_edge(api):
    source = (Quad*1)(q([(-20, 200), (20, 200),
                         (20, 300), (-20, 300)]))
    status, points, left, right = build(api, source)
    assert status == 1 and left > 0 and right == 0
    assert len(points) > 6
    gutter = points[6:]
    assert all(0 <= x <= 240 for x, y in gutter)
    assert all(200 <= y <= 300 for x, y in gutter)
    # With the same shape shifted inward, no independent margin extension.
    inner = (Quad*1)(q([(20, 200), (40, 200),
                        (40, 300), (20, 300)]))
    status, points, left, right = build(api, inner)
    assert status == 1 and (left,right) == (0,0)
    assert len(points) == 6


def test_right_edge_opens_as_native_quads_leave_the_viewport(api):
    crosses = (Quad*1)(q([(1890, 200), (1930, 200),
                           (1930, 300), (1890, 300)]))
    status, points, left, right = build(api, crosses)
    assert status == 1 and right > 0 and left == 0
    gone = (Quad*1)(q([(1950, 200), (1960, 200),
                        (1960, 300), (1950, 300)]))
    status, points, left, right = build(api, gone)
    assert status == 1 and right == left == 0
    # The game can legitimately stop drawing outer black polygons.
    # Never recreate an artificial eight-legged silhouette from V12.


def test_invalid_geometry_fails_closed(api):
    malformed = (Quad*1)(q([(-280, 200), (20, 200),
                             (20, 300), (-280, 300)]))
    assert build(api, malformed)[0] == 0
    source = (Quad*1)(q([(20, 200), (40, 200),
                         (40, 300), (20, 300)]))
    points = (Point*MAX)()
    n,l,r=ct.c_uint(),ct.c_uint(),ct.c_uint()
    assert api.build(source, 0, 1920, 1080, points, MAX,
                     ct.byref(n),ct.byref(l),ct.byref(r))==0
    assert api.build(source, 1, 1920, 1080, points, 5,
                     ct.byref(n),ct.byref(l),ct.byref(r))==0


def test_source_capture_is_after_native_suppression_gate():
    src = (ROOT / "spiderman2k1fix.c").read_text()
    header = (ROOT / "ui_transition_live_direct.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_DIRECT_TEST" in src
    assert "ui_atomic_direct_enabled=0" in src
    assert "ui_atomic_direct_source[SPIDER_ATOMIC_MASK_MAX]" in src
    assert "ui_atomic_direct_count=0u;ui_atomic_direct_overflow=0u;" in src
    gate_start = src.index("static int ui_atomic_renderer_skip_native(")
    gate_end = src.index("static int ui_atomic_renderer_draw_vector(", gate_start)
    gate = src[gate_start:gate_end]
    assert gate.index("if(ui_atomic_scene_mode==2u)return 0;") < gate.index(
        "ui_atomic_direct_source[ui_atomic_direct_count++]"
    )
    assert "++ui_atomic_scene_suppressed" in gate
    start=src.index("static int ui_atomic_renderer_draw_direct(")
    end=src.index("static int ui_atomic_renderer_draw_current_scene(",start)
    renderer=src[start:end]
    assert "SpiderLiveDirect_Build(" in renderer
    assert "ui_atomic_scene_scale" not in renderer
    assert "D3DPT_TRIANGLELIST" in renderer
    assert "IDirect3DDevice7_ApplyStateBlock" in renderer
    assert "UI-ATOMIC-DIRECT: COMPOSED" in renderer
    assert "ui_atomic_renderer_draw_direct(device)" in src
    assert "UI-ATOMIC-DIRECT: FALLBACK" in src
    assert "only the native black surfaces" in header
