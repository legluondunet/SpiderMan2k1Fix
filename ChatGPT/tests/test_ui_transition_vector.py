"""V12 native-geometry spider iris. These tests cover the actual C math.
Real Direct3D7 visual acceptance under Wine/Windows is still required.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
MAX_POINTS = (128 + 4) * 6


class Quad(ct.Structure):
    _fields_ = [
        ("x", ct.c_float * 4),
        ("y", ct.c_float * 4),
    ]


class Point(ct.Structure):
    _fields_ = [
        ("x", ct.c_float),
        ("y", ct.c_float),
    ]


@pytest.fixture(scope="module")
def mesh(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("native C compiler unavailable")
    directory = tmp_path_factory.mktemp("spider-vector")
    src = directory / "vector.c"
    libpath = directory / "vector.so"
    src.write_text(
        '#include "ui_transition_atomic_vector.h"\n'
        'int make(const SpiderAtomicMaskQuad*q,unsigned count,'
        'unsigned w,unsigned h,float zoom,SpiderAtomicVectorPoint*p,'
        'unsigned cap,unsigned*n){'
        'return SpiderAtomicVector_Build(q,count,w,h,zoom,p,cap,n);'
        '}\n'
    )
    subprocess.run(
        [
            cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fPIC", "-shared", "-I", str(ROOT),
            str(src), "-o", str(libpath), "-lm",
        ],
        check=True, capture_output=True, text=True,
    )
    dll = ct.CDLL(str(libpath))
    dll.make.argtypes = [
        ct.POINTER(Quad), ct.c_uint, ct.c_uint, ct.c_uint,
        ct.c_float, ct.POINTER(Point), ct.c_uint,
        ct.POINTER(ct.c_uint),
    ]
    dll.make.restype = ct.c_int
    return dll


def quads():
    # Two native quads meeting at the SAME source edge, deliberately.
    # Scaling must not move either edge independently or create detached feet.
    return (Quad * 2)(
        Quad((200, 320, 320, 200), (180, 180, 300, 300)),
        Quad((320, 440, 440, 320), (180, 180, 300, 300)),
    )


def build(mesh, ref, width, height, zoom=1):
    points = (Point * MAX_POINTS)()
    count = ct.c_uint()
    ok = mesh.make(ref, len(ref), width, height, zoom,
                   points, MAX_POINTS, ct.byref(count))
    assert ok == 1
    assert count.value % 3 == 0
    return [(p.x, p.y) for p in points[:count.value]]


@pytest.mark.parametrize("w,h,ratio", [
    (1920, 1080, .75),
    (1920, 1200, 5 / 6),
    (2560, 1080, .5625),
    (3440, 1440, 5760 / 10320),
    (1280, 960, 1.0),
])
def test_native_vertices_use_one_common_affine_mapping(mesh, w, h, ratio):
    ref = quads()
    before = bytes(ref)
    for zoom in (.25, .5, 1, 2, 6):
        verts = build(mesh, ref, w, h, zoom)
        # Exterior matte precedes source mesh; last twelve are source quads.
        a, b = verts[-12:-6], verts[-6:]
        assert a[1] == pytest.approx(b[0], abs=.0001)
        assert a[2] == pytest.approx(b[3], abs=.0001)
        assert a[1] == pytest.approx(b[0], abs=.0001)
        source = (320, 180)
        expected = (w / 2 + (source[0] - w / 2) * ratio * zoom,
                    h / 2 + (source[1] - h / 2) * zoom)
        assert a[1] == pytest.approx(expected, abs=.01)
    assert bytes(ref) == before


def test_black_outer_matte_participates_in_exact_same_zoom(mesh):
    ref = quads()
    at_one = build(mesh, ref, 1920, 1080, 1)
    at_half = build(mesh, ref, 1920, 1080, .5)
    at_four_three = build(mesh, ref, 1280, 960, 1)
    # At 16:9 normal size, 2 side rectangles (=4 triangles) + 4
    # original source triangles. At half-scale, top/bottom are added
    # from the same transformed canvas, not from independent tooth edits.
    assert len(at_one) == 24
    assert len(at_half) == 36
    assert len(at_four_three) == 12
    # Outer left matte starts at physical top-left.
    assert at_half[0] == (0, 0)
    # The left matte ends at the SAME X as the projected source canvas.
    edge_x = 1920 / 2 * (1 - .75 * .5)
    assert at_half[1][0] == pytest.approx(edge_x, abs=.01)


def test_fail_closed_invalid_inputs(mesh):
    ref = quads()
    points = (Point * MAX_POINTS)()
    out_count = ct.c_uint(777)
    assert mesh.make(ref, 2, 1920, 1080, 0,
                     points, MAX_POINTS, ct.byref(out_count)) == 0
    assert out_count.value == 0
    assert mesh.make(ref, 2, 800, 1200, 1,
                     points, MAX_POINTS, ct.byref(out_count)) == 0
    assert mesh.make(ref, 2, 1920, 1080, 1,
                     points, 6, ct.byref(out_count)) == 0
    assert mesh.make(ref, 0, 1920, 1080, 1,
                     points, MAX_POINTS, ct.byref(out_count)) == 0


def test_v12_preserves_opt_in_and_uses_one_untextured_batch():
    src = (ROOT / "spiderman2k1fix.c").read_text()
    header = (ROOT / "ui_transition_atomic_vector.h").read_text()
    start = src.index("static int ui_atomic_renderer_draw_vector(")
    stop = src.index("static int ui_atomic_renderer_draw(", start)
    draw = src[start:stop]
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_VECTOR_TEST" in src
    assert "ui_atomic_vector_enabled=0" in src
    assert "ui_atomic_fidelity_enabled,0,0)==1" in src
    assert "SpiderAtomicVector_Build(" in draw
    assert "IDirect3DDevice7_CreateStateBlock(" in draw
    assert "IDirect3DDevice7_ApplyStateBlock(" in draw
    assert "IDirect3DDevice7_SetTexture(device,0,NULL)" in draw
    assert "D3DCULL_NONE" in draw
    assert "D3DPT_TRIANGLELIST" in draw
    assert "ui_atomic_scene_scale" in draw
    assert "SpiderAtomicMask_Rasterize(" not in draw
    assert "SpiderAtomicMask_CorrectReference43(" not in draw
    assert "ui_atomic_renderer_draw_vector(device)" in src
    assert "ONE 4:3 X correction" in header
