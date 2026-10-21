"""Background pause banner stays inside Spider-Man; translucent popup only in foreground."""
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
    assert "if(banner_mode!=2)" in p
    assert "else if(banner_mode!=1)" in p
    assert 'FOCUS-PAUSE: game-parented dark dither/grid and PAUSE child remain visible in background' in p
    background=p[p.index("} else if(banner_mode!=1)"):p.index("MsgWaitForMultipleObjectsEx(",p.index("} else if(banner_mode!=1)"))]
    assert background.index("SpiderPauseDim_Hide();") < background.index("SpiderPauseOverlay_Hide();")
    assert background.index("SpiderPauseOverlay_Hide();") < background.index("SpiderPauseDim_ShowBackground(hwnd);")
    assert background.index("SpiderPauseDim_ShowBackground(hwnd);") < background.index("SpiderPauseOverlay_ShowBackground(hwnd);")
    assert "MsgWaitForMultipleObjectsEx(0,NULL,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE)" in p
    assert "GetActiveWindow()==game_window" in OVERLAY
    assert "SpiderPauseOverlay_ShowBackground(hwnd);" in p
    assert "WS_CHILD|WS_CLIPSIBLINGS" in OVERLAY
    assert "WS_CHILD|WS_CLIPSIBLINGS" in DIM
    assert "GetForegroundWindow()==game_window" in OVERLAY


def test_foreground_alpha_popup_and_background_native_child():
    assert "CreateDIBSection(" in OVERLAY
    assert "UpdateLayeredWindow(" in OVERLAY
    assert "PANEL_ALPHA 188" in OVERLAY
    assert "RoundRect(dc,1,1,width-1,height-1,20,20)" in OVERLAY
    assert "GetWindowRgn(" not in OVERLAY
    assert "PathToRegion(" not in OVERLAY
    assert "dim_panel_hwnd" not in DIM
    assert "void SpiderPauseOverlay_ShowBackground(HWND game_window)" in OVERLAY
    assert "CreateRoundRectRgn(0,0,OVERLAY_WIDTH,OVERLAY_HEIGHT,20,20)" in OVERLAY
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

def test_background_dimmer_created_before_banner_and_no_floating_popup():
    p=pause()
    assert p.index("SpiderPauseDim_ShowBackground(hwnd);") < p.index("SpiderPauseOverlay_ShowBackground(hwnd);")
    assert p.index("SpiderPauseDim_ShowBackground(hwnd);") > p.index("while(IsWindow(hwnd)")
    start=DIM.index("void SpiderPauseDim_ShowBackground(HWND game_window)")
    stop=DIM.index("void SpiderPauseDim_Hide(void)")
    background=DIM[start:stop]
    assert "WS_EX_LAYERED" not in background
    assert "WS_POPUP" not in background
    assert "WS_CHILD|WS_CLIPSIBLINGS" in background
    assert "game_window,NULL,instance,NULL" in background
    assert "SetWindowRgn(window,region,FALSE)" in background
    assert "dim_background_region(width,height,rows,dim_grid_enabled)" in background
    assert 'FOCUS-DIM: background game-child grid=' in background
