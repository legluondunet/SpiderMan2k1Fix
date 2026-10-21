"""Pause-only configurable 55% black shade and 16px grid leave banner intact."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DIM=(ROOT/"pause_dim.c").read_text(encoding="utf-8")
HEADER=(ROOT/"pause_dim.h").read_text(encoding="utf-8")
OVERLAY=(ROOT/"pause_overlay.c").read_text(encoding="utf-8")
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
EXAMPLE=(ROOT/"SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")
GEN=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
VALID=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")

def test_dim_is_opt_out_and_only_active_when_focus_pause_runs():
    assert "PauseDim=1" in INI
    assert "PauseDimOpacity=55" in INI
    assert "PauseGrid=1" in INI
    assert "PauseDim=1" in EXAMPLE
    assert "PauseDimOpacity=55" in EXAMPLE
    assert "PauseGrid=1" in EXAMPLE
    assert '"PauseDim=1\\r\\n"' in GEN
    assert '"PauseDimOpacity=55\\r\\n"' in GEN
    assert '"PauseGrid=1\\r\\n"' in GEN
    assert '"PauseDim"' in VALID
    assert '"PauseDimOpacity"' in VALID
    assert '"PauseGrid"' in VALID
    assert 'spider_ini_uint(value,0,90)' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseDim",1,path)==1' in DIM
    assert 'GetPrivateProfileIntA("Input","PauseDimOpacity",DIM_DEFAULT_OPACITY,path)' in DIM
    assert 'GetPrivateProfileIntA("Input","PauseGrid",1,path)==1' in DIM
    assert "pause_dim.c" in BUILD
    assert "void SpiderPauseDim_Init(void (*log_fn)(const char *));" in HEADER

def test_layered_owned_popup_avoids_legacy_unmanifested_child_restriction():
    assert "DIM_DEFAULT_OPACITY 55" in DIM
    assert "DIM_GRID_STEP 16" in DIM
    assert "dim_alpha=(BYTE)((dim_opacity*255+50)/100)" in DIM
    assert "WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|" in DIM
    assert "WS_EX_TOOLWINDOW|WS_EX_NOPARENTNOTIFY" in DIM
    assert "WS_POPUP" in DIM
    assert "WS_CHILD" not in DIM
    assert "game_window,NULL,instance,NULL" in DIM
    assert "ClientToScreen(game_window,&origin)" in DIM
    assert "CreateRectRgn(0,0,width,height)" in DIM
    assert "SpiderPauseOverlay_ExcludeFromRegion(game_window,visible_region)" in DIM
    assert "SetWindowRgn(window,visible_region,FALSE)" in DIM
    assert "GetWindowRgn(overlay_hwnd,banner_region)" in OVERLAY
    assert "MapWindowPoints(overlay_hwnd,game_window,&origin,1)" in OVERLAY
    assert "CombineRgn(visible_region,visible_region,banner_region,RGN_DIFF)" in OVERLAY
    assert "SetWindowRgn(window,visible_region,FALSE)" not in OVERLAY
    assert "SetLayeredWindowAttributes(window,0,dim_alpha,LWA_ALPHA)" in DIM
    assert "LWA_COLORKEY" not in DIM
    assert "BLACK_BRUSH" in DIM
    assert "FillRect(dc,&bounds,(HBRUSH)GetStockObject(BLACK_BRUSH))" in DIM
    assert DIM.index("SetLayeredWindowAttributes(window,0,dim_alpha,LWA_ALPHA)") < DIM.index("ShowWindow(window,SW_SHOWNOACTIVATE)")
    assert "FOCUS-DIM: alpha unavailable Win32=" in DIM
    assert "FOCUS-DIM: owned-popup creation failed Win32=" in DIM
    assert "FOCUS-DIM: shaped cutout failed Win32=" in DIM
    assert DIM.index("SpiderPauseOverlay_ExcludeFromRegion(game_window,visible_region)") < DIM.index("ShowWindow(window,SW_SHOWNOACTIVATE)")
    assert "DestroyWindow(window);" in DIM
    assert "return;" in DIM

def test_shade_never_takes_keyboard_mouse_or_desktop_foreground():
    assert "WM_MOUSEACTIVATE" in DIM and "MA_NOACTIVATE" in DIM
    assert "WM_NCHITTEST" in DIM and "HTTRANSPARENT" in DIM
    for unsafe in ("HWND_TOPMOST", "WS_EX_TOPMOST", "SetForegroundWindow(",
                   "SetActiveWindow(", "SetFocus(", "SetCapture(",
                   "DirectDrawCreate", "VirtualProtect(", "BinkPause(", "IDirectSoundBuffer_"):
        assert unsafe not in DIM

def test_original_centered_banner_is_preserved_above_shade():
    assert "SetWindowRgn(window,outline,FALSE)" in OVERLAY
    assert "WS_EX_LAYERED" not in OVERLAY
    assert 'L"PAUSE"' in OVERLAY
    assert 'L"PRESS ANY KEY TO RESUME"' in OVERLAY
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.count("SpiderPauseDim_Show(hwnd);")==1
    assert pause.count("SpiderPauseOverlay_Show(hwnd);")==1
    assert pause.index("SpiderPauseOverlay_Show(hwnd);") < pause.index("SpiderPauseDim_Show(hwnd);")
    assert pause.count("SpiderPauseDim_Hide();")==2
    assert pause.count("SpiderPauseOverlay_Hide();")==2
    for portion in pause.split("SpiderPauseOverlay_Hide();")[1:]:
        assert portion.index("SpiderPauseDim_Hide();") < portion.index("SpiderFocusDSound_Resume();")
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line);") < CORE.index("SpiderPauseDim_Init(log_line);")
    assert CORE.index("if (!supported_exe())") < CORE.index("SpiderPauseDim_Init(log_line);")
    assert "SpiderPauseDim_Hide();" in CORE

def test_game_pause_and_audio_do_not_depend_on_shade_creation():
    assert "if(!dim_enabled || !game_window || !IsWindow(game_window))return;" in DIM
    assert "FOCUS-DIM: owned-popup creation failed Win32=" in DIM
    assert "FOCUS-DIM: class registration failed Win32=" in DIM
    assert "SpiderFocusMedia_Pause();" in CORE
    assert "SpiderFocusDSound_Pause();" in CORE
    assert "SpiderPauseAny_Begin(hwnd)" in CORE

def test_grid_is_drawn_into_same_nonactivating_popup_not_a_new_window():
    paint=DIM[DIM.index("case WM_PAINT:"):DIM.index("case WM_MOUSEACTIVATE:")]
    assert "if(dim_grid_enabled && window!=dim_panel_hwnd)" in paint
    assert "CreatePen(PS_SOLID,1,DIM_GRID_COLOR)" in paint
    assert "for(x=DIM_GRID_STEP;x<bounds.right;x+=DIM_GRID_STEP)" in paint
    assert "for(y=DIM_GRID_STEP;y<bounds.bottom;y+=DIM_GRID_STEP)" in paint
    assert paint.count("LineTo(")==2
    assert "SelectObject(dc,previous)" in paint
    assert "DeleteObject(pen)" in paint
    assert "FillRect(dc,&bounds,(HBRUSH)GetStockObject(BLACK_BRUSH))" in paint
    assert "EndPaint(window,&ps)" in paint

def test_grid_never_affects_pause_or_scene_clipping():
    assert "SpiderPauseOverlay_ExcludeFromRegion(game_window,visible_region)" in DIM
    assert "SetWindowRgn(window,visible_region,FALSE)" in DIM
    assert "WS_EX_TRANSPARENT" in DIM
    assert "FOCUS-DIM: center panel and shade destroyed before pause release" in DIM
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.index("SpiderPauseOverlay_Show(hwnd);") < pause.index("SpiderPauseDim_Show(hwnd);")
    assert pause.count("SpiderPauseDim_Hide();")==2

def test_optional_inner_alpha_panel_is_clipped_around_original_letters():
    assert "#define DIM_PANEL_ALPHA 90" in DIM
    assert "SpiderPauseOverlay_GetBounds(game_window,&box)" in DIM
    assert "SpiderPauseOverlay_ExcludeFromRegion(game_window,region)" in DIM
    assert "CreateRoundRectRgn(box.left+4,box.top+4,box.right-4,box.bottom-4,16,16)" in DIM
    assert "CombineRgn(region,region,rounded,RGN_AND)" in DIM
    assert "OffsetRgn(region,-box.left,-box.top)" in DIM
    assert "SetWindowRgn(panel,region,FALSE)" in DIM
    assert "SetLayeredWindowAttributes(panel,0,DIM_PANEL_ALPHA,LWA_ALPHA)" in DIM
    assert "dim_show_center_panel(game_window,instance,origin,width,height)" in DIM
    assert "window!=dim_panel_hwnd" in DIM
    assert "if(panel && IsWindow(panel))DestroyWindow(panel)" in DIM
    assert "if(window && IsWindow(window))DestroyWindow(window)" in DIM
