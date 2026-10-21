"""No desktop alpha windows: 55% dim and 16px grid in frozen game frame."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
P=(ROOT/"pause_surface.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text()
EXAMPLE=(ROOT/"SpiderMan2k1Fix.generated-example.ini").read_text()
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
def test_original_ini_controls_retained():
    for src in (INI,EXAMPLE):
        for key in ("PauseDim=1","PauseDimOpacity=55","PauseGrid=1"):assert key in src
    for key in ("PauseDim","PauseDimOpacity","PauseGrid","PauseOverlay"):
        assert '"'+key+'"' in VALID and '"Input","'+key+'"' in P
    assert '"PauseDimOpacity=55\\r\\n"' in GEN
    assert '"PauseGrid=1\\r\\n"' in GEN
def test_alpha_blend_and_grid_are_drawn_to_existing_directdraw_dc():
    assert "#define PAUSE_GRID_STEP 16" in P
    assert "AlphaBlend(dest,x,y,width,height,source,0,0,1,1,blend)" in P
    assert "BLENDFUNCTION blend={AC_SRC_OVER,0,alpha,0}" in P
    assert "(opacity*255+50)/100" in P
    assert "ExcludeClipRect(dc,box_left-5,box_top-5," in P
    assert "const BLENDFUNCTION grid_blend={AC_SRC_OVER,0,28,0};" in P
    assert "RGB(100,104,124)" in P
    assert "for(x=grid_left+PAUSE_GRID_STEP;x<grid_right;x+=PAUSE_GRID_STEP)" in P
    assert "for(y=grid_top+PAUSE_GRID_STEP;y<grid_bottom;y+=PAUSE_GRID_STEP)" in P
    assert "grid_bottom-grid_top," in P and "grid_right-grid_left,1," in P
    assert "IntersectClipRect(dc,grid_left,grid_top,grid_right,grid_bottom)" in P
    assert "&pause_present_rect,&pause_content_rect," in CORE
    assert "menu_map.left" in CORE and "game_map.left" in CORE
    assert "BG_DITHER_PERIOD" not in P
    assert "WS_EX_TRANSPARENT" not in P
    assert "pause_surface.c" in BUILD and "-lmsimg32" in BUILD
def test_failure_does_not_change_input_or_audio_pause():
    assert "GetDC unsupported; keep pause without visual" in P
    assert "composition unavailable; game pause still functional" in P
    assert "SpiderFocusMedia_Pause();" in CORE
    assert "SpiderFocusDSound_Pause();" in CORE
    assert "SpiderPauseAny_Begin(hwnd)" in CORE
    assert "SpiderPauseSurface_End();" in CORE
