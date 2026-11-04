"""Experimental Wine/X11 app-deactivation -> next-Flip PAUSED fallback.

The real-time Windows message delivery must still be tested under Wine and
native Windows; this file protects fail-closed behavior and no duplicate pause.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
FOCUS=(ROOT/"input_focus_trace.c").read_text(encoding="utf-8")
HEADER=(ROOT/"input_focus_trace.h").read_text(encoding="utf-8")
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
EVENTS=FOCUS[FOCUS.index("static void deferred_focus_event("):
             FOCUS.index("BOOL SpiderInputFocus_ConsumeDeferredPause(")]
PAUSE=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
           CORE.index("static HRESULT WINAPI hooked_flip(")]


def test_opt_in_without_new_directinput_or_default_window_subclass():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_FOCUS_RETURN_LATCH"' in FOCUS
    assert "n==1 && return_latch[0]=='1'" in FOCUS
    assert "static volatile LONG deferred_pause_enabled=0;" in FOCUS
    assert "if(InterlockedCompareExchange(&deferred_pause_enabled,0,0)==1 &&" in FOCUS
    assert "FOCUS-RETURN: opt-in WndProc focus-event tracking active; no DI8 interception" in FOCUS
    assert "if(!want_trace && !want_fix && want_cursor)" in FOCUS


def test_deferred_latch_only_after_app_deactivation_and_150ms():
    assert "msg!=WM_ACTIVATEAPP" in EVENTS
    assert "if(wp==FALSE)" in EVENTS
    assert "InterlockedExchange(&deferred_away_active,1);" in EVENTS
    assert "if(InterlockedExchange(&deferred_away_active,0)!=1)return;" in EVENTS
    assert "if(elapsed<150u)" in EVENTS
    assert "InterlockedExchange(&deferred_return_pending,1);" in EVENTS
    assert "deferred_focus_event(msg,wp);" in FOCUS
    assert "FOCUS-RETURN: sustained app deactivation" in EVENTS


def test_original_foreground_detection_remains_flip_primary():
    assert "pause_confirmed_focus_loss(hwnd)" in PAUSE
    assert 'FOCUS-PAUSE: confirmed external focus loss' in PAUSE
    assert "SpiderInputFocus_DiscardDeferredPause();" in PAUSE
    assert PAUSE.index("SpiderInputFocus_DiscardDeferredPause();") < PAUSE.index("SpiderPauseAny_Begin(hwnd)")


def test_only_game_foreground_flip_consumes_one_deferred_request():
    active=PAUSE[PAUSE.index("if(GetForegroundWindow()==hwnd)"):
                 PAUSE.index("} else if(InterlockedCompareExchange(&pause_ever_foreground")]
    assert "InterlockedCompareExchange(&pause_ever_foreground,0,0)==1" in active
    assert "InterlockedCompareExchange(&pause_latched,0,0)==0" in active
    assert "SpiderInputFocus_ConsumeDeferredPause(&deferred_elapsed)" in active
    assert "InterlockedCompareExchange(&pause_latched,1,0)==0" in active
    assert "FOCUS-RETURN: deferred PAUSED latch at first returned Flip" in active
    assert "SpiderPauseSurface_Begin(" in PAUSE
    assert "SpiderFocusDSound_Pause();" in PAUSE
    assert "SpiderFocusMedia_Pause();" in PAUSE
    assert "SpiderPauseAny_Begin(hwnd)" in PAUSE
    assert "SpiderInputFocus_ConsumeDeferredPause(DWORD *elapsed_ms)" in HEADER
    assert "SpiderInputFocus_DiscardDeferredPause(void)" in HEADER


def test_wndproc_never_performs_off_thread_render_or_native_pause():
    observer=FOCUS[FOCUS.index("static void deferred_focus_event("):
                   FOCUS.index("static void focus_report_window(")]
    for forbidden in ("SetForegroundWindow(", "SetFocus(", "SendInput(",
                      "BinkPause(", "SpiderPauseSurface_Begin(", "SuspendThread(",
                      "IDirectDrawSurface7_", "CreateThread("):
        assert forbidden not in observer
    assert "BOOL SpiderInputFocus_ConsumeDeferredPause" in observer
    assert "void SpiderInputFocus_DiscardDeferredPause" in observer


def test_opt_in_wndproc_brackets_native_handler_and_tracks_completion():
    assert "static volatile LONG deferred_wndproc_report_count=0;" in FOCUS
    window=FOCUS[FOCUS.index("static LRESULT CALLBACK focus_window_proc("):
                 FOCUS.index("void SpiderInputFocus_AttachWindow(")]
    assert "InterlockedCompareExchange(&deferred_pause_enabled,0,0)==1" in window
    assert "InterlockedIncrement(&deferred_wndproc_report_count)<=32" in window
    assert '"FOCUS-RETURN: ORIGINAL-WNDPROC enter msg=%s' in window
    assert '"FOCUS-RETURN: ORIGINAL-WNDPROC returned msg=%s' in window
    assert "forwarded_result=previous_proc ? CallWindowProcA(previous_proc,hwnd,msg,wp,lp)" in window
    assert window.index("ORIGINAL-WNDPROC enter") < window.index("CallWindowProcA(previous_proc,hwnd,msg,wp,lp)") < window.index("ORIGINAL-WNDPROC returned")
    assert "return forwarded_result;" in window


def test_opt_in_focus_event_traces_do_not_require_debug_mode():
    assert "if(input_debug_logging && deferred_reports++<16)" not in EVENTS
    assert EVENTS.count("if(deferred_reports++<16)") == 3
    assert "FOCUS-RETURN: WM_ACTIVATEAPP(FALSE) recorded" in EVENTS
    assert "FOCUS-RETURN: sustained app deactivation" in EVENTS
