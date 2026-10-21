"""PR #16: Wine compositor regression, active foreground and one-piece banner."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
OVERLAY=(ROOT/"pause_overlay.c").read_text(encoding="utf-8")
DIM=(ROOT/"pause_dim.c").read_text(encoding="utf-8")
ANY=(ROOT/"pause_any_key.c").read_text(encoding="utf-8")


def pause():
    return CORE[CORE.index("static void pause_at_flip_if_background(void)"):
                CORE.index("static HRESULT WINAPI hooked_flip(")]


def test_pause_media_background_show_only_when_foreground_and_active():
    p=pause()
    assert "SpiderFocusMedia_Pause();" in p
    assert "SpiderFocusDSound_Pause();" in p
    assert "while(IsWindow(hwnd) && InterlockedCompareExchange(&pause_latched,0,0)==1)" in p
    assert "if(SpiderPauseOverlay_GameForeground(hwnd))" in p
    assert "if(!visuals_visible)" in p
    assert 'FOCUS-PAUSE: overlay hidden (game background)' in p
    assert "SpiderPauseDim_Hide();\n      SpiderPauseOverlay_Hide();" in p
    assert "MsgWaitForMultipleObjectsEx(0,NULL,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE)" in p
    assert "GetActiveWindow()==game_window" in OVERLAY
    assert "GetForegroundWindow()==game_window" in OVERLAY


def test_simple_two_popup_composition():
    assert "CreateDIBSection(" in OVERLAY
    assert "UpdateLayeredWindow(" in OVERLAY
    assert "PANEL_ALPHA 188" in OVERLAY
    assert "RoundRect(dc,1,1,width-1,height-1,20,20)" in OVERLAY
    assert "GetWindowRgn(" not in OVERLAY
    assert "PathToRegion(" not in OVERLAY
    assert "dim_panel_hwnd" not in DIM
    assert "dim_show_center_panel" not in DIM
    assert "SpiderPauseOverlay_ExcludeFromRegion(game_window,visible_region)" in DIM


def test_plain_white_letters_on_single_panel_with_grid_elsewhere():
    assert 'L"PAUSE"' in OVERLAY
    assert 'L"PRESS ANY KEY TO RESUME"' in OVERLAY
    assert "RGB(255,255,255)" in OVERLAY
    assert "a=(p==base)?PANEL_ALPHA:255" in OVERLAY
    assert "CreateRoundRectRgn(bounds.left,bounds.top,bounds.right,bounds.bottom,20,20)" in OVERLAY
    assert "ExcludeClipRect(" in DIM


def test_any_key_and_desktop_shortcuts():
    assert "SpiderPauseAny_Begin(hwnd)" in CORE
    assert "SpiderPauseAny_Completed()" in CORE
    assert "VK_MENU || vk==VK_LMENU" in ANY
    assert "VK_LWIN || vk==VK_RWIN" in ANY
    assert "pause_key_vk" not in CORE
    assert "WS_EX_TOPMOST" not in OVERLAY
    assert "WS_EX_TOPMOST" not in DIM
