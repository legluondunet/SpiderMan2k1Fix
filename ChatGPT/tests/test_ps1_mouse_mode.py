"""The optional DualShock 4 mode must block original mouse hover, not
merely conceal the OS pointer, and must be safe across Alt-Tab/hot-unplug.
These exact locations are cross-checked against the game's original
PCShell/PCInput disassembly, not an arbitrary Windows cursor API.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MODE=(ROOT/"ps1_mouse_mode.c").read_text()
BRIDGE=(ROOT/"gamepad_ui_profile.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
BUILD=(ROOT/"build.sh").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_option_explicit_and_only_activated_for_verified_ps1_controller():
    assert '"ControllerAutoDisableMouse"' in VALID
    assert 'GetPrivateProfileIntA("Input","ControllerAutoDisableMouse",0,ini)==1' in BRIDGE
    assert "ControllerAutoDisableMouse=1" not in CFG  # default OFF
    assert "ps1_mouse_auto_requested" in BRIDGE
    assert "ps1_mouse_auto_armed=SpiderPS1Mouse_Arm(log_message)" in BRIDGE
    assert "SpiderPS1Mouse_SetPadActive(1);" in BRIDGE
    assert "if(!detected || !ps1_preview_requested)return;" in BRIDGE
    assert "if(!probing)detect_device(hwnd);" in BRIDGE
    assert "if(d.found!=1u || d.ds4_count!=1u)" in BRIDGE
    assert "spider_ps1_mouse" not in BRIDGE.lower() # No unknown backend.


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


def test_never_change_windows_cursor_or_original_input_api():
    # Do not repeat historical Alt-Tab focus/cursor corruption.
    for banned in ("ShowCursor(", "SetCursorPos(", "ClipCursor(", 
                   "SetWindowLongPtr(", "SetWindowLongPtrA(",
                   "SetWindowsHookEx(", "IDirectInputDevice8_", "VirtualProtect("):
        assert banned not in MODE
    assert "original native sprite/poll/hit testing disabled" in MODE
    assert "desktop untouched" in MODE
    assert "if(!owns_bit)" in MODE
    assert "if(ps1_mouse_auto_armed)SpiderPS1Mouse_SetPadActive(0);" in BRIDGE
    assert "if(ps1_mouse_auto_armed)SpiderPS1Mouse_SetPadActive(1);" in BRIDGE
    assert "if(GetForegroundWindow()!=hwnd || !IsWindowVisible(hwnd))" in BRIDGE
    assert "if(FAILED(hr))" in BRIDGE
    assert "GAMEPAD-MOUSE: GAME MOUSE RESTORED" in MODE


def test_mouse_module_compiled_into_windows_x86_artifact():
    assert "gamepad_ui_profile.c ps1_joystick_readonly_menu.c ps1_mouse_mode.c" in BUILD
    assert '#include "ps1_mouse_mode.h"' in BRIDGE
