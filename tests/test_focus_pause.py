"""PR #2: focus pause is isolated from the base graphics and stability fixes."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text()
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text()
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()

def test_pause_is_opt_in_and_configured_consistently():
    assert "PauseOnFocusLoss=0" in INI
    assert '"PauseOnFocusLoss=0\\r\\n"' in GEN
    assert '"PauseOnFocusLoss"' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseOnFocusLoss",0,path)==1' in CORE

def test_pause_only_at_ddraw_flip_boundary():
    assert "static void pause_at_flip_if_background(void)" in CORE
    assert CORE.count("pause_at_flip_if_background();")==1
    start=CORE.index("static HRESULT WINAPI hooked_flip(")
    end=CORE.index("SpiderInputFocus_OnFlip(count);",start)
    assert "pause_at_flip_if_background();" in CORE[start:end]
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)") : start]
    for needle in ["GetForegroundWindow()==hwnd", "PeekMessageA", "PostQuitMessage",
                   "TranslateMessage", "DispatchMessageA", "MsgWaitForMultipleObjectsEx"]:
        assert needle in pause
    assert "SuspendThread" not in pause

def test_no_native_input_hook_is_enabled_by_default():
    assert "ExperimentalAltTabReacquire=0" in INI
    assert '"ExperimentalAltTabReacquire=0\\r\\n"' in GEN
