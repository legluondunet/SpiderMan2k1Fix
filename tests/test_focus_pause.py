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

def test_persistent_pause_requires_manual_foreground_key():
    pause=CORE[CORE.index("static volatile LONG pause_on_focus_loss=0;"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "GetAsyncKeyState(VK_F9)" in pause
    assert "GetForegroundWindow()!=hwnd" in pause
    assert "pause_latched" in pause
    assert "pause_key_down" in pause
    assert 'while(IsWindow(hwnd) && InterlockedCompareExchange(&pause_latched,0,0)==1)' in pause
    assert "while(IsWindow(hwnd) && GetForegroundWindow()!=hwnd)" not in pause
    assert "InterlockedExchange(&pause_latched,1)" in pause
    assert "PostQuitMessage" in pause
    assert "SuspendThread" not in pause
