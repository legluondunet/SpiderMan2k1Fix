"""Prevent a game-thread deadlock between SDL3 Bink Return and the 3D key-up barrier.

A real physical Enter/Space must still wait for release; only an injected,
pending Return owned by this exact module/thread may be balanced early.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
QUICK = (ROOT / "quick_exit_f12.c").read_text(encoding="utf-8")
UI = (ROOT / "gamepad_ui_profile.c").read_text(encoding="utf-8")
HEADER = (ROOT / "gamepad_ui_profile.h").read_text(encoding="utf-8")

BARRIER = QUICK[
    QUICK.index("static void spider_newgame_enter_release_barrier("):
    QUICK.index("/* Minimal safety-only owner for F10Shortcut=0.")
]
RELEASE = UI[
    UI.index("int SpiderGamepadUI_ReleaseSyntheticReturnBefore3D("):
    UI.index("/* Live SDL3 Pause is the eighth profile action.")
]


def test_pending_bink_return_is_released_before_original_key_up_wait():
    assert '#include "gamepad_ui_profile.h"' in QUICK
    assert "int SpiderGamepadUI_ReleaseSyntheticReturnBefore3D(HWND game_window);" in HEADER
    assert "if(confirmed_movie4) {" in BARRIER
    assert "SpiderGamepadUI_ReleaseSyntheticReturnBefore3D(hwnd);" in BARRIER
    assert BARRIER.index("if(confirmed_movie4) {") < BARRIER.index("GetAsyncKeyState(VK_RETURN)")
    assert BARRIER.index("SpiderGamepadUI_ReleaseSyntheticReturnBefore3D(hwnd);") < BARRIER.index("GetAsyncKeyState(VK_RETURN)")
    assert "GetAsyncKeyState(VK_SPACE)" in BARRIER
    assert "enter_after || space_after" in BARRIER
    assert "GetTickCount()-began)<1500u" in BARRIER
    assert "if(!enter_after && !space_after)Sleep(40);" in BARRIER
    assert "F10Shortcut" not in RELEASE
    # The barrier itself must not synthesize a key or alter the game's state.
    for forbidden in ("SendInput(", "keybd_event(", "PostMessageA(",
                      "*SPIDER_NATIVE_CUTSCENE_SKIP_REQUEST="):
        assert forbidden not in BARRIER


def test_release_is_owned_return_only_on_the_same_foreground_game_thread():
    assert "static DWORD active_scan_thread=0;" in UI
    press = UI[UI.index("static void begin_scancode("):
               UI.index("int SpiderGamepadUI_ReleaseSyntheticReturnBefore3D(")]
    assert "active_scan_thread=GetCurrentThreadId();" in press
    assert "active_scan_thread=0;" in UI[
        UI.index("static int release_active_key(void)"):
        UI.index("static void begin_scancode(")
    ]
    for check in ("!configured", "!hwnd", "GetForegroundWindow()!=hwnd",
                  "!IsWindowVisible(hwnd)", "active_scan!=0x1cu",
                  "!active_scan_thread", "active_scan_thread!=GetCurrentThreadId()"):
        assert check in RELEASE
    assert "if(!release_active_key()) {" in RELEASE
    assert "KEYUP failed; physical Enter/Space safety wait retained" in RELEASE
    assert "BINK-4-HANDOFF pending synthetic Return released" in RELEASE
    assert "SendInput(" not in RELEASE
    assert "active_scan=0;" not in RELEASE
    assert "Sleep(" not in RELEASE
    assert "SPIDER_NATIVE_GAME_INPUT_UPDATE(" not in RELEASE


def test_first_game_input_still_calls_original_owner_exactly_once():
    mini = QUICK[
        QUICK.index("static void __cdecl spider_movie4_safety_only_game_input(void)"):
        QUICK.index("/* Follow-up to the two 31-Oct physical Cross/Square comparisons:")
    ]
    assert "spider_newgame_enter_release_barrier(1);" in mini
    assert mini.count("SPIDER_NATIVE_GAME_INPUT_UPDATE();")==1
    assert "movie4_input_release_state,2,1" in mini
    assert "SPIDER_GAME_BINK_ACTIVE==0u" in mini
    assert "SPIDER_BINK_VIDEO_HANDLE==0u" in mini
    # Native input updates stay out of helper functions.
    assert "SPIDER_NATIVE_GAME_INPUT_UPDATE();" not in RELEASE


def test_actual_bink_interaction_is_not_repurposed_for_f10_or_non_bink():
    # The original F10 opt-in and its state machine are untouched.
    assert "if(!confirmed_movie4) {" in BARRIER
    assert "debug_auto3d_f10_origin,0,0)!=0" in BARRIER
    assert "debug_auto3d_state,0,0)!=0" in BARRIER
    assert "movie4_input_release_state,2,1" in QUICK
    assert "release_active_key();" in UI[
        UI.index("void SpiderGamepadUI_OnFlip("):
        UI.index("/* An SDL3 selected canonical native map")
    ]
