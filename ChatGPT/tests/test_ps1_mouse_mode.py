"""Permanent game-only mouse disablement, independent of gamepad discovery.

Legacy MouseEnabled=1 is intentionally ignored. Retail game bit 0x10
suppresses in-game cursor, polling and menu hover/click. Never touch
Windows desktop pointer, keyboard, or original controller bindings.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MODE=(ROOT/"ps1_mouse_mode.c").read_text(encoding="utf-8")
HEADER=(ROOT/"ps1_mouse_mode.h").read_text(encoding="utf-8")
BRIDGE=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
VALID=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")
CFG=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
README=(ROOT/"README.md").read_text(encoding="utf-8")


def test_permanent_mouse_disable_independent_of_controller_and_legacy_ini():
    assert '"MouseEnabled"' in VALID
    assert '"ControllerAutoDisableMouse"' in VALID
    assert 'SpiderPS1Mouse_Configure(logger);' in BRIDGE
    assert 'SpiderPS1Mouse_OnFlip(hwnd);' in BRIDGE
    assert 'GetPrivateProfileIntA("Input","MouseEnabled"' not in BRIDGE
    assert 'GetPrivateProfileStringA("Input","MouseEnabled"' not in BRIDGE
    assert '"MouseEnabled=0\\r\\n"' not in CFG
    assert '"MouseEnabled=1\\r\\n"' not in CFG
    assert 'SpiderPS1Mouse_Configure(void (*logger)(const char *))' in MODE
    assert 'mouse_policy_enabled' not in MODE
    assert 'mouse_enabled' not in MODE
    assert 'SpiderPS1Mouse_SetPadActive' not in MODE+HEADER
    assert 'GAME MOUSE RESTORED' not in MODE
    assert 'GAME MOUSE OFF (permanent)' in MODE
    assert 'if(!detected||!device)' not in BRIDGE
    assert 'ps1_mouse_auto_requested' not in BRIDGE
    assert 'ps1_mouse_auto_armed' not in BRIDGE
    assert 'void SpiderPS1Mouse_OnFlip(HWND game_hwnd);' in HEADER

def test_original_game_native_disable_gates_and_fail_closed_preflight():
    assert "SPIDER_GAME_RENDERTEST_VA 0x02E0988Cu" in MODE
    assert "SPIDER_GAME_MOUSE_DISABLED 0x10u" in MODE
    assert "0x0050C470u" in MODE  # draw game cursor
    assert "0x0050C4CFu" in MODE  # update game mouse input
    assert "0x0050C5F0u" in MODE  # game mouse hit-testing
    assert "0xF6u,0x05u,0x8Cu,0x98u,0xE0u,0x02u,0x10u,0x75u" in MODE
    assert "0xA0u,0x8Cu,0x98u,0xE0u,0x02u,0xA8u,0x10u,0x0Fu,0x85u" in MODE
    assert "0xA0u,0x8Cu,0x98u,0xE0u,0x02u,0x83u,0xECu,0x18u,0xA8u,0x10u,0x74u" in MODE
    assert "memcmp((const void *)at,mouse_gates[i].code,mouse_gates[i].length)" in MODE
    assert "nt->OptionalHeader.ImageBase!=0x00400000u" in MODE
    assert "nt->OptionalHeader.SizeOfImage" in MODE
    assert "mbi.State!=MEM_COMMIT" in MODE
    assert "PAGE_GUARD" in MODE
    assert "if(!SpiderPS1Mouse_Arm(mouse_logger))" in MODE
    assert "permanent disable REFUSED" in MODE


def test_game_mouse_bit_never_cleared_even_on_focus_loss():
    assert 'desired=(LONG)((unsigned long)before|SPIDER_GAME_MOUSE_DISABLED);' in MODE
    assert 'InterlockedCompareExchange(render_flags,desired,before)' in MODE
    assert 'before&~SPIDER_GAME_MOUSE_DISABLED' not in MODE
    assert 'owns_bit' not in MODE
    assert 'SpiderPS1Mouse_EnsureDisabled();' in MODE
    assert 'if(!mouse_policy_configured)return;' in MODE
    assert 'if(!in_game){' in MODE
    bg=MODE[MODE.index("if(!in_game){"):MODE.index("if(!armed && !mouse_arm_attempted)")]
    assert 'if(armed)SpiderPS1Mouse_EnsureDisabled();' in bg
    assert 'SpiderPS1Mouse_EnsureDisabled();' in MODE[MODE.index('if(!armed && !mouse_arm_attempted)'):]
    assert "GetForegroundWindow()==game_hwnd" in MODE
    assert "IsWindowVisible(game_hwnd)" in MODE


def test_native_keyboard_gamepad_and_desktop_pointer_untouched():
    for banned in ("ShowCursor(", "SetCursorPos(", "ClipCursor(",
                   "SetWindowLongPtr(", "SetWindowLongPtrA(",
                   "SetWindowsHookEx(", "IDirectInputDevice8_",
                   "VirtualProtect(", "SendInput(", "keybd_event("):
        assert banned not in MODE
    assert "desktop untouched" in MODE
    assert "desktop pointer unchanged" in MODE
    assert "gamepad_ui_profile.c ps1_joystick_readonly_menu.c ps1_mouse_mode.c" in BUILD
    assert '#include "ps1_mouse_mode.h"' in BRIDGE
    assert 'void SpiderPS1Mouse_OnFlip(HWND game_hwnd);' in HEADER
    assert "MouseEnabled=1" not in README  # no live opt-in documented
