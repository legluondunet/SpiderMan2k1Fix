"""Native ESC save confirmation text: keep all letters in one X transform.

An original long title extends beyond the middle-50% heuristic used for
PAUSED / REALLY QUIT?  The first/last glyphs were excluded, so the rest
compressed and created artificial gaps in "SA  VE GAME PROGRESS  ?".
"""
from pathlib import Path
import ctypes
import shutil
import subprocess
import tempfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "native_pause_title_bounds.h"
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")


def test_native_title_classifier_stays_inside_existing_safe_d3d_owner():
    assert HEADER.is_file()
    block = CORE.split("static const void *maybe_correct_native_pause43(", 1)[1].split(
        "/* Native ESC translucent geometric motif", 1
    )[0]
    assert '#include "native_pause_title_bounds.h"' in CORE
    assert "SpiderNativePause43_TextInZone(" in block
    for gate in (
        "native_pause43_enabled", "menu_upscale_active", "gameplay_4x3_active",
        "rva!=0x00102C72UL", "count!=4", "fvf!=0x144UL",
        "native_pause43_banner_flip", "tw==16 || tw==32 || tw==64",
        "fabsf(z-0.00098f)", "fabsf(rhw-12.686f)",
        "right-left<10.0f", "bottom-top<24.0f",
    ):
        assert gate in block
    assert "left<=(float)desktop_w*0.25f" not in block
    assert "right>=(float)desktop_w*0.75f" not in block


@pytest.fixture(scope="module")
def in_zone():
    if not shutil.which("gcc"):
        pytest.skip("host GCC unavailable")
    source = """
#include "native_pause_title_bounds.h"
int probe(float l, float r, float t, float b, float w, float h)
{ return SpiderNativePause43_TextInZone(l,r,t,b,w,h); }
"""
    with tempfile.TemporaryDirectory() as tmp:
        lib = Path(tmp) / "native_pause_title_test.so"
        subprocess.run(
            ["gcc", "-std=c11", "-O2", "-fPIC", "-shared",
             "-Wall", "-Wextra", "-Werror", "-I", str(ROOT),
             "-x", "c", "-", "-o", str(lib)],
            input=source, text=True, capture_output=True, check=True,
        )
        handle = ctypes.CDLL(str(lib))
        fn = handle.probe
        fn.argtypes = [ctypes.c_float] * 6
        fn.restype = ctypes.c_int
        yield fn


def test_save_title_left_center_and_question_mark_at_16_9(in_zone):
    # Approximate observed quad bounds, in full 1920x1080 native coordinates.
    # Last '?' extends beyond 75% of width; first S is left of 25%.
    assert in_zone(427, 477, 213, 266, 1920, 1080) == 1
    assert in_zone(915, 964, 213, 266, 1920, 1080) == 1
    assert in_zone(1470, 1515, 213, 266, 1920, 1080) == 1


def test_legacy_short_titles_and_middle_dialog_choices_unchanged(in_zone):
    assert in_zone(800, 860, 180, 232, 1920, 1080) == 1
    assert in_zone(900, 950, 510, 570, 1920, 1080) == 1
    assert in_zone(1470, 1515, 510, 570, 1920, 1080) == 0
    assert in_zone(427, 477, 510, 570, 1920, 1080) == 0


def test_wide_dialog_title_16_10_preserves_same_relative_geometry(in_zone):
    assert in_zone(427, 477, 236, 295, 1920, 1200) == 1
    assert in_zone(1470, 1515, 236, 295, 1920, 1200) == 1


def test_outside_title_or_far_side_glyphs_never_transformed(in_zone):
    for args in (
        (175, 215, 213, 266, 1920, 1080),
        (1650, 1700, 213, 266, 1920, 1080),
        (427, 477, 86, 140, 1920, 1080),
        (427, 477, 450, 510, 1920, 1080),
        (427, 477, 877, 930, 1920, 1080),
        (427, 427, 213, 266, 1920, 1080),
        (float("nan"), 477, 213, 266, 1920, 1080),
    ):
        assert in_zone(*args) == 0
