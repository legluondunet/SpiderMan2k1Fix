"""Opt-in focus-event pause prototype does not disturb stable Windows/Wine defaults.

These are source-level safety regression guards, NOT native Windows/Wine
runtime validation. See FOCUS_EVENT_PAUSE_PROTOTYPE.md for real-game tests.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "spiderman2k1fix.c").read_text()
PAUSE = (ROOT / "focus_event_pause.c").read_text()
HEADER = (ROOT / "focus_event_pause.h").read_text()
INI = (ROOT / "SpiderMan2k1Fix.ini").read_text()
GENERATED = (ROOT / "virtual_spidey_cfg.c").read_text()
VALID = (ROOT / "ini_validation.h").read_text()
BUILD = (ROOT / "build.sh").read_text()
GUIDE = (ROOT / "FOCUS_EVENT_PAUSE_PROTOTYPE.md").read_text()


def test_focus_event_prototype_requires_explicit_ini_opt_in():
    assert "ExperimentalFocusEventPause=0" in INI
    assert '"ExperimentalFocusEventPause=0\\r\\n"' in GENERATED
    assert '"ExperimentalFocusEventPause"' in VALID
    assert 'GetPrivateProfileIntA("Input","ExperimentalFocusEventPause",0,path)==1' in PAUSE
    assert "InterlockedExchange(&enabled,1)" in PAUSE
    assert "if(!want) return;" in PAUSE


def test_prototype_never_directly_intercepts_or_operates_directinput():
    for forbidden in ("DirectInput8Create", "IDirectInputDevice", "Acquire(",
                      "Unacquire(", "SuspendThread", "SetForegroundWindow(",
                      "ShowCursor(", "SendInput(", "keybd_event(", "mouse_event("):
        assert forbidden not in PAUSE
    assert '"ExperimentalAltTabReacquire",0,path)==1' in PAUSE
    assert '"ExperimentalFocusTrace",0,path)==1' in PAUSE
    assert "incompatible with experimental DirectInput focus hooks" in PAUSE


def test_focus_observer_only_sets_state_never_waits():
    start = PAUSE.index("static LRESULT CALLBACK focus_pause_proc(")
    end = PAUSE.index("void SpiderFocusEventPause_Init(", start)
    proc = PAUSE[start:end]
    assert "WM_ACTIVATEAPP" in proc
    assert "WM_KILLFOCUS" in proc
    assert "WM_SETFOCUS" in proc
    for forbidden in ("MsgWaitForMultipleObjectsEx(", "PeekMessageA(",
                      "Sleep(", "Acquire(", "DispatchMessageA("):
        assert forbidden not in proc
    assert "CallWindowProcA(chained,hwnd,msg,wp,lp)" in proc
    assert "WM_NCDESTROY" in proc


def test_safe_flip_gate_pumps_messages_and_retains_wm_quit():
    start = PAUSE.index("void SpiderFocusEventPause_OnFlip(void)")
    gate = PAUSE[start:]
    for needle in ("PeekMessageA(&msg,NULL,0,0,PM_REMOVE)",
                   "TranslateMessage(&msg)", "DispatchMessageA(&msg)",
                   "MsgWaitForMultipleObjectsEx(0,NULL,50,QS_ALLINPUT,MWMO_INPUTAVAILABLE)",
                   "PostQuitMessage((int)msg.wParam)"):
        assert needle in gate
    assert "GetForegroundWindow()!=hwnd" in gate
    assert "FOCUS-EVENT" in PAUSE
    assert "GetTickCount()" in PAUSE


def test_legacy_flip_pause_is_retained_and_not_nested_with_prototype():
    assert "static void pause_at_flip_if_background(void)" in CORE
    assert "    if(SpiderFocusEventPause_IsEnabled())" in CORE
    assert "      SpiderFocusEventPause_OnFlip();" in CORE
    assert "    else\n      pause_at_flip_if_background();" in CORE
    assert CORE.count("pause_at_flip_if_background();")==1
    assert "PauseOnFocusLoss=0" in INI
    assert '"PauseOnFocusLoss=0\\r\\n"' in GENERATED
    assert "focus_event_pause.c" in BUILD
    assert '#include "focus_event_pause.h"' in CORE
    assert "SpiderFocusEventPause_AttachWindow(hwnd);" in CORE
    assert "SpiderFocusEventPause_Init(log_line);" in CORE


def test_docs_disclose_not_real_engine_pause_and_require_both_platforms():
    assert "not yet a true immediate" in GUIDE
    assert "Wine 11.17/X11" in GUIDE
    assert "Windows 11" in GUIDE
    assert "DirectInput" in GUIDE
    assert "do not" in GUIDE.lower()
