"""The Sony DS4 2D Enter profile is isolated from native gamepad gameplay.

The old BUTTON8/native Start table rewrite was abandoned after the user
reported failure and the second log showed a strict PE-header refusal.
Our new test checks the exact shipped gamepad implementation.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAIN = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
PROFILE = (ROOT / "gamepad_ui_profile.c").read_text(encoding="utf-8")
HEADER = (ROOT / "gamepad_ui_profile.h").read_text(encoding="utf-8")
BUILD = (ROOT / "build.sh").read_text(encoding="utf-8")
CFG = (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
INI = (ROOT / "ini_validation.h").read_text(encoding="utf-8")


def test_profile_is_automatic_for_verified_sony_dualshock4_only():
    for label in ("0x054cu", "0x05c4u", "0x09ccu", "0x0ba0u"):
        assert label in PROFILE
    for gate in ("d.found!=1u", "d.ds4_count!=1u", "GUID", "DirectInput8Create",
                 "IDirectInput8_EnumDevices", "DIEDFL_ATTACHEDONLY"):
        assert gate in PROFILE
    assert "PROFILE DualShock4 ACTIVE" in PROFILE
    assert "no unique verified DualShock4" in PROFILE
    assert "ControllerProfile=Auto" in CFG
    assert "ControllerEnterButtons=Both" in CFG
    assert "ControllerBackButton=Circle" in CFG
    assert "ControllerPauseButton=Options" in CFG
    assert '"ControllerProfile","ControllerEnterButtons","ControllerBackButton","ControllerPauseButton"' in INI


def test_input_profile_preserves_original_gamepad_and_never_hooks_com():
    assert "gamepad_ui_profile.c" in BUILD
    assert "gamepad_start_native.c" not in BUILD
    assert 'gamepad_ui_profile.h' in MAIN
    assert 'gamepad_start_native.h' not in MAIN
    assert "SpiderGamepadUI_Init(input_ini,log_line)" in MAIN
    assert "SpiderGamepadUI_OnFlip(game_hwnd" in MAIN
    for forbidden in ("SetWindowsHookEx(", "keybd_event(",
                      "IDirectInputDevice8_GetDeviceData(",
                      "SetWindowLongPtr", "VirtualProtect(", "InterlockedExchange((volatile LONG*)",
                      "SpiderGamepad_MappingRecognized("):
        assert forbidden not in PROFILE
    assert "SpiderGamepadStart_Init" not in MAIN


def test_ui_enter_is_not_injected_into_ordinary_3d_or_background():
    assert "menu_allowed=(menu_2d && !gameplay_3d)" in PROFILE
    assert "movie_allowed=bink_active!=0" in PROFILE
    assert "confirm_allowed=menu_allowed || movie_allowed" in PROFILE
    assert "gameplay_allowed=gameplay_3d && !menu_2d && !movie_allowed" in PROFILE
    assert "GetForegroundWindow()!=hwnd" in PROFILE
    assert "else if(confirm_allowed &&" in PROFILE
    assert "release_active_key();" in PROFILE
    assert "KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP" in PROFILE
    assert 'begin_scancode(0x1Cu,"Return");' in PROFILE
    assert 'begin_scancode(0x01u,"Escape");' in PROFILE
    assert "SendInput(1,&in,sizeof(in))" in PROFILE
    assert "previous_buttons" in PROFILE
    assert "previous_back" in PROFILE
    assert "active_scan=scan" in PROFILE
    assert "in.ki.wScan=(WORD)active_scan" in PROFILE
    assert "if(!active_scan)return;" in PROFILE
    # Read hardware with our own (nonexclusive) DI object, not the game's.
    assert "DISCL_FOREGROUND|DISCL_NONEXCLUSIVE" in PROFILE
    assert "IDirectInputDevice8_GetDeviceState" in PROFILE
    assert "IDirectInputDevice8_Poll" in PROFILE
    # A synthetic key must be released before Flip can pause on focus loss.
    flip = MAIN.split("static HRESULT WINAPI hooked_flip(", 1)[1].split(
        "static void install_ddraw_hook(", 1
    )[0]
    assert flip.index("SpiderGamepadUI_OnFlip(game_hwnd") < flip.index(
        "pause_at_flip_if_background()")


def test_known_ds4_buttons_and_honest_compatibility_limits():
    assert "state.rgbButtons[1]" in PROFILE   # Cross, zero-based
    assert "state.rgbButtons[9]" in PROFILE   # Options, zero-based
    assert "state.rgbButtons[2]" in PROFILE   # Circle, zero-based
    assert "state.rgbButtons[8]" in PROFILE   # Share, optional zero-based
    assert "ControllerEnterButtons" in INI
    assert "ControllerBackButton" in INI
    assert "KEYEVENTF_SCANCODE" in PROFILE
    assert "still need" in PROFILE or "still experimental" in HEADER
    assert "ExperimentalGamepadStart" in PROFILE
    assert "old ExperimentalGamepadStart is RETIRED/ignored" in PROFILE


def test_circle_back_has_precedence_and_never_overlaps_gameplay():
    # This check prevents a regression to injecting Enter+Escape in the same
    # 2D frame. The original native Triangle confirm remains untouched.
    assert 'GetPrivateProfileStringA("Input","ControllerBackButton","Circle"' in PROFILE
    assert 'if(!_stricmp(back_choice,"Off"))' in PROFILE
    assert 'else if(!_stricmp(back_choice,"Circle"))' in PROFILE
    assert 'else if(!_stricmp(back_choice,"Share"))' in PROFILE
    assert "back_pressed=(state.rgbButtons[2]&0x80u?SPIDER_BACK_CIRCLE:0)" in PROFILE
    assert "if(menu_allowed && (back_pressed & (unsigned)back)" in PROFILE
    escape = PROFILE.index('begin_scancode(0x01u,"Escape");')
    enter = PROFILE.index('begin_scancode(0x1Cu,"Return");')
    assert escape < enter
    assert "previous_back=(int)back_pressed;" in PROFILE
    assert "Triangle" in PROFILE or "TRIANGLE" in PROFILE
    assert "state.rgbButtons[3]" not in PROFILE
    assert "KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP" in PROFILE
    assert "gameplay_3d" in PROFILE


def test_native_movie_flag_not_viewport_is_used_after_main_menu():
    assert "0x006151F8u" in MAIN
    assert "SpiderGamepadUI_OnFlip(game_hwnd" in MAIN
    assert "InterlockedCompareExchange(&fov_game_height,0,0)>0" in MAIN
    assert "(*(const volatile DWORD *)(uintptr_t)0x006151F8u)!=0u" in MAIN
    assert "gameplay_4x3_active,0,0)==1);" not in MAIN.split(
        "SpiderGamepadUI_OnFlip(game_hwnd", 1
    )[1][:320]
    assert "movie_allowed=bink_active!=0" in PROFILE
    assert "confirm_allowed=menu_allowed || movie_allowed" in PROFILE
    assert 'begin_scancode(0x1Cu,"Return");' in PROFILE
    assert "GAMEPAD-UI: POST-MENU BINK Return attempted" in PROFILE
    assert "movie_allowed && !menu_allowed" in PROFILE


def test_options_3d_pause_checks_entire_original_gamepad_button_map():
    # Original game binds BUTTON9/OPTIONS to native Zipline by default.
    # The fix MUST NOT inject Escape and trigger Zipline in one 3D frame.
    assert "native_factory_buttons[7]={3u,4u,0u,1u,6u,9u,7u}" in PROFILE
    assert '"Jump","Kick","Punch","Web","Aim","Zipline","Swing"' in PROFILE
    assert "button==9u" in PROFILE
    assert "if(!parse_native_button(value,&button) || button==9u)" in PROFILE
    assert 'GetPrivateProfileStringA("Joystick",native_actions[i],fallback' in PROFILE
    assert "PAUSE BLOCKED:" in PROFILE
    assert "options_free_of_native_actions()" in PROFILE
    assert 'pause_button==SPIDER_PAUSE_OPTIONS' in PROFILE
    assert "gameplay_allowed && pause_button" in PROFILE
    assert "pause_pressed && !previous_pause" in PROFILE
    assert 'ControllerPauseButton","Options"' in PROFILE
    assert "ControllerPauseButton=Options" in CFG
    assert "ControllerPauseButton" in INI
    assert "state.rgbButtons[3]" not in PROFILE  # native Triangle untouched
    assert "VirtualProtect(" not in PROFILE
