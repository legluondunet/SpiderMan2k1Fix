"""Centered transparent pause banner stays over the game without taking focus."""
from pathlib import Path
import pytest

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/"pause_overlay.c").read_text(encoding="utf-8")
HDR=(ROOT/"pause_overlay.h").read_text(encoding="utf-8")
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
EXAMPLE=(ROOT/"SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")
VIRTUAL=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
VALID=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")

def test_default_is_on_only_when_the_focus_pause_is_active():
    assert "PauseOverlay=1" in INI
    assert "PauseOverlay=1" in EXAMPLE
    assert '"PauseOverlay=1\\r\\n"' in VIRTUAL
    assert '"PauseOverlay"' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseOverlay",1,path)==1' in SRC
    assert "pause_overlay.c" in BUILD
    assert "void SpiderPauseOverlay_Show(HWND game_window, int resume_key)" in HDR

def test_overlay_uses_child_window_not_desktop_topmost_popup():
    assert "WS_CHILD|WS_CLIPSIBLINGS" in SRC
    assert "SW_SHOWNOACTIVATE" in SRC
    assert "WS_EX_NOACTIVATE|WS_EX_NOPARENTNOTIFY" in SRC
    assert "game_window,NULL,instance,NULL" in SRC
    assert "WM_MOUSEACTIVATE" in SRC
    assert "MA_NOACTIVATE" in SRC
    assert "WM_NCHITTEST" in SRC
    assert "HTTRANSPARENT" in SRC
    for unsafe in ("WS_EX_TOPMOST","HWND_TOPMOST","SetForegroundWindow",
                   "SetActiveWindow","SetFocus(","DirectDrawCreate","VirtualProtect",
                   "IDirectSoundBuffer_", "BinkPause("):
        assert unsafe not in SRC

def test_instructions_are_dynamic_and_fr():
    assert 'L"PAUSE"' in SRC
    assert 'L"APPUYEZ SUR UNE TOUCHE"' in SRC
    assert 'L"APPUYEZ SUR ESPACE"' in SRC
    assert 'L"APPUYEZ SUR F9"' in SRC
    assert "overlay_resume_key==VK_SPACE" in SRC
    assert "overlay_resume_key==VK_F9" in SRC
    assert "TextOutW(" in SRC
    assert "OVERLAY_WHITE RGB(255,255,255)" in SRC
    assert "OVERLAY_DARK RGB(18,16,28)" in SRC
    assert "OVERLAY_RED RGB(221,34,45)" in SRC
    assert "FW_HEAVY,TRUE" in SRC
    assert 'L"Arial Black"' in SRC

def test_unfilled_red_frame_and_comic_text_use_real_window_region():
    # Excluding background pixels from the child HWND avoids a black box
    # when color-keyed layered windows interact with DirectDraw presentation.
    assert "CreateRoundRectRgn(0,0,width,height,16,16)" in SRC
    assert "CreateRoundRectRgn(3,3,width-3,height-3,13,13)" in SRC
    assert "CombineRgn(outline,outline,inside,RGN_DIFF)" in SRC
    assert "BeginPath(dc)" in SRC
    assert "PathToRegion(dc)" in SRC
    assert "CombineRgn(visible,visible,shifted,RGN_OR)" in SRC
    assert "SetWindowRgn(window,outline,FALSE)" in SRC
    assert "GetStockObject(HOLLOW_BRUSH)" in SRC
    assert "SetBkMode(dc,TRANSPARENT)" in SRC
    assert "x=(width-banner_width)/2" in SRC
    assert "y=(height-banner_height)/2" in SRC
    assert "ShowWindow(window,SW_SHOWNOACTIVATE)" in SRC
    assert SRC.index("SetWindowRgn(window,outline,FALSE)") < SRC.index("ShowWindow(window,SW_SHOWNOACTIVATE)")
    assert "WS_EX_LAYERED" not in SRC
    assert "SetLayeredWindowAttributes" not in SRC
    assert "LWA_COLORKEY" not in SRC
    assert "FillRect(dc,&client" not in SRC

def test_no_opaque_window_if_glyph_region_is_unavailable():
    assert "if(!overlay_apply_region(window,banner_width,banner_height))" in SRC
    assert "FOCUS-OVERLAY: window region unavailable; skipping opaque overlay" in SRC
    assert "DestroyWindow(window)" in SRC

def test_overlay_lifecycle_matches_focus_pause():
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.count("SpiderPauseOverlay_Show(hwnd,pause_key_vk);")==1
    assert pause.count("SpiderPauseOverlay_Hide();")==2
    assert pause.index("SpiderPauseOverlay_Show(hwnd,pause_key_vk);") < pause.index("while(IsWindow(hwnd)")
    assert pause.index("SpiderPauseAny_Begin(hwnd)") < pause.index("SpiderPauseOverlay_Show(hwnd,pause_key_vk);")
    assert pause.index("SpiderPauseOverlay_Hide();") < pause.index("SpiderFocusDSound_Resume();")
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line);") < CORE.index("SpiderPauseOverlay_Init(log_line);")
    assert CORE.index("if (!supported_exe())") < CORE.index("SpiderPauseOverlay_Init(log_line);")
    assert "DestroyWindow(window)" in SRC
    assert "case WM_NCDESTROY:" in SRC

def test_failures_dont_interrupt_original_pause():
    assert "FOCUS-OVERLAY: creation failed; continuing focus pause" in SRC
    assert "FOCUS-OVERLAY: class registration failed; continuing without label" in SRC
    assert "if(!overlay_enabled || !game_window || !IsWindow(game_window))return;" in SRC
