"""Passive Alt+Tab graphics-path instrumentation; source-level safety checks.

These tests do not prove runtime rendering or audio behavior under Wine/Windows.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "spiderman2k1fix.c").read_text()
MOD = (ROOT / "pause_path_trace.c").read_text()
HDR = (ROOT / "pause_path_trace.h").read_text()
BUILD = (ROOT / "build.sh").read_text()


def test_diagnostics_opt_in_and_require_logging():
    assert "SPIDERMAN2K1FIX_PAUSE_PATH_TRACE" in MOD
    assert "if(!debug_enabled || n!=1 || env[0]!='1')return;" in MOD
    assert "PAUSE-PATH: passive background" in MOD
    assert "SpiderPausePath_Init(log_line,debug_log_enabled);" in CORE


def test_recording_only_when_out_of_focus_with_rate_limit():
    assert "GetForegroundWindow()==hwnd)return;" in MOD
    assert "(DWORD)(now-(DWORD)old)<1000" in MOD
    assert "InterlockedCompareExchange(&counter->last_tick" in MOD
    for api in ("Blt", "DrawPrimitive", "DrawIndexedPrimitive"):
        assert api in MOD
        assert 'SpiderPausePath_Record("'+api+'"' in CORE


def test_gate_timestamps_surround_existing_pause_selection():
    before = CORE.index("trace_entered=SpiderPausePath_FlipEnter(")
    first = CORE.index("if(SpiderFocusEventPause_IsEnabled())", before)
    second = CORE.index("pause_at_flip_if_background();", first)
    after = CORE.index("SpiderPausePath_FlipLeave(",second)
    assert before < first < second < after
    assert "PAUSE-PATH: background Flip pause-check ENTER" in MOD
    assert "PAUSE-PATH: Flip pause-check EXIT elapsed-ms=" in MOD


def test_tracing_never_changes_game_state_or_waits():
    forbidden = (
        "SuspendThread(", "ResumeThread(", "SetForegroundWindow(",
        "SendInput(", "DirectInput8Create", "Acquire(", "Unacquire(",
        "PeekMessage(", "MsgWaitForMultipleObjects",
        "SetWindowLong", "VirtualProtect(", "WriteProcessMemory(",
    )
    for forbidden_call in forbidden:
        assert forbidden_call not in MOD
    # Only the opt-in observer thread may sleep; never stall the game's
    # intercepted rendering call on a timer or window-message loop.
    render_paths = MOD[MOD.index("void SpiderPausePath_Record("):]
    assert "Sleep(" not in render_paths
    assert "WaitForSingleObject(" not in render_paths
    assert "void SpiderPausePath_Init" in HDR
    assert '#include "pause_path_trace.h"' in CORE
    assert "pause_path_trace.c" in BUILD


def test_passive_observer_tracks_focus_independently_of_flip():
    assert "SPIDERMAN2K1FIX_PAUSE_PATH_TRACE" in MOD
    assert "CreateThread(NULL,0,observe_foreground" in MOD
    assert "PAUSE-PATH: FOCUS LOST tick=" in MOD
    assert "PAUSE-PATH: FOCUS RESTORED tick=" in MOD
    assert "PAUSE-PATH: FOCUS BACKGROUND elapsed-ms=" in MOD
    assert "focus-loss-to-gate-ms=%ld" in MOD
    assert "Sleep(25);" in MOD
    assert "SpiderPausePath_AttachWindow(hwnd);" in CORE
    assert "void SpiderPausePath_AttachWindow(HWND hwnd);" in HDR
    assert CORE.index("SpiderPausePath_AttachWindow(hwnd);") > CORE.index(
        "SpiderFocusEventPause_AttachWindow(hwnd);")
