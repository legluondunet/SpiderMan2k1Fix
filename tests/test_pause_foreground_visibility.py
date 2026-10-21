"""A paused DirectDraw frame follows the game's normal desktop stacking."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text()
P=(ROOT/"pause_surface.c").read_text()
ANY=(ROOT/"pause_any_key.c").read_text()
def paused():
    return CORE[CORE.index("static void pause_at_flip_if_background(void)"):
                CORE.index("static HRESULT WINAPI hooked_flip(")]
def test_pause_media_first_and_present_one_game_frame():
    f=paused()
    assert f.index("SpiderFocusMedia_Pause();")<f.index("SpiderPauseSurface_Begin(")
    assert f.index("SpiderFocusDSound_Pause();")<f.index("SpiderPauseSurface_Begin(")
    assert f.index("SpiderPauseSurface_Begin(")<f.index("while(IsWindow(hwnd)")
    assert "pause_present_rect.left=borderless_monitor_x" in f
    assert "pause_present_rect.top=borderless_monitor_y" in f
    assert "MsgWaitForMultipleObjectsEx(0,NULL,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE)" in f
def test_no_cross_app_popup_or_focus_switching():
    for word in ("SpiderPauseDim_ShowBackground","SpiderPauseOverlay_ShowBackground",
                 "SpiderPauseOverlay_Show(","SpiderPauseOverlay_Hide(",
                 "SpiderPauseDim_Show(","SpiderPauseDim_Hide("):
        assert word not in CORE
    for word in ("CreateWindowEx","ShowWindow(","SetWindowPos(","WS_POPUP","WS_CHILD",
                 "UpdateLayeredWindow","SetLayeredWindowAttributes"):
        assert word not in P
    assert "AlphaBlend(" in P
    assert "IDirectDrawSurface7_GetDC(" in P
def test_any_key_and_os_shortcuts_still_work():
    f=paused()
    assert "SpiderPauseAny_Begin(hwnd)" in f
    assert "SpiderPauseAny_Completed()" in f
    assert "VK_MENU || vk==VK_LMENU" in ANY
    assert "VK_LWIN || vk==VK_RWIN" in ANY
    assert "pause_key_vk" not in CORE
    assert f.count("SpiderPauseSurface_End();")==2
    assert f.count("SpiderFocusDSound_Resume();")==2
    assert f.count("SpiderFocusMedia_Resume();")==2
    assert f.index("SpiderPauseSurface_End();")<f.index("SpiderFocusDSound_Resume();")
