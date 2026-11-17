"""Windows/Wine unified: no game mouse, but free OS cursor and focus pause."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KEY = (ROOT/"pause_any_key.c").read_text(encoding="utf-8")
FOCUS = (ROOT/"input_focus_trace.c").read_text(encoding="utf-8")
CORE = (ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")


def test_keyboard_hook_follows_its_dll_callback_not_historical_filename():
    assert "GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS" in KEY
    assert "GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT" in KEY
    assert "&keyboard_proc,&module)" in KEY
    assert 'GetModuleHandleA("SpiderMan2k1Fix.dll")' not in KEY
    assert 'GetModuleHandleA("fix.dll")' not in KEY
    assert "SetWindowsHookExA(WH_KEYBOARD_LL,keyboard_proc,module,0)" in KEY
    assert "CallNextHookEx(hook,code,wp,lp)" in KEY


def test_unified_pointer_idle_hide_and_reveal_without_capture():
    assert "#ifdef SPIDER_UNIFIED_FIX" in FOCUS
    assert "#define SPIDER_UNIFIED_CURSOR_IDLE_MS 2500u" in FOCUS
    assert "GetCursorPos(&pos)" in FOCUS
    assert "GetTickCount()" in FOCUS
    assert "PtInRect(&window,pos)" in FOCUS
    assert "unified_cursor_last_motion_tick=now;" in FOCUS
    assert "(DWORD)(now-unified_cursor_last_motion_tick)" in FOCUS
    assert "pos.x!=unified_cursor_last_pos.x" in FOCUS
    assert "pos.y!=unified_cursor_last_pos.y" in FOCUS
    assert "focus_unified_cursor_unhide_ours();" in FOCUS
    assert "focus_unified_cursor_show_foreground();" in FOCUS
    assert "ShowCursor(FALSE);" in FOCUS
    assert "ShowCursor(TRUE);" in FOCUS
    assert "LoadCursorA(NULL,IDC_ARROW)" in FOCUS
    assert "SetCursor(" in FOCUS
    assert "GetClipCursor(&clipped)" in FOCUS
    assert "GetWindowRect(tracked_window,&window)" in FOCUS
    assert "EqualRect(&clipped,&window)" in FOCUS
    assert "!EqualRect(&clipped,&desktop)" in FOCUS
    assert "ClipCursor(NULL)" in FOCUS
    assert "idle 2500ms over game" in FOCUS
    assert "pointer moved; showing desktop arrow again" in FOCUS

    begin=FOCUS.split("static void focus_unified_cursor_available(void) {",1)[1].split(
        "static void focus_window_transition(",1
    )[0]
    # On loss of focus, release only our temporary hide, never mask pointer.
    assert "if(!foreground)" in begin
    assert "focus_unified_cursor_unhide_ours();" in begin
    assert "if(!inside ||" in begin
    assert "SPIDER_UNIFIED_CURSOR_IDLE_MS" in begin
    transition=FOCUS.split("static void focus_window_transition(",1)[1].split(
        "void SpiderInputFocus_OnFlip(",1
    )[0]
    assert "#ifndef SPIDER_UNIFIED_FIX" in transition
    assert "ShowCursor(FALSE)" in transition  # legacy code only
    unified=transition.split("#else",1)[1].split("#endif",1)[0]
    assert "ShowCursor(FALSE)" not in unified
    assert "focus_unified_cursor_available();" in unified
    flip=FOCUS.split("void SpiderInputFocus_OnFlip(",1)[1]
    assert "focus_unified_cursor_available();" in flip
    assert "if(previous!=-1) focus_window_transition((int)foreground);" in flip
    # No Windows mouse hook, no WndProc subclass or pointer lock.
    unified_cursor=FOCUS.split("#define SPIDER_UNIFIED_CURSOR_IDLE_MS",1)[1].split(
        "#endif\nstatic void focus_window_transition(",1
    )[0]
    for forbidden in ("SetCapture(", "SetCursorPos(", "SetWindowsHookEx(",
                      "SetWindowLongPtr(", "Sleep(", "PeekMessage("):
        assert forbidden not in unified_cursor


def test_verified_game_mouse_is_off_and_focus_loss_pauses():
    mouse=(ROOT/"ps1_mouse_mode.c").read_text(encoding="utf-8")
    assert "SPIDER_GAME_MOUSE_DISABLED" in mouse
    assert "GAME MOUSE OFF (permanent)" in mouse
    assert "SpiderPauseAny_Begin(hwnd)" in CORE
    assert "SpiderFocusMedia_Pause();" in CORE
    assert "SpiderFocusDSound_Pause();" in CORE
    assert "SpiderPauseSurface_End();" in CORE
