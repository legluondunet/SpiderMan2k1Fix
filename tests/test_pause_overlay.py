"""Atomic comic pause banner: no shaped-child glyph mask or desktop overlay."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/"pause_overlay.c").read_text(encoding="utf-8")
HDR=(ROOT/"pause_overlay.h").read_text(encoding="utf-8")
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
GEN=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
VALID=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")


def test_enabled_only_during_real_focus_pause():
    assert "PauseOverlay=1" in INI
    assert '"PauseOverlay=1\\r\\n"' in GEN
    assert '"PauseOverlay"' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseOverlay",1,path)==1' in SRC
    assert "pause_overlay.c" in BUILD
    assert "void SpiderPauseOverlay_Show(HWND game_window)" in HDR
    assert "SpiderPauseOverlay_GameForeground(hwnd)" in CORE


def test_owned_popup_rather_than_legacy_shaped_child():
    assert "WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|" in SRC
    assert "WS_EX_TOOLWINDOW|WS_EX_NOPARENTNOTIFY" in SRC
    assert "WS_POPUP" in SRC
    assert "game_window,NULL,instance,NULL" in SRC
    assert "WS_CHILD" not in SRC
    assert "GetWindowRgn" not in SRC
    assert "PathToRegion" not in SRC
    assert "BeginPath" not in SRC
    assert "WM_MOUSEACTIVATE" in SRC
    assert "MA_NOACTIVATE" in SRC
    assert "HTTRANSPARENT" in SRC
    for bad in ("HWND_TOPMOST","WS_EX_TOPMOST","SetForegroundWindow(",
                "SetActiveWindow(", "SetFocus(", "DirectDrawCreate(",
                "BinkPause(", "IDirectSoundBuffer_"):
        assert bad not in SRC


def test_all_content_precomposed_in_one_premultiplied_alpha_image():
    assert "CreateDIBSection(" in SRC
    assert "info.bmiHeader.biBitCount=32" in SRC
    assert "info.bmiHeader.biHeight=-height" in SRC
    assert "UpdateLayeredWindow(window,screen,&origin,&size,dc,&src,0,&blend,ULW_ALPHA)" in SRC
    assert "BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA}" in SRC
    assert "a=(p==base)?PANEL_ALPHA:255" in SRC
    assert "pixels[y*width+x]=(a<<24)|p" in SRC
    assert "#define PANEL_ALPHA 188" in SRC
    assert "RoundRect(dc,1,1,width-1,height-1,20,20)" in SRC
    assert "CreatePen(PS_SOLID,2,RGB(174,47,64))" in SRC
    assert "SetLayeredWindowAttributes(" not in SRC
    assert "LWA_COLORKEY" not in SRC
    assert "PANEL_ALPHA" in SRC
    assert "if(!render_banner(window,banner,OVERLAY_WIDTH,OVERLAY_HEIGHT))" in SRC
    assert SRC.index("render_banner(window,banner") < SRC.index("ShowWindow(window,SW_SHOWNOACTIVATE)")


def test_solid_english_comic_type_and_spacing():
    assert 'L"PAUSE"' in SRC
    assert 'L"PRESS ANY KEY TO RESUME"' in SRC
    assert 'L"Arial Black"' in SRC
    assert "FW_HEAVY,TRUE" in SRC
    assert "FW_HEAVY,FALSE" in SRC
    assert "RGB(255,255,255)" in SRC
    assert "RGB(8,7,16)" in SRC
    assert "text_at(dc,title,L\"PAUSE\",width,13)" in SRC
    assert "text_at(dc,hint,L\"PRESS ANY KEY TO RESUME\",width,66)" in SRC
    assert "APPUYEZ" not in SRC
    assert "PRESS F9" not in SRC and "PRESS SPACE" not in SRC


def test_strict_active_foreground_guard_and_cleanup():
    assert "GetForegroundWindow()==game_window" in SRC
    assert "GetActiveWindow()==game_window" in SRC
    assert "IsWindowVisible(game_window)" in SRC
    assert "IsIconic(game_window)" in SRC
    assert "if(!overlay_enabled || !SpiderPauseOverlay_GameForeground(game_window))return;" in SRC
    assert "if(!SpiderPauseOverlay_GameForeground(game_window)) {" in SRC
    assert "DestroyWindow(window)" in SRC
    assert "void SpiderPauseOverlay_Hide(void)" in SRC
    assert "SpiderPauseOverlay_ExcludeFromRegion" in SRC


def test_center_rectangle_is_excluded_whole_not_per_glyph():
    assert "CreateRoundRectRgn(bounds.left,bounds.top,bounds.right,bounds.bottom,20,20)" in SRC
    assert "CombineRgn(visible_region,visible_region,banner,RGN_DIFF)" in SRC
    assert "GetWindowRect(overlay_hwnd,&desktop)" in SRC
    assert "MapWindowPoints(HWND_DESKTOP,game_window,pt,2)" in SRC


def test_focus_only_lifecycle_keeps_game_paused():
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.count("SpiderPauseOverlay_Show(hwnd);")==1
    assert pause.count("SpiderPauseDim_Show(hwnd);")==1
    assert pause.count("SpiderPauseOverlay_Hide();")==3
    assert pause.count("SpiderPauseDim_Hide();")==3
    assert pause.index("SpiderPauseAny_Begin(hwnd)") < pause.index("SpiderFocusMedia_Pause();")
    assert pause.index("SpiderPauseOverlay_Show(hwnd);") > pause.index("while(IsWindow(hwnd)")
    assert 'if(SpiderPauseOverlay_GameForeground(hwnd))' in pause
    assert "SpiderPauseAny_Completed()" in pause
    assert "SpiderPauseAny_DiscardGameKeyMessage(&msg,hwnd)" in pause
    assert "PostQuitMessage((int)msg.wParam)" in pause
