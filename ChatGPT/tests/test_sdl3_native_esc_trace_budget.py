"""Native ESC controller instrumentation stays independent and read-only.

An original 3D ESC ribbon was unrecognized in Venom (2), so SDL3's
native_pause43_visible_near_flip gate could remain false. Normal
100-line SDL3 diagnostics were also exhausted before that level.
Record only original native Pause context transitions / UI button edges
without changing the existing button-to-keyboard behavior.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "sdl3_universal_gameplay.c").read_text(encoding="utf-8")
DRAW = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
POLICY = (ROOT / "native_pause_43_policy.h").read_text(encoding="utf-8")


def test_native_esc_menu_trace_independent_of_generic_action_limit():
    assert "static unsigned pause_menu_trace_reports=0;" in CORE
    assert "static void log_native_pause_menu(const char *msg)" in CORE
    block = CORE.split("static void log_native_pause_menu(", 1)[1].split(
        "static void log_focus_release(", 1
    )[0]
    assert "pause_menu_trace_reports++<48u" in block
    assert "report(msg);" in block
    assert "trace++<100u" not in block
    assert "SendInput(" not in block
    assert "send_scan(" not in block
    assert "update_keys(" not in block
    for text in (
        "SDL3-UNIVERSAL: PAUSE-MENU context transition",
        "SDL3-UNIVERSAL: PAUSE-MENU INPUT",
    ):
        pos = CORE.index(text)
        assert "log_native_pause_menu(msg);" in CORE[pos:pos+420]


def test_original_native_pause_status_controls_the_sdl3_context_gate():
    assert "native_pause43_visible_near_flip(count)" in DRAW
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in DRAW
    assert "context=spider_sdl3_universal_route_context(" in CORE
    assert "if(context!=SPIDER_UNIVERSAL_CTX_3D)" in CORE
    assert 'pulse(0x1cu,"menu Confirm")' in CORE
    assert 'pulse(0x01u,"menu Back/Pause resume")' in CORE
    assert 'pulse(0x01u,"3D Pause")' in CORE
    # The new exact renderer source signature permits native Pause
    # classification, but does NOT itself prove input was received.
    assert "SpiderNativePause43_Abs(rhw-10.6651f)<0.060f" in POLICY


def test_log_does_not_change_input_semantics():
    block = CORE.split("static void log_native_pause_menu(", 1)[1].split(
        "static void log_focus_release(", 1
    )[0]
    for forbidden in ("pulse(", "send_scan(", "restore_original(",
                      "update_keys(", "SendInput(", "VirtualProtect("):
        assert forbidden not in block
