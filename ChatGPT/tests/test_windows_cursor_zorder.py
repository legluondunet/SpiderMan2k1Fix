"""Windows desktop cursor restore defaults ON; Z-order remains experimental."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
S = (ROOT / "input_focus_trace.c").read_text()
INI = (ROOT / "ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini").read_text()
GEN = (ROOT / "virtual_spidey_cfg.c").read_text()
VALID = (ROOT / "ini_validation.h").read_text()
DOC = (ROOT / "docs/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()


def test_cursor_restore_enabled_by_default_including_old_user_ini():
    assert "RestoreCursorOnFocusLoss=1" in INI
    assert '"RestoreCursorOnFocusLoss=1\\r\\n"' in GEN
    assert '"RestoreCursorOnFocusLoss"' in VALID
    assert 'GetPrivateProfileIntA("Input","RestoreCursorOnFocusLoss",1,ini)==1' in S
    assert 'InterlockedExchange(&cursor_visibility_test,want_cursor);' in S
    assert 'SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST' not in S


def test_cursor_only_tracking_never_subclasses_or_hooks_directinput():
    anchor = S.index('if(!want_trace && !want_fix && want_cursor) {')
    end = S.index('if(!want_trace && !want_fix) {', anchor)
    code = S[anchor:end]
    assert 'InterlockedExchange(&cursor_only_mode,1)' in code
    assert 'InterlockedExchange(&diagnostic_active,1)' in code
    assert 'InterlockedExchange(&reacquire_enabled,0)' in code
    assert 'return;' in code
    attach = S[S.index('void SpiderInputFocus_AttachWindow'):S.index('static void focus_log_device_call')]
    assert 'if(InterlockedCompareExchange(&cursor_only_mode,0,0)==1)' in attach
    assert 'tracked_window=hwnd;' in attach
    assert attach.index('return;', attach.index('cursor_only_mode')) < attach.index('SetWindowLongPtrA(hwnd,GWLP_WNDPROC')
    assert 'no WndProc subclass or DirectInput hooks' in S
    assert 'SPIDERMAN2K1FIX_ZORDER_TEST' in S


def test_cursor_restore_only_after_real_background_transition_and_balanced():
    assert 'previous=InterlockedExchange(&last_focus_foreground,foreground)' in S
    assert 'if(previous!=-1) focus_window_transition((int)foreground);' in S
    assert 'if(!foreground && InterlockedCompareExchange(&cursor_visibility_test,0,0)==1)' in S
    assert 'cursor_display_increments==0 && GetCursorInfo(&ci)' in S
    assert '!(ci.flags&CURSOR_SHOWING)' in S
    assert 'for(i=0;i<16;i++)' in S
    assert 'ShowCursor(TRUE)' in S
    assert 'if(foreground && cursor_display_increments>0)' in S
    assert 'ShowCursor(FALSE)' in S
    assert 'cursor_display_increments=0;' in S


def test_window_zorder_remains_opt_in_only():
    assert 'if(foreground && InterlockedCompareExchange(&zorder_test,0,0)==1' in S
    assert 'SetWindowPos(tracked_window,HWND_TOP' in S
    assert 'SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER' in S
    assert 'HWND_TOPMOST' not in S
    assert 'SPIDERMAN2K1FIX_ZORDER_TEST' in DOC


def test_ini_opt_out_is_documented():
    assert 'RestoreCursorOnFocusLoss=0' in DOC
    assert 'RestoreCursorOnFocusLoss=1' in DOC
