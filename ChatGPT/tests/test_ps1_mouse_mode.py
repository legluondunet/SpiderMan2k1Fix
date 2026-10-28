"""Global game-only mouse policy must not depend on a DualShock 4.

Retail Spider-Man PC 2001 uses mouse for menu navigation only. With
MouseEnabled=0 by default, its own render/poll/hit-test gates are disabled;
the actual desktop cursor and all keyboard input remain untouched.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MODE=(ROOT/"ps1_mouse_mode.c").read_text()
HEADER=(ROOT/"ps1_mouse_mode.h").read_text()
BRIDGE=(ROOT/"gamepad_ui_profile.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
BUILD=(ROOT/"build.sh").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_mouse_disabled_by_default_independent_of_controller_with_user_override():
    assert '"MouseEnabled"' in VALID
    assert 'GetPrivateProfileIntA("Input","MouseEnabled",0,ini)==1' in BRIDGE
    assert 'SpiderPS1Mouse_Configure(GetPrivateProfileIntA(' in BRIDGE
    assert '"MouseEnabled=0\\r\\n"' in CFG
    assert '"MouseEnabled=1\\r\\n"' not in CFG
    assert 'SpiderPS1Mouse_OnFlip(hwnd);' in BRIDGE
    assert BRIDGE.index("SpiderPS1Mouse_OnFlip(hwnd);") < BRIDGE.index("if(!configured || !hwnd || !IsWindow(hwnd))")
    assert 'SpiderPS1Mouse_Configure(int mouse_enabled' in MODE
    assert 'mouse_policy_enabled=mouse_enabled!=0' in MODE
    assert "MouseEnabled=1; original game menu mouse enabled" in MODE
    assert "MouseEnabled=0 (default)" in MODE
    # Without a pad / with ControllerProfile=Off, the shared policy still runs.
    assert "if(!detected||!device)" in BRIDGE
    assert "if(!mouse_policy_configured)return;" in MODE
    assert "SpiderPS1Mouse_SetPadActive(1);" in MODE
    assert "if(armed)SpiderPS1Mouse_SetPadActive(1);" in MODE
    assert "ps1_mouse_auto_requested" not in BRIDGE
    assert "ps1_mouse_auto_armed" not in BRIDGE


def test_legacy_ds4_mouse_option_remains_accepted_but_is_retired():
    assert '"ControllerAutoDisableMouse"' in VALID
    assert 'ControllerAutoDisableMouse is retired' in BRIDGE
    assert '"ControllerAutoDisableMouse=1' not in CFG


def test_original_game_gRenderTest_mouse_disable_gates_are_exact():
    assert "SPIDER_GAME_RENDERTEST_VA 0x02E0988Cu" in MODE
    assert "SPIDER_GAME_MOUSE_DISABLED 0x10u" in MODE
    assert "0x0050C470u" in MODE  # draw game cursor
    assert "0x0050C4CFu" in MODE  # update original mouse state
    assert "0x0050C5F0u" in MODE  # IsMouseOver
    assert "0xF6u,0x05u,0x8Cu,0x98u,0xE0u,0x02u,0x10u,0x75u" in MODE
    assert "0xA0u,0x8Cu,0x98u,0xE0u,0x02u,0xA8u,0x10u,0x0Fu,0x85u" in MODE
    assert "0xA0u,0x8Cu,0x98u,0xE0u,0x02u,0x83u,0xECu,0x18u,0xA8u,0x10u,0x74u" in MODE
    assert "memcmp((const void *)at,mouse_gates[i].code,mouse_gates[i].length)" in MODE
    assert "nt->OptionalHeader.ImageBase!=0x00400000u" in MODE
    assert "nt->OptionalHeader.SizeOfImage" in MODE
    assert "mbi.State!=MEM_COMMIT" in MODE
    assert "PAGE_GUARD" in MODE
    assert "InterlockedCompareExchange(render_flags,desired,before)" in MODE


def test_mouse_restoration_on_focus_loss_and_no_win32_cursor_hook():
    for banned in ("ShowCursor(", "SetCursorPos(", "ClipCursor(",
                   "SetWindowLongPtr(", "SetWindowLongPtrA(",
                   "SetWindowsHookEx(", "IDirectInputDevice8_", "VirtualProtect("):
        assert banned not in MODE
    assert "original native sprite/poll/hit testing disabled" in MODE
    assert "desktop untouched" in MODE
    assert "if(!owns_bit)" in MODE
    assert "GetForegroundWindow()==game_hwnd" in MODE
    assert "IsWindowVisible(game_hwnd)" in MODE
    assert "if(!in_game){" in MODE
    assert "SpiderPS1Mouse_SetPadActive(0);" in MODE
    assert "if(!armed && !mouse_arm_attempted)" in MODE
    assert "if(!SpiderPS1Mouse_Arm(mouse_logger))" in MODE
    assert "GAME MOUSE RESTORED (game focus lost)" in MODE


def test_mouse_module_compiled_into_windows_x86_artifact():
    assert "gamepad_ui_profile.c ps1_joystick_readonly_menu.c ps1_mouse_mode.c" in BUILD
    assert '#include "ps1_mouse_mode.h"' in BRIDGE
    assert 'void SpiderPS1Mouse_OnFlip(HWND game_hwnd);' in HEADER
