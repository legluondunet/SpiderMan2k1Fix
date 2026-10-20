"""Windows 11 opt-in taskbar icon regression checks."""
from pathlib import Path

SRC=(Path(__file__).resolve().parents[1]/"spiderman2k1fix.c").read_text()

def test_icon_never_applied_by_default():
    assert 'SPIDERMAN2K1FIX_WINDOW_ICON_TEST' in SRC
    assert "n==1 && value[0]=='1'" in SRC
    assert 'if(!spidey_icon_optin() || !hwnd || !IsWindow(hwnd)) return;' in SRC

def test_uses_game_directory_not_working_directory():
    assert 'GetModuleFileNameW(NULL,path,MAX_PATH)' in SRC
    assert 'L"SpideyPC.ico"' in SRC
    assert 'GetFileAttributesW(path)' in SRC
    assert 'LR_LOADFROMFILE' in SRC

def test_sets_small_and_large_window_icons_without_changing_zorder():
    assert 'WM_SETICON,ICON_SMALL' in SRC
    assert 'WM_SETICON,ICON_BIG' in SRC
    assert 'WM_GETICON,ICON_SMALL' in SRC
    assert 'WM_GETICON,ICON_BIG' in SRC
    start=SRC.index('static void borderless_window_apply(void)')
    end=SRC.index('static HRESULT borderless_get_back(',start)
    assert SRC[start:end].count('spidey_apply_window_icon(game_hwnd);')==1
    assert 'HWND_NOTOPMOST' in SRC[start:end]
