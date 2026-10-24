"""Windows 11 taskbar icon: use icon-group resources embedded in the game EXE."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()


def test_icon_enabled_by_default_with_explicit_diagnostic_opt_out():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_WINDOW_ICON"' in SRC
    assert "return !(n==1 && value[0]=='0');" in SRC
    assert 'if(!spidey_icon_enabled() || !hwnd || !IsWindow(hwnd)) return;' in SRC
    assert 'SPIDERMAN2K1FIX_WINDOW_ICON_TEST' not in SRC


def test_extracts_embedded_exe_icon_group_not_external_ico_or_hardcoded_resource_id():
    assert '#include <shellapi.h>' in SRC
    assert 'GetModuleFileNameW(NULL,path,MAX_PATH)' in SRC
    assert 'ExtractIconExW(path,0,&big,&small,1)' in SRC
    assert '-lshell32' in BUILD
    assert 'L"SpideyPC.ico"' not in SRC
    assert 'LoadImageW(NULL,path,IMAGE_ICON' not in SRC
    assert 'MAKEINTRESOURCE' not in SRC[SRC.index('/* Win11 taskbar window icon:'):SRC.index('static void borderless_window_apply(void)')]


def test_sets_small_and_large_window_icons_without_changing_zorder():
    assert 'WM_SETICON,ICON_SMALL' in SRC
    assert 'WM_SETICON,ICON_BIG' in SRC
    assert 'WM_GETICON,ICON_SMALL' in SRC
    assert 'WM_GETICON,ICON_BIG' in SRC
    start=SRC.index('static void borderless_window_apply(void)')
    end=SRC.index('static HRESULT borderless_get_back(',start)
    assert SRC[start:end].count('spidey_apply_window_icon(game_hwnd);')==1
    assert 'HWND_NOTOPMOST' in SRC[start:end]


def test_missing_resource_preserves_original_window_icon_and_releases_partial_icons():
    # Windows may return 2 for the large and small handles even with nIcons=1.
    assert 'if(extracted==0 || extracted==UINT_MAX || !small || !big)' in SRC
    assert 'if(extracted!=1 || !small || !big)' not in SRC
    assert 'error=(extracted==UINT_MAX)?GetLastError():0;' in SRC
    assert 'loaded small+big extracted=%u' in SRC
    assert 'if(small) DestroyIcon(small);' in SRC
    assert 'if(big) DestroyIcon(big);' in SRC
    assert 'EXE embedded icon unavailable' in SRC
    assert 'original icon unchanged' in SRC
    assert 'if(!spidey_icon_small || !spidey_icon_big) return;' in SRC
    assert 'spidey_icon_load_attempted=TRUE;' in SRC
