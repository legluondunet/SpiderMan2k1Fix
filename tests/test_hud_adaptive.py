"""Compile the pure portable adaptive HUD module and check its mapping.

The recorded Wine D3D7 numeric signatures are fixtures. These tests catch
an accidental transformation of generic world geometry or native 4:3
draws before the unvalidated opt-in game hook reaches a user's machine.
"""
import ctypes as c
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


class Vertex(c.Structure):
    _fields_ = [
        ("x", c.c_float), ("y", c.c_float),
        ("z", c.c_float), ("rhw", c.c_float),
        ("diffuse", c.c_uint32), ("u", c.c_float), ("v", c.c_float)
    ]


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if cc is None:
        pytest.skip("A host C compiler is required for this native layout test")
    target = tmp_path_factory.mktemp("hud2d") / "libspiderhud2d.so"
    subprocess.run([
        cc, "-std=c11", "-fPIC", "-shared", "-Wall", "-Wextra",
        "-Werror", "-I", str(ROOT), str(ROOT / "hud_adaptive.c"),
        "-o", str(target),
    ], check=True, capture_output=True, text=True)
    lib = c.CDLL(str(target))
    lib.SpiderHudAdaptive_Transform.argtypes = [
        c.POINTER(Vertex), c.POINTER(Vertex), c.c_uint, c.c_uint,
        c.c_uint32, c.c_uint32, c.c_uint, c.c_uint, c.c_uint, c.c_uint
    ]
    lib.SpiderHudAdaptive_Transform.restype = c.c_int
    return lib.SpiderHudAdaptive_Transform


def quad(x1, y1, x2, y2, *, z=0.0, rhw=27.6,
         diffuse=0xFFFF8E00, u0=0.95, u2=0.05):
    a = (Vertex * 4)()
    for v, x, y, u in zip(
        a,
        (x1, x2, x2, x1),
        (y1, y1, y2, y2),
        (u0, u2, u2, u0),
    ):
        v.x = x
        v.y = y
        v.z = z
        v.rhw = rhw
        v.diffuse = diffuse
        v.u = u
        v.v = 0.5
    return a


def mapped(api, source, tex=(64, 64), output=(1920, 1080), rva=0x00102C72):
    dest = (Vertex * 4)()
    kind = api(source, dest, 4, 6, 0x144, rva,
               tex[0], tex[1], output[0], output[1])
    return kind, dest


def check_other_coordinates_unchanged(a, b):
    for va, vb in zip(a, b):
        assert bytes(va)[4:] == bytes(vb)[4:]


def test_compass_mirrored_halves_right_anchor(api):
    left = quad(1428.8, 823.5, 1620.0, 1003.5)
    right = quad(1616.2, 823.5, 1807.5, 1003.5, u0=0.05, u2=0.95)
    for src in (left, right):
        kind, dst = mapped(api, src)
        assert kind == 2
        for v0, v1 in zip(src, dst):
            assert v1.x == pytest.approx(1920 + (v0.x - 1920) * 0.75, abs=0.01)
        check_other_coordinates_unchanged(src, dst)


def test_compass_pointer_follows_same_anchor(api):
    pointer = quad(1541.2, 864.0, 1638.8, 918.0,
                   z=0, rhw=26.0678, diffuse=0xFFFFFF00, u0=0, u2=0)
    kind, dst = mapped(api, pointer, tex=(0, 0))
    assert kind == 2
    assert dst[0].x == pytest.approx(1635.9, abs=0.02)


def test_instruction_panel_both_variants(api):
    variants = [
        quad(123.8, 864.0, 1173.8, 931.5,
             z=0.0012, rhw=11.1764, diffuse=0xA0000060),
        quad(11.2, 774.0, 667.5, 891.0,
             z=0.0012, rhw=11.1764, diffuse=0xA0600000),
    ]
    for src in variants:
        kind, dst = mapped(api, src, tex=(0, 0))
        assert kind == 1
        for original, copy in zip(src, dst):
            assert copy.x == pytest.approx(original.x * 0.75, abs=0.01)
        check_other_coordinates_unchanged(src, dst)


def test_instruction_letters_in_right_half_are_not_compass(api):
    glyph = quad(1128.0, 877.0, 1166.0, 918.0,
                 z=0.0012, rhw=11.1764, diffuse=0xFF000000,
                 u0=0, u2=0.875)
    kind, dst = mapped(api, glyph, tex=(32, 32))
    assert kind == 1
    assert dst[0].x == pytest.approx(846.0, abs=0.01)


def test_instruction_shadow_and_bright_foreground_both_scale_identically(api):
    # D3D7 F12 traces record two glyph passes with same physical position,
    # but different z/RHW/diffuse. The previous code scaled black shadow
    # and left lavender bright glyphs at full-width, outside their panel.
    origin = (1128.0, 877.0, 1166.0, 918.0)
    shadow = quad(*origin, z=0.0012, rhw=11.1764,
                  diffuse=0xFF000000, u0=0, u2=0.875)
    light = quad(*origin, z=0.0010, rhw=12.6862,
                 diffuse=0xFFC0C0FF, u0=0, u2=0.875)
    s_type, s_mapped = mapped(api, shadow, tex=(32, 32))
    l_type, l_mapped = mapped(api, light, tex=(32, 32))
    assert (s_type, l_type) == (1, 1)
    assert [(s_mapped[i].x, s_mapped[i].y) for i in range(4)] == [
        (l_mapped[i].x, l_mapped[i].y) for i in range(4)
    ]
    assert s_mapped[0].x == pytest.approx(846, abs=0.01)
    check_other_coordinates_unchanged(shadow, s_mapped)
    check_other_coordinates_unchanged(light, l_mapped)


@pytest.mark.parametrize("texture", [(16, 8), (16, 32), (32, 32), (64, 32)])
def test_instruction_bright_glyph_all_captured_atlas_sizes(api, texture):
    source = quad(345, 877, 399, 913, z=0.0010,
                  rhw=12.6862, diffuse=0xFFC0C0FF, u0=0, u2=0.875)
    kind, dest = mapped(api, source, tex=texture)
    assert kind == 1
    assert dest[0].x == pytest.approx(258.75, abs=0.01)


def test_foreground_signature_mismatch_and_upper_left_player_are_safe(api):
    foreground = quad(1128, 877, 1166, 918, z=0.0010,
                      rhw=12.6862, diffuse=0xFFC0C0FF, u0=0, u2=0.875)
    kind, _ = mapped(api, foreground, tex=(32, 32))
    assert kind == 1
    foreground[0].diffuse = 0xFFFFFFFF
    assert mapped(api, foreground, tex=(32, 32))[0] == 0
    foreground[0].diffuse = 0xFFC0C0FF
    foreground[0].rhw = 11.1764
    assert mapped(api, foreground, tex=(32, 32))[0] == 0
    player = quad(217.5, 112.5, 333.8, 139.5,
                  z=0.0010, rhw=12.6862, diffuse=0xFFFF0000)
    assert mapped(api, player, tex=(0, 0))[0] == 0
    player_glyph = quad(217.5, 112.5, 272.0, 139.5,
                        z=0.0010, rhw=12.6862, diffuse=0xFFC0C0FF)
    assert mapped(api, player_glyph, tex=(32, 32))[0] == 0


def test_world_particles_and_wrong_depth_stay_unchanged(api):
    world = quad(775.4, 307.4, 779.2, 311.9,
                 z=0.9983, rhw=0.0229, diffuse=0x80383838)
    assert mapped(api, world, tex=(0, 0))[0] == 0
    wrong_compass = quad(1428.8, 823.5, 1620, 1003.5,
                         z=0.9983, rhw=27.6)
    assert mapped(api, wrong_compass)[0] == 0
    wrong_glyph = quad(1005, 877, 1042, 913,
                       z=0.0012, rhw=5.0, diffuse=0xFF000000)
    assert mapped(api, wrong_glyph, tex=(32, 32))[0] == 0


def test_unsafe_calls_and_original_four_by_three_fail_closed(api):
    compass = quad(1428.8, 823.5, 1620, 1003.5)
    assert mapped(api, compass, rva=0x00102C71)[0] == 0
    assert mapped(api, compass, tex=(32, 32))[0] == 0
    assert mapped(api, compass, output=(1440, 1080))[0] == 0


def test_unchanged_fov_pause_and_default_ini():
    src = (ROOT / "spiderman2k1fix.c").read_text()
    ini = (ROOT / "SpiderMan2k1Fix.ini").read_text()
    build = (ROOT / "build.sh").read_text()
    assert '#include "hud_adaptive.h"' in src
    assert "SpiderHudWidescreen_Installed()" in src
    assert 'HUDWidescreenFix=0' in ini
    assert "pause_at_flip_if_background();" in src
    assert "maybe_correct_native_comic(" in src
    assert "if(v==verts && count==4)" in src
    assert "maybe_correct_adaptive_hud(" in src
    assert "hud_adaptive.c" in build
