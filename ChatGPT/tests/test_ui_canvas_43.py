"""Opt-in unified 4:3 screen-space canvas geometry and host integration.

These tests do not certify HUD/world ownership. That still requires
read-only native gameplay traces and explicit Wine/Windows hardware tests.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]


class Vertex(ct.Structure):
    _fields_ = [
        ("x", ct.c_float), ("y", ct.c_float),
        ("z", ct.c_float), ("rhw", ct.c_float),
        ("diffuse", ct.c_uint32),
        ("u", ct.c_float), ("v", ct.c_float),
    ]


@pytest.fixture(scope="module")
def transform(tmp_path_factory):
    compiler = shutil.which("cc") or shutil.which("gcc")
    if not compiler:
        pytest.skip("C compiler needed to exercise native HUD math")
    folder = tmp_path_factory.mktemp("canvas43")
    source = folder / "probe.c"
    library = folder / "probe.so"
    source.write_text(
        '#include "ui_canvas_43.h"\n'
        'int canvas43(const SpiderMenuVertex *src, SpiderMenuVertex *dst,'
        'unsigned n, unsigned primitive, unsigned fvf, unsigned rva,'
        'unsigned w, unsigned h) {'
        'return SpiderUICanvas_43Transform(src,dst,n,primitive,fvf,rva,w,h);'
        '}\n'
    )
    subprocess.run(
        [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-fPIC", "-shared", "-I", str(ROOT),
         str(source), "-o", str(library)],
        check=True, capture_output=True, text=True,
    )
    lib = ct.CDLL(str(library))
    fn = lib.canvas43
    fn.argtypes = [
        ct.POINTER(Vertex), ct.POINTER(Vertex), ct.c_uint,
        ct.c_uint, ct.c_uint, ct.c_uint, ct.c_uint, ct.c_uint,
    ]
    fn.restype = ct.c_int
    return fn


def quad(l, t, r, b, *, z=0.00073, rhw=14.134,
         colour=0xFFFFFFFF):
    vertices = (Vertex * 4)()
    for i, (x, y) in enumerate([(l, t), (r, t), (r, b), (l, b)]):
        v = vertices[i]
        v.x, v.y, v.z, v.rhw = x, y, z, rhw
        v.diffuse = colour
        v.u, v.v = (i % 2), (i // 2) / 2
    return vertices


def apply(fn, original, *, screen=(1920, 1080),
          primitive=6, fvf=0x144, rva=0x00102C72, count=4):
    dest = (Vertex * 4)()
    done = fn(original, dest, count, primitive, fvf,
              rva, screen[0], screen[1])
    return done, dest


def assert_unchanged_except_x(src, dst):
    for old, new in zip(src, dst):
        assert bytes(old)[4:] == bytes(new)[4:]


@pytest.mark.parametrize("source", [
    quad(1668.8, 319.5, 1856.2, 454.5),  # portrait
    quad(1725, 652.5, 1815, 715.5),  # border pieces
    quad(1751.2, 445.5, 1796.2, 697.5,
         z=.00098, rhw=12.1569, colour=0xFF000000),  # dark fill
    quad(1751.2, 562.5, 1796.2, 697.5,
         z=.00073, rhw=14.134, colour=0xFF8080FF),  # violet fill
    quad(1680, 72, 1848.8, 211.5,
         z=.00024, rhw=20.9475),  # upper boss portrait
    quad(1080, 130.5, 1383.8, 166.5,
         z=.00098, rhw=12.1569, colour=0xFF00FF00),  # bar
    quad(795, 171, 847, 220,
         z=.00122, rhw=11.1764, colour=0xFF000000),  # text shadow
    quad(791, 171, 843, 220,
         z=.00098, rhw=12.6862, colour=0xFFFE3242),  # text foreground
])
def test_mary_jane_boss_and_fonts_share_identical_canvas_rule(transform, source):
    ok, corrected = apply(transform, source)
    assert ok == 1
    for v0, v1 in zip(source, corrected):
        assert v1.x == pytest.approx(960 + (v0.x - 960) * .75, abs=.02)
    assert_unchanged_except_x(source, corrected)


@pytest.mark.parametrize("resolution,scale", [
    ((1920, 1080), .75),
    ((1920, 1200), 5 / 6),
    ((2560, 1080), .5625),
    ((3440, 1440), (4 * 1440) / (3 * 3440)),
])
def test_centered_4_3_at_different_widescreen_resolutions(
        transform, resolution, scale):
    w, h = resolution
    original = quad(w * .85, h * .32, w * .94, h * .45)
    ok, corrected = apply(transform, original, screen=resolution)
    assert ok == 1
    assert corrected[0].x == pytest.approx(
        w / 2 + (original[0].x - w / 2) * scale, abs=.03
    )
    assert_unchanged_except_x(original, corrected)


@pytest.mark.parametrize("resolution", [(640, 480), (1024, 768), (1600, 1200)])
def test_original_four_by_three_never_reprojected(transform, resolution):
    original = quad(100, 100, 220, 230)
    assert apply(transform, original, screen=resolution)[0] == 0


def test_world_geometry_not_intercepted(transform):
    assert apply(transform, quad(330, 220, 450, 340,
              z=.9983, rhw=.0229))[0] == 0
    assert apply(transform, quad(330, 220, 450, 340,
              z=.1129, rhw=.2203))[0] == 0
    # Fake very-near quad that fails consistent per-vertex RHW.
    v = quad(330, 220, 450, 340)
    v[1].rhw = .01
    assert apply(transform, v)[0] == 0


@pytest.mark.parametrize("tint", [0xB0FF4040, 0xB040FF40])
def test_aiming_reticles_are_not_shifted_away_from_target(transform, tint):
    circle = quad(630, 567, 817.5, 706.5,
                  z=.00146, rhw=9.4993, colour=tint)
    assert apply(transform, circle)[0] == 0


def test_reject_wrong_draw_owners_and_invalid_geometry(transform):
    normal = quad(60, 60, 190, 160)
    assert apply(transform, normal, rva=0x00102C71)[0] == 0
    assert apply(transform, normal, primitive=4)[0] == 0
    assert apply(transform, normal, fvf=0x112)[0] == 0
    assert apply(transform, normal, count=3)[0] == 0
    normal[1].x = float("nan")
    assert apply(transform, normal)[0] == 0


def test_host_wiring_and_original_main_menu_isolated():
    source = (ROOT / "spiderman2k1fix.c").read_text()
    native = (ROOT / "hud_widescreen.c").read_text()
    assert '#include "ui_canvas_43.h"' in source
    assert "SPIDERMAN2K1FIX_UI_CANVAS_TEST" in source
    assert "SpiderUICanvas_43Transform(" in source
    assert "SpiderHudWidescreen_SetUICanvasPrototype(TRUE)" in source
    assert 'SpiderHudWidescreen_Installed()' in source
    assert "if(InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)!=1) {" in source
    assert "v=scale_menu_vertices(" in source
    assert "if(v==verts && count==4)" in source
    assert 'g_ui_canvas_prototype' in native
    assert "pause_at_flip_if_background();" in source
    assert "maybe_correct_aim_reticle(" in source


@pytest.mark.parametrize("resolution", [(1920, 1080), (1920, 1200), (3440, 1440)])
def test_full_frame_presentation_filters_preserve_physical_width(transform, resolution):
    w, h = resolution
    for colour in (0xFF000000, 0x80000000, 0x60FFFFFF, 0xFFAA3377):
        effect = quad(0, 0, w, h, colour=colour)
        assert apply(transform, effect, screen=resolution)[0] == 0


@pytest.mark.parametrize("resolution", [(1920, 1080), (1920, 1200), (3440, 1440)])
def test_top_and_bottom_cinematic_letterbox_bands_stay_full_width(
        transform, resolution):
    w, h = resolution
    top = quad(-.01 * w, -3, 1.01 * w, h * .13,
               colour=0xFF000000)
    bottom = quad(0, h * .85, w, h + 2,
                  colour=0xFF000000)
    assert apply(transform, top, screen=resolution)[0] == 0
    assert apply(transform, bottom, screen=resolution)[0] == 0


def test_native_middle_menu_panel_remains_in_four_by_three_canvas(transform):
    # A wide native pause ribbon is interface, not an edge cinematic band.
    panel = quad(-7.5, 430, 1923.8, 640, colour=0x880011CC)
    ok, mapped = apply(transform, panel)
    assert ok == 1
    assert mapped[0].x == pytest.approx(234.375, abs=.03)
    assert mapped[1].x == pytest.approx(1682.85, abs=.04)


def test_black_hud_fills_and_middle_world_art_not_lost(transform):
    # Being black alone never means "full-screen cinematic overlay".
    gauge = quad(1751.2, 445.5, 1796.2, 697.5,
                 colour=0xFF000000)
    assert apply(transform, gauge)[0] == 1
    # A large overlay that does not touch BOTH screen edges remains a
    # canvas candidate, even if coloured black.
    panel = quad(320, 0, 1600, 1080, colour=0xFF000000)
    assert apply(transform, panel)[0] == 1


def test_fullscreen_policy_has_a_trace_and_stays_opt_in():
    host = (ROOT / "spiderman2k1fix.c").read_text()
    code = (ROOT / "ui_canvas_43.h").read_text()
    assert "UI-CANVAS-FULLSCREEN: keep=" in host
    assert "SpiderUICanvas_43IsFullScreenEffect(" in host
    assert "SpiderUICanvas_43IsFullScreenEffect(" in code
    assert "SPIDERMAN2K1FIX_UI_CANVAS_TEST" in host
