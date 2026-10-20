"""Win11 cursor/Z-order foreground-only experiments (opt-in)."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
S=(ROOT/"input_focus_trace.c").read_text()
DOC=(ROOT/"WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()

def test_passive_only_controls_and_no_directinput_hook():
    assert 'SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST' in S
    assert 'SPIDERMAN2K1FIX_ZORDER_TEST' in S
    idx=S.index('SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST')
    next_part=S[idx:S.index('if(!want_trace && !want_fix)',idx)]
    assert 'reacquire_enabled,0' in next_part
    assert 'return;' in next_part

def test_cursor_and_window_repair_only_on_transition():
    assert 'previous=InterlockedExchange(&last_focus_foreground,foreground)' in S
    assert 'if(previous!=-1) focus_window_transition((int)foreground);' in S
    assert 'if(!foreground && InterlockedCompareExchange(&cursor_visibility_test,0,0)==1)' in S
    assert 'if(foreground && InterlockedCompareExchange(&zorder_test,0,0)==1' in S
    assert 'SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER' in S
    assert 'SetWindowPos(tracked_window,HWND_TOP' in S
    assert 'HWND_TOPMOST' not in S

def test_showcursor_counter_balanced():
    assert 'cursor_display_increments==0 && GetCursorInfo(&ci)' in S
    assert 'cursor_display_increments>0' in S
    assert 'ShowCursor(TRUE)' in S
    assert 'ShowCursor(FALSE)' in S
    assert 'for(i=0;i<16;i++)' in S

def test_usage_documented():
    assert "$env:SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST='1'" in DOC
    assert "$env:SPIDERMAN2K1FIX_ZORDER_TEST='1'" in DOC
    assert "$env:SPIDERMAN2K1FIX_D3D_INPLACE='1'" in DOC
