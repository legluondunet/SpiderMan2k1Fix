"""Source-level checks for the optional passive Win32 focus-message comparison.

Runtime behavior under Wine/X11 or native Windows requires user testing.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MOD = (ROOT / "win32_focus_message_trace.c").read_text()
HDR = (ROOT / "win32_focus_message_trace.h").read_text()
CORE = (ROOT / "spiderman2k1fix.c").read_text()
PATH = (ROOT / "pause_path_trace.c").read_text()
BUILD = (ROOT / "build.sh").read_text()


def test_only_explicit_double_opt_in_with_debug_log():
    assert 'env_one("SPIDERMAN2K1FIX_PAUSE_PATH_TRACE")' in MOD
    assert 'env_one("SPIDERMAN2K1FIX_WIN32_FOCUS_MESSAGE_TRACE")' in MOD
    assert "if(!debug_enabled ||" in MOD
    assert "static volatile LONG enabled=0;" in MOD
    assert "SpiderWin32FocusTrace_Init(log_line,debug_log_enabled);" in CORE
    assert "win32_focus_message_trace.c" in BUILD


def test_hooks_are_thread_local_and_leave_original_wndproc_untouched():
    assert "GetWindowThreadProcessId(hwnd,NULL)" in MOD
    assert "SetWindowsHookExA(WH_CALLWNDPROC,focus_before_hook,module,thread)" in MOD
    assert "SetWindowsHookExA(WH_CALLWNDPROCRET,focus_after_hook,module,thread)" in MOD
    assert "return CallNextHookEx(before_hook,code,wp,lp);" in MOD
    assert "return CallNextHookEx(after_hook,code,wp,lp);" in MOD
    assert "if(data->hwnd==watched_hwnd)" in MOD
    assert "SetWindowLongPtr" not in MOD
    assert "SetWindowSubclass" not in MOD
    assert "CallWindowProc" not in MOD


def test_observes_focus_messages_without_input_or_pause_side_effects():
    for name in ("WM_ACTIVATEAPP","WM_KILLFOCUS","WM_SETFOCUS",
                 "WM_ACTIVATE","WM_NCACTIVATE"):
        assert name in MOD
    for forbidden in (
        "SuspendThread(", "ResumeThread(", "SetForegroundWindow(",
        "SetFocus(", "SetActiveWindow(", "SendInput(", "Acquire(",
        "Unacquire(", "DispatchMessage(", "PeekMessage(",
        "MsgWaitForMultipleObjectsEx(", "WaitForSingleObject(",
        "Sleep(", "VirtualProtect(", "WriteProcessMemory(",
    ):
        assert forbidden not in MOD
    assert 'WIN32-FOCUS: EVENT %s %s wall=%s tick=' in MOD
    assert "void SpiderWin32FocusTrace_AttachWindow(HWND hwnd);" in HDR
    assert "SpiderWin32FocusTrace_AttachWindow(hwnd);" in CORE
    assert CORE.index("SpiderWin32FocusTrace_AttachWindow(hwnd);") > CORE.index(
        "SpiderPausePath_AttachWindow(hwnd);")


def test_heartbeat_comparison_even_when_win32_reports_foreground():
    assert "SpiderWin32FocusTrace_Heartbeat(hwnd,now);" in PATH
    assert "GetGUIThreadInfo(game_thread,&gui)" in MOD
    assert "GetLocalTime(&st)" in MOD
    assert "WIN32-FOCUS: HEARTBEAT wall=" in MOD
    assert "if(last_heartbeat && (DWORD)(now-last_heartbeat)<5000UL)return;" in MOD


def test_existing_pause_and_input_retry_untouched():
    assert "if(SpiderFocusEventPause_IsEnabled())" in CORE
    assert "pause_at_flip_if_background();" in CORE
    assert "SpiderDI8Retry_OnFlip(count);" in CORE
    assert "SpiderInputFocus_OnFlip(count);" in CORE
    assert "SpiderWin32FocusTrace" not in CORE[
        CORE.index("static HRESULT WINAPI hooked_flip("):
        CORE.index("static HRESULT WINAPI hooked_flip(")+2300
    ]
