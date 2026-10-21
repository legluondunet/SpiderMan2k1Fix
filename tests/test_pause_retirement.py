"""Regression guards: automatic focus-loss pause was retired on both
native Windows and Wine. Source-level checks, not end-to-end runtime tests.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "spiderman2k1fix.c").read_text()
INI = (ROOT / "SpiderMan2k1Fix.ini").read_text()
GENERATED = (ROOT / "virtual_spidey_cfg.c").read_text()
VALID = (ROOT / "ini_validation.h").read_text()
README = (ROOT / "README.md").read_text()
BUILD = (ROOT / "build.sh").read_text()


def test_no_automatic_focus_pause_hooks_or_waits():
    for obsolete in (
        "pause_at_flip_if_background", "pause_on_focus_loss",
        "SpiderFocusEventPause", "SpiderPausePath",
        "SpiderWin32FocusTrace", "FOCUS-PAUSE:",
        "SPIDERMAN2K1FIX_PAUSE_PATH_TRACE",
        "SPIDERMAN2K1FIX_WIN32_FOCUS_MESSAGE_TRACE",
        'GetPrivateProfileIntA("Input","PauseOnFocusLoss"',
        'GetPrivateProfileIntA("Input","ExperimentalFocusEventPause"',
    ):
        assert obsolete not in CORE
    # The Flip still runs normal rendering/cursor sampling and input retry.
    flip = CORE.split("static HRESULT WINAPI hooked_flip(", 1)[1]
    flip = flip.split("static void instrument_surface(", 1)[0]
    assert "SpiderInputFocus_OnFlip(count);" in flip
    assert "SpiderDI8Retry_OnFlip(count);" in flip
    assert flip.count("SpiderInputFocus_OnFlip(count);") == 1
    for dangerous in ("MsgWaitForMultipleObjectsEx(", "SuspendThread(",
                      "Sleep(", "PostQuitMessage(", "PeekMessageA(",
                      "WaitForSingleObject("):
        assert dangerous not in flip


def test_pause_options_not_generated_or_advertised():
    for option in ("PauseOnFocusLoss=", "ExperimentalFocusEventPause="):
        assert option not in INI
        assert option not in GENERATED
    assert "Automatic background pausing is not supported" in README
    assert "FOCUS_EVENT_PAUSE_PROTOTYPE.md" not in README


def test_legacy_pause_options_accepted_but_ignored():
    # Preserve user-edited INIs from older versions without warning or
    # invalidation. They are intentionally recognized ONLY by the validator.
    for option in ('"PauseOnFocusLoss"', '"ExperimentalFocusEventPause"'):
        assert option in VALID
        assert option not in CORE
        assert option not in GENERATED
    assert "legacy ignored keys" in VALID


def test_remove_pause_only_modules_but_preserve_input_and_cursor():
    for obsolete in ("focus_event_pause.c", "pause_path_trace.c",
                     "win32_focus_message_trace.c"):
        assert obsolete not in BUILD
        assert not (ROOT / obsolete).exists()
    assert "input_notacquired_retry.c" in BUILD
    assert "input_focus_trace.c" in BUILD
    assert '#include "input_notacquired_retry.h"' in CORE
    assert '#include "input_focus_trace.h"' in CORE
    assert "RestoreCursorOnFocusLoss=1" in INI
    assert "ExperimentalAltTabReacquire=0" in INI
