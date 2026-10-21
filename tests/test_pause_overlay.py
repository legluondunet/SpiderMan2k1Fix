"""Passive pause banner stays inside the game window and never takes input focus."""
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
    assert "WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS" in SRC
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
    assert 'L"Appuyez sur une touche"' in SRC
    assert 'L"Appuyez sur Espace"' in SRC
    assert 'L"Appuyez sur F9"' in SRC
    assert "overlay_resume_key==VK_SPACE" in SRC
    assert "overlay_resume_key==VK_F9" in SRC
    assert "DrawTextW(" in SRC

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
