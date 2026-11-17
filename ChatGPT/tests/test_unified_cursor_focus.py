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


def test_unified_pointer_not_hidden_again_on_focus_return():
    assert "#ifdef SPIDER_UNIFIED_FIX" in FOCUS
    begin=FOCUS.split("static void focus_unified_cursor_available(void) {",1)[1].split(
        "static void focus_window_transition(",1
    )[0]
    assert "GetForegroundWindow()!=tracked_window" in begin
    assert "GetCursorInfo(&ci)" in begin
    assert "ShowCursor(TRUE)" in begin
    assert "LoadCursorA(NULL,IDC_ARROW)" in begin
    assert "SetCursor(" in begin
    assert "GetClipCursor(&clipped)" in begin
    assert "GetWindowRect(tracked_window,&window)" in begin
    assert "EqualRect(&clipped,&window)" in begin
    assert "!EqualRect(&clipped,&desktop)" in begin
    assert "ClipCursor(NULL)" in begin
    transition=FOCUS.split("static void focus_window_transition(",1)[1].split(
        "void SpiderInputFocus_OnFlip(",1
    )[0]
    assert "#ifndef SPIDER_UNIFIED_FIX" in transition
    assert "ShowCursor(FALSE)" in transition  # legacy code only
    assert "#else" in transition
    unified=transition.split("#else",1)[1].split("#endif",1)[0]
    assert "ShowCursor(FALSE)" not in unified
    flip=FOCUS.split("void SpiderInputFocus_OnFlip(",1)[1]
    assert "focus_unified_cursor_available();" in flip
    assert "if(previous!=-1) focus_window_transition((int)foreground);" in flip


def test_verified_game_mouse_is_off_and_focus_loss_pauses():
    mouse=(ROOT/"ps1_mouse_mode.c").read_text(encoding="utf-8")
    assert "SPIDER_GAME_MOUSE_DISABLED" in mouse
    assert "GAME MOUSE OFF (permanent)" in mouse
    assert "SpiderPauseAny_Begin(hwnd)" in CORE
    assert "SpiderFocusMedia_Pause();" in CORE
    assert "SpiderFocusDSound_Pause();" in CORE
    assert "SpiderPauseSurface_End();" in CORE
