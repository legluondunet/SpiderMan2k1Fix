"""PR #15: under Wine the owned alpha overlays must NEVER stay over desktop apps."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
OVERLAY=(ROOT/"pause_overlay.c").read_text(encoding="utf-8")
DIM=(ROOT/"pause_dim.c").read_text(encoding="utf-8")


def paused_flip():
    return CORE[CORE.index("static void pause_at_flip_if_background(void)"):
                CORE.index("static HRESULT WINAPI hooked_flip(")]


def test_keep_media_paused_in_background_but_hide_visuals():
    pause=paused_flip()
    assert "SpiderFocusMedia_Pause();" in pause
    assert "SpiderFocusDSound_Pause();" in pause
    assert "while(IsWindow(hwnd) && InterlockedCompareExchange(&pause_latched,0,0)==1)" in pause
    assert pause.index("SpiderFocusMedia_Pause();") < pause.index("while(IsWindow(hwnd)")
    assert pause.index("SpiderFocusDSound_Pause();") < pause.index("while(IsWindow(hwnd)")
    assert pause.index("SpiderPauseOverlay_Show(hwnd);") > pause.index("while(IsWindow(hwnd)")
    assert pause.index("SpiderPauseDim_Show(hwnd);") > pause.index("while(IsWindow(hwnd)")
    assert "if(GetForegroundWindow()==hwnd)" in pause
    assert "if(!visuals_visible)" in pause
    assert "SpiderPauseOverlay_Show(hwnd);\n        SpiderPauseDim_Show(hwnd);" in pause
    assert "else if(visuals_visible)" in pause
    assert "SpiderPauseDim_Hide();\n      SpiderPauseOverlay_Hide();" in pause
    assert 'FOCUS-PAUSE: overlay hidden (game background)' in pause
    assert "MsgWaitForMultipleObjectsEx(0,NULL,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE)" in pause


def test_background_always_cleans_up_both_popups_before_audio_resume():
    pause=paused_flip()
    assert pause.count("SpiderPauseDim_Hide();")==3
    assert pause.count("SpiderPauseOverlay_Hide();")==3
    assert pause.count("SpiderFocusDSound_Resume();")==2
    assert pause.count("SpiderFocusMedia_Resume();")==2
    assert "PostQuitMessage((int)msg.wParam)" in pause
    assert pause.index("SpiderPauseAny_Begin(hwnd)") < pause.index("SpiderFocusMedia_Pause();")
    assert "if(SpiderPauseAny_Completed())" in pause
    assert "SpiderPauseAny_DiscardGameKeyMessage(&msg,hwnd)" in pause


def test_alpha_popups_and_label_refuse_background_creation():
    assert "GetForegroundWindow()!=game_window)return;" in DIM
    assert "GetForegroundWindow()!=game_window)return;" in OVERLAY
    assert "GetForegroundWindow()!=game_window ||" in DIM
    assert "WS_EX_NOACTIVATE" in DIM
    assert "WS_EX_NOACTIVATE" in OVERLAY
    for unsafe in ("SetForegroundWindow(", "HWND_TOPMOST", "WS_EX_TOPMOST"):
        assert unsafe not in DIM
        assert unsafe not in OVERLAY


def test_grid_never_drawn_behind_hint_on_legacy_compositor():
    paint=DIM[DIM.index("case WM_PAINT:"):DIM.index("case WM_MOUSEACTIVATE:")]
    assert "ExcludeClipRect(dc,center.left-8,center.top-8," in paint
    assert "SpiderPauseOverlay_GetBounds(GetWindow(window,GW_OWNER),&center)" in paint
    assert "for(x=DIM_GRID_STEP;" in paint
    assert paint.index("ExcludeClipRect(") < paint.index("for(x=DIM_GRID_STEP;")
    assert "SetLayeredWindowAttributes(window,0,dim_alpha,LWA_ALPHA)" in DIM
    assert "SetLayeredWindowAttributes(panel,0,DIM_PANEL_ALPHA,LWA_ALPHA)" in DIM
    assert "GetWindowRgn(overlay_hwnd,banner_region)" in OVERLAY
    assert "for(dy=-5;dy<=5 && ok;dy++)" in OVERLAY
    assert "CreateFontW(-25,0,0,0,FW_HEAVY,FALSE" in OVERLAY


def test_one_english_instruction_only():
    assert 'L"PRESS ANY KEY TO RESUME"' in OVERLAY
    assert 'L"PAUSE"' in OVERLAY
    assert "APPUYEZ" not in OVERLAY
    assert "PRESS SPACE" not in OVERLAY
    assert "PRESS F9" not in OVERLAY
    assert "pause_key_vk" not in CORE
