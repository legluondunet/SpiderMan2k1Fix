"""Bink-to-3D handoff: only SDL3 may synthesize menu input.

With the old PS1/Xbox UI producer gone, its return-release entry is a
no-op; original physical Enter/Space must still be released before the
retail engine advances. The original Bink #4 ownership gate stays intact.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
QUICK=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
UI=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
PROBE=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
HEADER=(ROOT/"gamepad_ui_profile.h").read_text(encoding="utf-8")
BARRIER=QUICK.split("static void spider_newgame_enter_release_barrier(",1)[1].split(
    "/* Minimal safety-only owner for F10Shortcut=0.",1)[0]


def test_original_gamebarrier_retains_physical_enter_and_space_guard():
    assert "SpiderGamepadUI_ReleaseSyntheticReturnBefore3D(hwnd);" in BARRIER
    assert "GetAsyncKeyState(VK_RETURN)" in BARRIER
    assert "GetAsyncKeyState(VK_SPACE)" in BARRIER
    assert "enter_after || space_after" in BARRIER
    assert "GetTickCount()-began)<1500u" in BARRIER
    for forbidden in ("SendInput(", "keybd_event(", "PostMessageA("):
        assert forbidden not in BARRIER


def test_removed_old_ui_synthetic_return_cannot_steal_sdl3_owner():
    assert "int SpiderGamepadUI_ReleaseSyntheticReturnBefore3D(HWND hwnd)" in UI
    assert "  (void)hwnd;return 0;" in UI
    assert "begin_scancode(" not in UI
    assert "active_scan_thread" not in UI
    assert "SpiderSDL3ControllerSetup_Init(ini,logger);" in UI
    assert "SpiderSDL3_InstallDirectJoystickEntry(ui_log);" in UI
    assert "SpiderGamepadUI_ReleaseSyntheticReturnBefore3D" in HEADER


def test_scripted_bink_south_wait_uses_live_selected_sdl3_id():
    assert "int SpiderSDL3Probe_NewGameBinkWaitSouthRelease(HWND hwnd)" in PROBE
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in PROBE
    assert "sdl_button(pad,0)" in PROBE
    assert "GetTickCount()" in PROBE
    assert "GetForegroundWindow()!=hwnd" in PROBE
    assert "IDirectInputDevice8_SetDeviceData" not in PROBE


def test_first_3d_callback_preserves_single_original_gameinput():
    mini=QUICK.split("static void __cdecl spider_movie4_safety_only_game_input(void)",1)[1].split(
       "/* Follow-up to the two 31-Oct physical Cross/Square comparisons:",1
    )[0]
    assert mini.count("SPIDER_NATIVE_GAME_INPUT_UPDATE();")==1
    assert "spider_newgame_enter_release_barrier(1);" in mini
    assert "movie4_input_release_state,2,1" in mini
