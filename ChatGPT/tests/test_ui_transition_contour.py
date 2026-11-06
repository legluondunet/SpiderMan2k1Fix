"""V13 connected spider aperture: pure C tests of original polygon ray contour.

These tests certify geometry and opt-in guards only, NOT visual similarity to
original game footage or D3D7 driver compatibility (requires hardware run).
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
RAYS = 2048


class Quad(ct.Structure):
    _fields_ = [("x", ct.c_float * 4), ("y", ct.c_float * 4)]


class Contour(ct.Structure):
    _fields_ = [
        ("radius", ct.c_float * RAYS),
        ("minimum", ct.c_float), ("maximum", ct.c_float),
    ]


class Diagnostic(ct.Structure):
    _fields_ = [
        ("reason", ct.c_uint),
        ("quad_index", ct.c_uint),
        ("ray_index", ct.c_uint),
        ("first", ct.c_float),
        ("second", ct.c_float),
    ]


class Point(ct.Structure):
    _fields_ = [("x", ct.c_float), ("y", ct.c_float)]


def rectangle(l, t, r, b):
    return Quad((l, r, r, l), (t, t, b, b))


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("native C compiler unavailable")
    wd = tmp_path_factory.mktemp("spider-contour")
    source = wd / "contour.c"
    library = wd / "contour.so"
    source.write_text(
        '#include "ui_transition_atomic_contour.h"\n'
        'int prepare(const SpiderAtomicMaskQuad*q,unsigned n,'
        'unsigned w,unsigned h,SpiderAtomicContour*c){'
        'return SpiderAtomicContour_Prepare(q,n,w,h,c);}\n'
        'int prepare_detailed(const SpiderAtomicMaskQuad*q,unsigned n,'
        'unsigned w,unsigned h,SpiderAtomicContour*c,'
        'SpiderAtomicContourDiagnostic*d){'
        'return SpiderAtomicContour_PrepareDetailed(q,n,w,h,c,d);}\n'
        'int make(const SpiderAtomicContour*c,unsigned w,unsigned h,'
        'float scale,SpiderAtomicVectorPoint*p,unsigned cap,unsigned*n){'
        'return SpiderAtomicContour_Build(c,w,h,scale,p,cap,n);}\n'
    )
    subprocess.run([
        cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fPIC",
        "-shared", "-I", str(ROOT), str(source), "-o", str(library),
        "-lm",
    ], check=True, capture_output=True, text=True)
    dll = ct.CDLL(str(library))
    dll.prepare.argtypes = [
        ct.POINTER(Quad), ct.c_uint, ct.c_uint, ct.c_uint,
        ct.POINTER(Contour),
    ]
    dll.prepare.restype = ct.c_int
    dll.prepare_detailed.argtypes = [
        ct.POINTER(Quad), ct.c_uint, ct.c_uint, ct.c_uint,
        ct.POINTER(Contour), ct.POINTER(Diagnostic),
    ]
    dll.prepare_detailed.restype = ct.c_int
    dll.make.argtypes = [
        ct.POINTER(Contour), ct.c_uint, ct.c_uint, ct.c_float,
        ct.POINTER(Point), ct.c_uint, ct.POINTER(ct.c_uint),
    ]
    dll.make.restype = ct.c_int
    return dll


def complete_source():
    # Four original polygon strips encircle one transparent 200x200 square.
    # This deliberately tests a center hole, not a standalone spider sprite.
    return (Quad * 4)(
        rectangle(0, 0, 300, 600),
        rectangle(500, 0, 800, 600),
        rectangle(300, 0, 500, 200),
        rectangle(300, 400, 500, 600),
    )


def test_native_polygon_edges_recover_4by3_center_hole(api):
    source = complete_source()
    before = bytes(source)
    boundary = Contour()
    assert api.prepare(source, 4, 800, 600, ct.byref(boundary)) == 1
    # Right, down, left, up: the same 100px edge distance.
    for index in (0, 512, 1024, 1536):
        assert boundary.radius[index] == pytest.approx(100, abs=.01)
    # Corner at 45 degrees is at 100*sqrt(2), not a raster grid.
    assert boundary.radius[256] == pytest.approx(141.421356, abs=.05)
    assert bytes(source) == before


def test_native_scale_only_changes_one_complete_contour(api):
    source = complete_source()
    boundary = Contour()
    assert api.prepare(source, 4, 800, 600, ct.byref(boundary)) == 1
    verts = (Point * (RAYS * 6))()
    n = ct.c_uint()
    for zoom in (.05, .5, 1.0, 2.0, 8.0, 20.0):
        assert api.make(ct.byref(boundary), 800, 600, zoom, verts,
                        RAYS * 6, ct.byref(n)) == 1
        assert n.value == RAYS * 6  # ONE D3D7 triangle batch
        assert (verts[0].x, verts[0].y) == pytest.approx(
            (400 + 100 * zoom, 300), abs=.01
        )
        # All sectors share exactly the same inner contour vertices.
        assert verts[5].x == pytest.approx(verts[6].x, abs=.01)
        assert verts[5].y == pytest.approx(verts[6].y, abs=.01)


def test_exterior_transparent_gap_is_forced_black(api):
    # There is a deliberate uncovered 20px vertical island in the RIGHT
    # strip after its inner part. It must not open an extra view of 3D world.
    source = (Quad * 5)(
        rectangle(0, 0, 300, 600),
        rectangle(500, 0, 560, 600),
        rectangle(580, 0, 800, 600),
        rectangle(300, 0, 500, 200),
        rectangle(300, 400, 500, 600),
    )
    boundary = Contour()
    assert api.prepare(source, 5, 800, 600, ct.byref(boundary)) == 1
    assert boundary.radius[0] == pytest.approx(100, abs=.01)


def test_invalid_open_or_closed_center_fails_without_fabricating_iris(api):
    boundary = Contour()
    # Only one side: not all rays meet native black, so V12 fallback.
    partial = (Quad * 1)(rectangle(500, 0, 800, 600))
    assert api.prepare(partial, 1, 800, 600, ct.byref(boundary)) == 0
    # Center occluded by source native geometry: not a valid iris.
    closed = (Quad * 1)(rectangle(0, 0, 800, 600))
    assert api.prepare(closed, 1, 800, 600, ct.byref(boundary)) == 0
    # Invalid source sizes and resolutions are not guessed.
    full = complete_source()
    assert api.prepare(full, 4, 600, 800, ct.byref(boundary)) == 0
    assert api.prepare(full, 0, 800, 600, ct.byref(boundary)) == 0


def test_v13_strict_optin_uses_original_game_scale_and_safe_v12_fallback():
    source = (ROOT / "spiderman2k1fix.c").read_text()
    header = (ROOT / "ui_transition_atomic_contour.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_CONTOUR_TEST" in source
    assert "ui_atomic_contour_enabled=0" in source
    assert "ui_atomic_contour_ready=0" in source
    assert "SpiderAtomicContour_PrepareDetailed(" in source
    assert "UI-ATOMIC-CONTOUR: REJECTED" in source
    assert "ui_atomic_renderer_draw_vector(device)" in source
    assert "ui_atomic_renderer_draw_contour(device)" in source
    start = source.index("static int ui_atomic_renderer_draw_contour(")
    end = source.index("static int ui_atomic_renderer_draw_vector(", start)
    segment = source[start:end]
    assert "SpiderAtomicContour_Build(" in segment
    assert "ui_atomic_scene_scale" in segment
    assert "D3DPT_TRIANGLELIST" in segment
    assert "D3DCULL_NONE" in segment
    assert "IDirect3DDevice7_CreateStateBlock" in segment
    assert "IDirect3DDevice7_ApplyStateBlock" in segment
    assert "SpiderAtomicMask_Rasterize(" not in segment
    assert "SpiderAtomicMask_CorrectReference43(" not in segment
    assert "the CENTER-CONNECTED iris opening" in header


def test_v13_diagnostics_distinguish_invalid_center_and_missing_ray(api):
    contour, diagnostic = Contour(), Diagnostic()
    # A single right-side strip leaves some rays without native black.
    partial = (Quad * 1)(rectangle(500, 0, 800, 600))
    assert api.prepare_detailed(partial, 1, 800, 600, ct.byref(contour),
                                ct.byref(diagnostic)) == 0
    assert diagnostic.reason == 3
    assert diagnostic.ray_index < RAYS
    # A full-screen black source cannot be a center-connected opening.
    full_black = (Quad * 1)(rectangle(0, 0, 800, 600))
    assert api.prepare_detailed(full_black, 1, 800, 600,
                                ct.byref(contour), ct.byref(diagnostic)) == 0
    assert diagnostic.reason == 2
    assert diagnostic.quad_index == 0
    # Complete ring still passes: the diagnostic addition changes no art.
    complete = complete_source()
    assert api.prepare_detailed(complete, 4, 800, 600, ct.byref(contour),
                                ct.byref(diagnostic)) == 1
    assert diagnostic.reason == 0
