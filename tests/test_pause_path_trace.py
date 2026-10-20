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


def test_extended_paths_are_opt_in_besides_existing_trace_flag():
    assert "SPIDERMAN2K1FIX_EXTENDED_PRESENT_TRACE" in MOD
    assert 'if(n==1 && env[0]==\'1\') {' in MOD
    assert "SpiderPausePath_ExtendedEnabled" in HDR
    assert "if(SpiderPausePath_ExtendedEnabled())" in CORE
    assert "static volatile LONG extended_enabled=0;" in MOD
    assert "if(!debug_enabled || n!=1 || env[0]!='1')return;" in MOD


def test_surface_vtable_slots_match_microsoft_ddraw7_and_passthrough():
    # SDK order: BltBatch=6 BltFast=7 GetDC=17 Lock=26 ReleaseDC=27
    # SetOverlayPosition=31 Unlock=33 UpdateOverlay=34..36.
    names = {
        6: "blt_batch", 7: "blt_fast", 17: "get_dc",
        26: "lock", 27: "release_dc", 31: "set_overlay_position",
        33: "unlock", 34: "update_overlay", 35: "update_overlay_display",
        36: "update_overlay_z_order",
    }
    for slot, name in names.items():
        assert f"original_{name}=" in CORE
        assert f"vtable[{slot}]" in CORE
        assert f"patched_surface_vtable[{slot}]=(void*)traced_{name};" in CORE
        assert f"return original_{name}(" in CORE
    # All new surface hooks are installed only in the opt-in branch.
    branch = CORE.index("if(SpiderPausePath_ExtendedEnabled())", CORE.index("static void instrument_surface("))
    for name in names.values():
        assert CORE.index("patched_surface_vtable[", branch) < len(CORE)
    assert "PAUSE-PATH: EXTENDED Surface7 wrappers installed" in CORE


def test_alternate_d3d7_hooks_relay_unmodified_arguments():
    names = {
        5: "begin_scene", 6: "end_scene",
        29: "draw_strided", 30: "draw_indexed_strided",
        31: "draw_vb", 32: "draw_indexed_vb",
    }
    for slot,name in names.items():
        assert f"original_{name}=(D3D" in CORE
        assert f"vt[{slot}]" in CORE
        assert f"vt[{slot}]=(void*)traced_{name};" in CORE
        assert f"return original_{name}(" in CORE
    assert "if(SpiderPausePath_ExtendedEnabled())" in CORE
    assert "PAUSE-PATH: EXTENDED D3D7 passthrough wrappers" in CORE


def test_extended_trace_counts_report_from_separate_observer():
    for api in ("BltFast", "BltBatch", "Lock", "Unlock", "GetDC",
                "ReleaseDC", "UpdateOverlay", "SetOverlayPosition",
                "DrawPrimitiveStrided", "DrawIndexedPrimitiveStrided",
                "DrawPrimitiveVB", "DrawIndexedPrimitiveVB",
                "BeginScene", "EndScene"):
        assert api in MOD
        assert api in CORE
    assert "PAUSE-PATH: EXTENDED SNAPSHOT" in MOD
    assert "if(SpiderPausePath_ExtendedEnabled())" in MOD
