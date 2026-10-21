"""Opt-in Any-key resume; default F9 and original DirectInput repair unchanged."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
HEADER=(ROOT/"pause_any_key.h").read_text(encoding="utf-8")
SOURCE=(ROOT/"pause_any_key.c").read_text(encoding="utf-8")
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
GEN=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
VALID=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")


def test_option_is_opt_in_and_ini_validator_accepts_any():
    assert "PauseKey=F9" in INI
    assert '"PauseKey=F9\\r\\n"' in GEN
    assert 'spider_ini_icmp(value,"Any")' in VALID
    assert 'lstrcmpiA(key,"Any")' in CORE
    assert "pause_key_vk=(lstrcmpiA(key,\"Any\")==0)?0:" in CORE
    assert "VK_SPACE:VK_F9" in CORE


def test_any_is_not_polled_as_regular_manual_pause():
    poll=CORE[CORE.index("static void pause_poll_key("):
              CORE.index("static void pause_log_resume_focus(")]
    assert "if(pause_key_vk==0) return;" in poll
    assert "GetAsyncKeyState(pause_key_vk)" in poll
    assert 'pause_key_vk==0 ? "FOCUS-PAUSE:' in CORE


def test_any_hook_only_lives_during_focus_pause():
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "SpiderPauseAny_Begin(hwnd)" in pause
    assert "SpiderPauseAny_Completed()" in pause
    assert "SpiderPauseAny_DiscardGameKeyMessage(&msg,hwnd)" in pause
    assert pause.count("SpiderPauseAny_End();")==2
    assert "pause_key_vk=VK_F9" in pause  # fallback on install error
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
