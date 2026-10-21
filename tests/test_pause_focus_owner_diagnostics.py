"""Read-only foreground-owner evidence for unexpected in-game auto-pause."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")

def test_focus_loss_owner_is_logged_at_actual_latch_not_every_flip():
    pause=SRC[SRC.index("static void pause_at_flip_if_background(void)"):
              SRC.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.count('pause_log_foreground_owner(hwnd,GetForegroundWindow(),"external-foreground-latched");')==1
    assert pause.count('pause_log_foreground_owner(hwnd,GetForegroundWindow(),"resume-foreground-state");')==1
    assert pause.index("pause_confirmed_focus_loss(hwnd)") < pause.index('pause_log_foreground_owner(hwnd,GetForegroundWindow(),"external-foreground-latched");')
    assert "GetForegroundWindow()==hwnd" in pause

def test_owner_diagnostic_is_passive_and_privacy_bounded():
    diag=SRC[SRC.index("static void pause_log_foreground_owner("):
             SRC.index("/* Ignore brief foreground changes", SRC.index("static void pause_log_foreground_owner("))]
    assert "if(!debug_log_enabled)return;" in diag
    assert "GetWindowThreadProcessId(" in diag
    assert "QueryFullProcessImageNameA(" in diag
    assert "PROCESS_QUERY_LIMITED_INFORMATION" in diag
    assert "GetClassNameA(" in diag
    assert "basename=strrchr(full," in diag
    assert "FOCUS-PAUSE-OWNER:" in diag
    assert "paused-after-ms=" in diag
    assert "log_line(line)" in diag
    for dangerous in ("SetForegroundWindow(","SetActiveWindow(","SetFocus(",
                      "AttachThreadInput(","ShowWindow(","PostMessage(","SendMessage(",
                      "SetWindowLong","VirtualProtect(","MessageBox("):
        assert dangerous not in diag

def test_focus_detection_wake_audio_and_overlay_untouched():
    assert "pause_confirmed_focus_loss(hwnd)" in SRC
    assert "SpiderPauseOverlay_Show(hwnd,pause_key_vk);" in SRC
    assert "SpiderPauseOverlay_Hide();" in SRC
    assert "SpiderFocusDSound_Pause();" in SRC
    assert "SpiderFocusMedia_Pause();" in SRC
    assert "SpiderFocusDSound_Resume();" in SRC
    assert "SpiderFocusMedia_Resume();" in SRC
