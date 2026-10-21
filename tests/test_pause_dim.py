"""55% black gridded backdrop without extra popups or per-letter masks."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DIM=(ROOT/"pause_dim.c").read_text(encoding="utf-8")
OVERLAY=(ROOT/"pause_overlay.c").read_text(encoding="utf-8")
HEADER=(ROOT/"pause_dim.h").read_text(encoding="utf-8")
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
EXAMPLE=(ROOT/"SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")
GEN=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
VALID=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")


def test_config_and_defaults_unchanged():
    for text in (INI,EXAMPLE):
        assert "PauseDim=1" in text
        assert "PauseDimOpacity=55" in text
        assert "PauseGrid=1" in text
    assert '"PauseDim=1\\r\\n"' in GEN
    assert '"PauseDimOpacity=55\\r\\n"' in GEN
    assert '"PauseGrid=1\\r\\n"' in GEN
    for key in ("PauseDim","PauseDimOpacity","PauseGrid"):
        assert '"'+key+'"' in VALID
    assert 'spider_ini_uint(value,0,90)' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseDim",1,path)==1' in DIM
    assert 'GetPrivateProfileIntA("Input","PauseDimOpacity",DIM_DEFAULT_OPACITY,path)' in DIM
    assert 'GetPrivateProfileIntA("Input","PauseGrid",1,path)==1' in DIM
    assert "pause_dim.c" in BUILD
    assert "SpiderPauseDim_Init" in HEADER


def test_one_global_owned_alpha_popup_only():
    assert "DIM_DEFAULT_OPACITY 55" in DIM
    assert "DIM_GRID_STEP 16" in DIM
    assert "dim_alpha=(BYTE)((dim_opacity*255+50)/100)" in DIM
    assert "WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|" in DIM
    assert "WS_POPUP" in DIM
    assert "WS_CHILD|WS_CLIPSIBLINGS" in DIM
    assert "WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|" in DIM
    assert "SetLayeredWindowAttributes(window,0,dim_alpha,LWA_ALPHA)" in DIM
    assert "dim_panel_hwnd" not in DIM
    assert "dim_background_hwnd" in DIM
    assert "void SpiderPauseDim_ShowBackground(HWND game_window)" in DIM
    assert "void SpiderPauseDim_ShowBackground(HWND game_window);" in HEADER
    assert "DIM_PANEL_ALPHA" not in DIM
    assert "dim_show_center_panel" not in DIM
    assert "SpiderPauseOverlay_GameForeground(game_window)" in DIM
    assert "dim_background_hwnd=NULL;" in DIM
    assert "SpiderPauseOverlay_ExcludeFromRegion(game_window,visible_region)" in DIM
    assert "SetWindowRgn(window,visible_region,FALSE)" in DIM
    assert "CreateRoundRectRgn(bounds.left,bounds.top,bounds.right,bounds.bottom,20,20)" in OVERLAY


def test_grid_is_bounded_away_from_banner_and_controls_unchanged():
    paint=DIM[DIM.index("case WM_PAINT:"):DIM.index("case WM_MOUSEACTIVATE:")]
    assert "if(dim_grid_enabled)" in paint
    assert "if(window==dim_background_hwnd)" in paint
    assert "dim_background_paint(dc,&bounds);" in paint
    assert "CreatePen(PS_SOLID,1,DIM_GRID_COLOR)" in paint
    assert "ExcludeClipRect(dc,center.left-8,center.top-8," in paint
    assert "SpiderPauseOverlay_GetBounds(GetWindow(window,GW_OWNER),&center)" in paint
    assert "for(x=DIM_GRID_STEP;x<bounds.right;x+=DIM_GRID_STEP)" in paint
    assert "for(y=DIM_GRID_STEP;y<bounds.bottom;y+=DIM_GRID_STEP)" in paint
    assert "SelectObject(dc,previous)" in paint
    assert "DeleteObject(pen)" in paint
    assert "FillRect(dc,&bounds,(HBRUSH)GetStockObject(BLACK_BRUSH))" in paint
    assert "EndPaint(window,&ps)" in paint
    assert "SetWindowRgn(window,visible_region,FALSE)" in DIM
    assert "GetForegroundWindow()!=game_window" not in DIM
    assert "dim_background_region(width,height,rows,dim_grid_enabled)" in DIM


def test_nonactivating_focus_shade_and_safe_failure():
    assert "WM_MOUSEACTIVATE" in DIM and "MA_NOACTIVATE" in DIM
    assert "WM_NCHITTEST" in DIM and "HTTRANSPARENT" in DIM
    assert "FOCUS-DIM: owned-popup creation failed Win32=" in DIM
    assert "FOCUS-DIM: alpha unavailable Win32=" in DIM
    assert "FOCUS-DIM: shaped cutout failed Win32=" in DIM
    assert "SpiderPauseOverlay_GameForeground(game_window)" in DIM
    for bad in ("HWND_TOPMOST","WS_EX_TOPMOST","SetForegroundWindow(", "SetActiveWindow(",
                "SetFocus(", "SetCapture(", "DirectDrawCreate(", "BinkPause("):
        assert bad not in DIM


def test_resume_and_wm_quit_destroy_both_before_audio():
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.count("SpiderPauseDim_Hide();")==4
    assert pause.count("SpiderPauseOverlay_Hide();")==4
    assert pause.count("SpiderFocusDSound_Resume();")==2
    assert pause.count("SpiderFocusMedia_Resume();")==2
    assert "SpiderPauseDim_Hide();\\n        SpiderPauseOverlay_Hide();\\n        SpiderFocusDSound_Resume();".replace("\\n","\n") in pause
    assert "SpiderPauseDim_Hide();\\n  SpiderPauseOverlay_Hide();\\n  SpiderFocusDSound_Resume();".replace("\\n","\n") in pause

def test_unfocused_grid_shade_is_a_sparse_game_parented_child():
    assert "BG_DITHER_PERIOD 8" in DIM
    assert "DIM_GRID_STEP 16" in DIM
    assert "(dim_opacity*BG_DITHER_PERIOD+50)/100" in DIM
    assert "((y*5)%BG_DITHER_PERIOD>=rows" in DIM
    assert "CreateRectRgn(0,y,width,y+1)" in DIM
    assert "CreateRectRgn(x,0,x+1,height)" in DIM
    assert "CombineRgn(region,region,strip,RGN_OR)" in DIM
    assert "WS_CHILD|WS_CLIPSIBLINGS" in DIM
    assert "CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_NOPARENTNOTIFY" in DIM
    assert "0,0,width,height,game_window,NULL,instance,NULL" in DIM
    assert "SetWindowRgn(window,region,FALSE)" in DIM
    assert "SetLayeredWindowAttributes(window,0,dim_alpha,LWA_ALPHA)" in DIM
    background=DIM[DIM.index("void SpiderPauseDim_ShowBackground("):
                   DIM.index("void SpiderPauseDim_Hide(")]
    assert "WS_EX_LAYERED" not in background
    assert "WS_POPUP" not in background
    assert "WS_EX_TOPMOST" not in background
    assert "dim_background_hwnd=window;" in background
    assert "ShowWindow(window,SW_SHOWNOACTIVATE)" in background
    assert "if(child && IsWindow(child))DestroyWindow(child)" in DIM

def test_dither_density_tracks_opacity_without_black_rectangle():
    def rows(opacity):
        return min(8,(opacity*8+50)//100)
    assert rows(0)==0
    assert rows(55)==4
    assert rows(90)==7
    for opacity in (0,15,40,55,75,90):
        covered=sum(1 for y in range(8) if (y*5)%8<rows(opacity))
        assert covered==rows(opacity)
    assert "CreateRectRgn(0,0,width,height)" not in DIM[
        DIM.index("static HRGN dim_background_region("):
        DIM.index("static void dim_background_paint(")]
