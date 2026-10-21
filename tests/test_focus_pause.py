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

def test_no_focus_pause_at_ddraw_flip_boundary():
    assert "static void pause_at_flip_if_background(void)" not in CORE
    assert "pause_at_flip_if_background();" not in CORE
    start=CORE.index("static HRESULT WINAPI hooked_flip(")
    end=CORE.index("SpiderDI8Retry_OnFlip(count);",start)
    flip=CORE[start:end]
    assert flip.count("SpiderInputFocus_OnFlip(count);")==1
    for obsolete in ("PeekMessageA(", "MsgWaitForMultipleObjectsEx(", "SuspendThread("):
        assert obsolete not in flip

def test_no_native_input_hook_is_enabled_by_default():
    assert "ExperimentalAltTabReacquire=0" in INI
    assert '"ExperimentalAltTabReacquire=0\\r\\n"' in GEN
