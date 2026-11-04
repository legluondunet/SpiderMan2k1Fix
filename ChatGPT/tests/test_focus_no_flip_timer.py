"""Opt-in, same-thread, no-Flip focus rescue experiment.

Guard only: runtime behavior on Wine and native Windows must be validated
before enabling by default or merging into the stable branch.
"""
from pathlib import Path

SRC=(Path(__file__).resolve().parents[2]/"spiderman2k1fix.c").read_text(encoding="utf-8")
START=SRC.index("/* Experimental Wine/X11 no-Flip recovery.")
END=SRC.index("static HRESULT WINAPI hooked_flip(",START)
TIMER=SRC[START:END]
FLIP=SRC[END:SRC.index("SpiderDI8Retry_OnFlip(count);",END)]


def test_experiment_is_explicitly_opt_in_and_on_owner_thread():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_FOCUS_TIMER"' in TIMER
    assert "if(n!=1 || enabled[0]!='1')return;" in TIMER
    assert "GetWindowThreadProcessId(hwnd,NULL)!=GetCurrentThreadId()" in TIMER
    assert "SetTimer(hwnd,0,50u,pause_focus_timer_proc)" in TIMER
    assert "pause_focus_timer_state=0" in TIMER
    assert "InterlockedExchange(&pause_focus_timer_state,1);" in TIMER


def test_reuses_existing_pause_only_after_stalled_flip_and_foreground_loss():
    callback=TIMER[TIMER.index("static VOID CALLBACK pause_focus_timer_proc("):
                   TIMER.index("static void pause_focus_timer_try_arm(")]
    for required in (
        "message!=WM_TIMER", "hwnd!=game_hwnd",
        "timer_id!=pause_focus_timer_id",
        "InterlockedCompareExchange(&pause_on_focus_loss,0,0)!=1",
        "InterlockedCompareExchange(&pause_ever_foreground,0,0)!=1",
        "InterlockedCompareExchange(&pause_in_progress,0,0)!=0",
        "InterlockedCompareExchange(&pause_latched,0,0)!=0",
        "game_foreground",
        "if(since_flip<250u)return;",
        "pause_at_flip_if_background();",
    ):
        assert required in callback
    assert "pause_confirmed_focus_loss(hwnd)" in SRC
    assert "(DWORD)(now-pause_background_since)>=150" in SRC


def test_no_extra_thread_hook_focus_activation_or_fake_input():
    for forbidden in (
        "CreateThread(", "SuspendThread(", "SetForegroundWindow(",
        "SetFocus(", "SendInput(", "AttachThreadInput(",
        "SetWindowLongPtr(", "PostMessage(", "SendMessage(",
    ):
        assert forbidden not in TIMER
    assert "InterlockedExchange(&pause_focus_timer_last_flip_tick,(LONG)GetTickCount());" in FLIP
    assert "pause_focus_timer_try_arm(game_hwnd);" in FLIP
    assert FLIP.count("pause_at_flip_if_background();")==1


def test_timer_does_not_replace_paused_surface_or_movie_pause():
    assert "SpiderPauseSurface_Begin(" in SRC
    assert "SpiderFocusMedia_Pause();" in SRC
    assert "SpiderFocusDSound_Pause();" in SRC
    assert "SpiderPauseAny_Begin(hwnd)" in SRC
    assert 'FOCUS-TIMER: no-Flip candidate' in TIMER

def test_timer_callback_heartbeat_distinguishes_registration_from_delivery():
    assert "FOCUS-TIMER: CALLBACK heartbeat=" in TIMER
    assert "pause_focus_timer_heartbeat_reports<30u" in TIMER
    assert "pause_focus_timer_last_foreground!=game_foreground" in TIMER
    assert "(DWORD)(GetTickCount()-pause_focus_timer_last_heartbeat)>=30000u" in TIMER
    assert "fg=GetForegroundWindow();" in TIMER
    assert "game_foreground=(fg==hwnd);" in TIMER
    assert "if(debug_log_enabled" in TIMER
    # Count callbacks before the focus gates; a registered-but-never-delivered
    # WM_TIMER is otherwise impossible to distinguish from a foreground check.
    assert TIMER.index("FOCUS-TIMER: CALLBACK heartbeat=") < TIMER.index("if(since_flip<250u)return;")
