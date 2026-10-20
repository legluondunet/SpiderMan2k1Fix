"""Windows 11 taskbar icon enabled if adjacent ICO is available."""
from pathlib import Path

SRC=(Path(__file__).resolve().parents[1]/"spiderman2k1fix.c").read_text()

def test_icon_is_enabled_by_default_with_diagnostic_override():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_WINDOW_ICON"' in SRC
    assert "return !(n==1 && value[0]=='0');" in SRC
    assert 'if(!spidey_icon_enabled() || !hwnd || !IsWindow(hwnd)) return;' in SRC
    assert 'SPIDERMAN2K1FIX_WINDOW_ICON_TEST' not in SRC

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


def test_missing_ico_retains_game_window_icon():
    assert 'SpideyPC.ico not found; original icon unchanged' in SRC
    assert 'if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&FILE_ATTRIBUTE_DIRECTORY))' in SRC
    assert 'if(small) DestroyIcon(small);' in SRC
    assert 'if(big) DestroyIcon(big);' in SRC
