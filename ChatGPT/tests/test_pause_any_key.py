"""Exclusive Any-key pause wake while preserving native DirectInput repair."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
HEADER=(ROOT/"pause_any_key.h").read_text(encoding="utf-8")
SOURCE=(ROOT/"pause_any_key.c").read_text(encoding="utf-8")
INI=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
GEN=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
VALID=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")


def test_retired_pause_key_configuration_is_not_emitted_or_used():
    assert "PauseKey=" not in INI
    assert '"PauseKey=' not in GEN
    assert '"PauseKey"' in VALID  # historical values tolerated only
    assert "GetPrivateProfileStringA(\"Input\",\"PauseKey\"" not in CORE
    assert "pause_key_vk" not in CORE
    assert "pause_poll_key" not in CORE
    # Keep the no-F9-wake guarantee scoped to the pause implementation.
    # A separate, opt-in F9 readback trace is not part of pause handling.
    pause = CORE[CORE.index("static void pause_at_flip_if_background(void)"):
                 CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "GetAsyncKeyState(VK_F9)" not in pause


def test_any_hook_only_lives_during_focus_pause():
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "SpiderPauseAny_Begin(hwnd)" in pause
    assert "SpiderPauseAny_Completed()" in pause
    assert "SpiderPauseAny_DiscardGameKeyMessage(&msg,hwnd)" in pause
    assert pause.count("SpiderPauseAny_End();")==2
    assert "if(!SpiderPauseAny_Begin(hwnd))" in pause
    assert "skip focus pause (no fallback)" in pause
    assert pause.index("SpiderPauseAny_Begin(hwnd)") < pause.index("SpiderFocusMedia_Pause();")
    assert pause.index("SpiderPauseAny_Begin(hwnd)") < pause.index("SpiderFocusDSound_Pause();")
    assert "pause_key_vk" not in pause
    assert "PostQuitMessage(" in pause
    assert "SuspendThread" not in pause


def test_swallow_press_repeat_and_release_with_foreground_filter():
    assert "WH_KEYBOARD_LL" in SOURCE
    assert "SetWindowsHookExA(WH_KEYBOARD_LL,keyboard_proc,module,0)" in SOURCE
    assert "GetForegroundWindow()==target" in SOURCE
    assert "WM_KEYDOWN || wp==WM_SYSKEYDOWN" in SOURCE
    assert "WM_KEYUP || wp==WM_SYSKEYUP" in SOURCE
    assert "if(down || up) return 1;" in SOURCE
    assert "CallNextHookEx(hook,code,wp,lp)" in SOURCE
    assert "UnhookWindowsHookEx(old)" in SOURCE
    assert "if(!any_held && InterlockedCompareExchange(&received_press,0,0)==1)" in SOURCE
    assert "GetForegroundWindow()!=target" in SOURCE
    assert "DirectInput8Create" not in SOURCE and "CreateDevice" not in SOURCE
    assert "SpiderPauseAny_DiscardGameKeyMessage" in HEADER


def test_linker_includes_hook_module():
    assert "pause_any_key.c" in BUILD

def test_system_navigation_remains_available_during_foreground_pause():
    assert 'vk==VK_MENU || vk==VK_LMENU || vk==VK_RMENU' in SOURCE
    assert 'vk==VK_LWIN || vk==VK_RWIN' in SOURCE
    assert '(vk==VK_TAB && (info->flags&LLKHF_ALTDOWN))' in SOURCE
    assert 'return CallNextHookEx(hook,code,wp,lp);' in SOURCE
    assert SOURCE.index('vk==VK_MENU || vk==VK_LMENU') < SOURCE.index('if(vk<256u && down)')
    assert 'SpiderPauseAny_Completed()' in CORE
    assert 'SpiderPauseAny_DiscardGameKeyMessage(&msg,hwnd)' in CORE
