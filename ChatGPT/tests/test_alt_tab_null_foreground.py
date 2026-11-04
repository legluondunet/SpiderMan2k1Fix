"""Wine/X11 Alt+Tab: never confuse a transient NULL foreground with a real loss.

These static guard tests protect the existing Flip-only focus policy. The
Windows/Wine behavior itself still requires real gameplay validation.
"""
from pathlib import Path

CORE=(Path(__file__).resolve().parents[2]/"spiderman2k1fix.c").read_text(encoding="utf-8")
HELPER=CORE[CORE.index("static BOOL pause_confirmed_null_foreground_loss(HWND hwnd)"):
            CORE.index("static BOOL pause_confirmed_focus_loss(HWND hwnd)")]
PREDICATE=CORE[CORE.index("static BOOL pause_confirmed_focus_loss(HWND hwnd)"):
               CORE.index("/* Wake only with the foreground Any-key hook. */")]


def test_null_foreground_needs_a_valid_inactive_game_gui_thread():
    assert "GetWindowThreadProcessId(hwnd,NULL)" in HELPER
    assert "if(!tid)return FALSE;" in HELPER
    assert "gi.cbSize=sizeof(gi);" in HELPER
    assert "if(!GetGUIThreadInfo(tid,&gi))return FALSE;" in HELPER
    assert "gi.hwndActive==NULL && gi.hwndFocus==NULL" in HELPER
    for forbidden in ("SetForegroundWindow(", "SetActiveWindow(", "SetFocus(",
                      "AttachThreadInput(", "ShowWindow(", "SendInput("):
        assert forbidden not in HELPER


def test_null_foreground_and_foreign_process_share_one_150ms_debounce():
    assert "if(fg==hwnd)" in PREDICATE
    assert "pause_background_since=0" in PREDICATE
    assert "(fg && (!fg_pid || fg_pid==game_pid))" in PREDICATE
    assert "(!fg && !pause_confirmed_null_foreground_loss(hwnd))" in PREDICATE
    assert "(DWORD)(now-pause_background_since)>=150" in PREDICATE
    assert "null-foreground-candidate-first-flip" in PREDICATE
    assert "null-foreground-confirmed-after-150ms" in PREDICATE
    assert "external-candidate-confirmed-after-150ms" in PREDICATE


def test_no_new_hooks_or_forced_activation_for_null_foreground():
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "pause_confirmed_focus_loss(hwnd)" in pause
    assert "SpiderPauseAny_Begin(hwnd)" in pause
    assert "SpiderPauseSurface_Begin(" in pause
    assert "while(IsWindow(hwnd) && InterlockedCompareExchange(&pause_latched,0,0)==1)" in pause
    for forbidden in ("SetForegroundWindow(", "AttachThreadInput(", "SendInput("):
        assert forbidden not in pause
