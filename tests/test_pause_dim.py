"""Pause-only 40% black shade must never obscure input or original banner."""
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
    assert "PauseDim=1" in EXAMPLE
    assert '"PauseDim=1\\r\\n"' in GEN
    assert '"PauseDim"' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseDim",1,path)==1' in DIM
    assert "pause_dim.c" in BUILD
    assert "void SpiderPauseDim_Init(void (*log_fn)(const char *));" in HEADER

def test_layered_child_uses_uniform_alpha_not_failed_color_key():
    assert "DIM_ALPHA 102" in DIM
    assert "WS_EX_LAYERED|WS_EX_TRANSPARENT|" in DIM
    assert "WS_EX_NOACTIVATE|WS_EX_NOPARENTNOTIFY" in DIM
    assert "WS_CHILD|WS_CLIPSIBLINGS" in DIM
    assert "game_window,NULL,instance,NULL" in DIM
    assert "SetLayeredWindowAttributes(window,0,DIM_ALPHA,LWA_ALPHA)" in DIM
    assert "LWA_COLORKEY" not in DIM
    assert "BLACK_BRUSH" in DIM
    assert "FillRect(dc,&bounds,(HBRUSH)GetStockObject(BLACK_BRUSH))" in DIM
    assert DIM.index("SetLayeredWindowAttributes(window,0,DIM_ALPHA,LWA_ALPHA)") < DIM.index("ShowWindow(window,SW_SHOWNOACTIVATE)")
    assert "FOCUS-DIM: alpha unavailable; skipping opaque layer" in DIM
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
    assert 'L"APPUYEZ SUR UNE TOUCHE"' in OVERLAY
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.count("SpiderPauseDim_Show(hwnd);")==1
    assert pause.count("SpiderPauseOverlay_Show(hwnd,pause_key_vk);")==1
    assert pause.index("SpiderPauseDim_Show(hwnd);") < pause.index("SpiderPauseOverlay_Show(hwnd,pause_key_vk);")
    assert pause.count("SpiderPauseDim_Hide();")==2
    assert pause.count("SpiderPauseOverlay_Hide();")==2
    for portion in pause.split("SpiderPauseOverlay_Hide();")[1:]:
        assert portion.index("SpiderPauseDim_Hide();") < portion.index("SpiderFocusDSound_Resume();")
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line);") < CORE.index("SpiderPauseDim_Init(log_line);")
    assert CORE.index("if (!supported_exe())") < CORE.index("SpiderPauseDim_Init(log_line);")
    assert "SpiderPauseDim_Hide();" in CORE

def test_game_pause_and_audio_do_not_depend_on_shade_creation():
    assert "if(!dim_enabled || !game_window || !IsWindow(game_window))return;" in DIM
    assert "FOCUS-DIM: creation failed; continuing pause without shade" in DIM
    assert "FOCUS-DIM: class registration failed; skipping dim layer" in DIM
    assert "SpiderFocusMedia_Pause();" in CORE
    assert "SpiderFocusDSound_Pause();" in CORE
    assert "SpiderPauseAny_Begin(hwnd)" in CORE
