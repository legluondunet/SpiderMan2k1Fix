"""Retired pause regression checks for Windows and Wine."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text()
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text()
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()

def test_pause_is_opt_in_and_configured_consistently():
    assert "PauseOnFocusLoss=" not in INI
    assert "PauseOnFocusLoss=" not in GEN
    assert '"PauseOnFocusLoss"' in VALID
    assert "PauseOnFocusLoss" not in CORE

def test_pause_only_at_ddraw_flip_boundary():
    assert "static void pause_at_flip_if_background(void)" in CORE
    assert CORE.count("pause_at_flip_if_background();")==1
    start=CORE.index("static HRESULT WINAPI hooked_flip(")
    end=CORE.index("SpiderDI8Retry_OnFlip(count);",start)
    flip=CORE[start:end]
    assert flip.count("pause_at_flip_if_background();")==1
    assert flip.count("SpiderInputFocus_OnFlip(count);")==2
    assert flip.index("SpiderInputFocus_OnFlip(count);") < flip.index("pause_at_flip_if_background();")
    assert "if(was_background &&" in flip
    assert flip.rindex("SpiderInputFocus_OnFlip(count);") > flip.index("pause_at_flip_if_background();")
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)") : start]
    for needle in ["GetForegroundWindow()==hwnd", "PeekMessageA", "PostQuitMessage",
                   "TranslateMessage", "DispatchMessageA", "MsgWaitForMultipleObjectsEx"]:
        assert needle in pause
    assert "SuspendThread" not in pause

def test_no_native_input_hook_is_enabled_by_default():
    assert "ExperimentalAltTabReacquire=0" in INI
    assert '"ExperimentalAltTabReacquire=0\\r\\n"' in GEN
