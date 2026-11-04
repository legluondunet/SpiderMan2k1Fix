"""Read-only foreground-owner evidence for unexpected in-game auto-pause."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
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
    assert "SpiderPauseSurface_Begin(" in SRC
    assert "SpiderPauseSurface_End();" in SRC
    assert "SpiderFocusDSound_Pause();" in SRC
    assert "SpiderFocusMedia_Pause();" in SRC
    assert "SpiderFocusDSound_Resume();" in SRC
    assert "SpiderFocusMedia_Resume();" in SRC


def test_x11_aligned_timeline_is_explicit_opt_in_and_does_not_rewrite_default_log():
    # Environment flag is observed only with Debug Log=1. In its absence
    # unchanged log strings still start with their original markers.
    assert "static int focus_timeline_enabled=0;" in SRC
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_FOCUS_TIMELINE"' in SRC
    assert "focus_timeline_enabled=(n==1 && opt[0]=='1');" in SRC
    assert 'GetPrivateProfileIntA("Debug","Log",0,path)==1' in SRC
    logger=SRC[SRC.index("static void log_line("):SRC.index("/* Windows creates")]
    assert "if(focus_timeline_enabled)" in logger
    assert "GetLocalTime(&st);" in logger
    assert "GetTickCount()" in logger
    assert "FOCUS-CLOCK" in logger
    assert 'fprintf(f,"%s\\n",s);' in logger
    assert "fclose(f);" in logger


def test_sampled_directdraw_blt_and_flip_timeline_is_read_only_bounded():
    trace=SRC[SRC.index("static void focus_timeline_pulse("):
              SRC.index("static HRESULT WINAPI hooked_blt(")]
    assert "if(!focus_timeline_enabled)return;" in trace
    assert "InterlockedExchange(last_call" in trace
    assert "InterlockedExchange(last_sample" in trace
    assert "GetForegroundWindow()" in trace
    assert "GetWindowThreadProcessId(" in trace
    assert "(DWORD)(now-sampled)<250u" in trace
    assert "gap<750u" in trace
    assert "FOCUS-TIMELINE: callback=%s" in trace
    assert '"Flip":"Blt"' in trace
    assert "surface-is-primary" in trace
    assert "menu-active" in trace
    assert "movie-active" in trace
    assert "thread=%lu" in trace
    for dangerous in ("SetForegroundWindow(", "SetFocus(", "SetActiveWindow(",
                      "BinkPause(", "SendInput(", "SpiderPauseSurface_Begin(",
                      "IDirectDrawSurface7_GetSurfaceDesc(",
                      "IDirectDrawSurface7_Lock(", "CreateWindow", "SetWindowLongPtr"):
        assert dangerous not in trace
    blt=SRC[SRC.index("static HRESULT WINAPI hooked_blt("):
            SRC.index("/* The surface-creation hook remains responsible for installing the shared vtable. */")]
    flip=SRC[SRC.index("static HRESULT WINAPI hooked_flip("):]
    assert "focus_timeline_pulse(0,count,self);" in blt
    assert "focus_timeline_pulse(1,count,self);" in flip
    assert SRC.count("focus_timeline_pulse(0,count,self);")==1
    assert SRC.count("focus_timeline_pulse(1,count,self);")==1
    assert "pause_at_flip_if_background();" not in blt
    assert "pause_at_flip_if_background();" in flip


def test_existing_optional_passive_window_events_are_separate_from_timeline():
    inp=(ROOT/"input_focus_trace.c").read_text(encoding="utf-8")
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_FOCUS_PASSIVE"' in inp
    assert "INPUT-FOCUS: PASSIVE ONLY - WndProc events/Flip snapshots active" in inp
    assert "INPUT-FOCUS: window subclass installed" in inp
    assert "case WM_ACTIVATEAPP:" in inp
    assert "case WM_KILLFOCUS:" in inp
    assert "case WM_SETFOCUS:" in inp
    assert "if(nbytes==1 && passive[0]=='1')" in inp
    assert "InterlockedExchange(&reacquire_enabled,0);" in inp
