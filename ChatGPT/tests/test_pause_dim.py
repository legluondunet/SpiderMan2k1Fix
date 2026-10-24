"""No desktop alpha windows: 50% shade, optional 16px grid, frozen frame."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
P=(ROOT/"pause_surface.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
INI=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini").read_text()
EXAMPLE=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
def test_original_ini_controls_retained():
    for src in (INI,EXAMPLE):
        for key in ("PauseDim=1","PauseDimOpacity=50","PauseGrid=0"):assert key in src
    for key in ("PauseDim","PauseDimOpacity","PauseGrid","PauseOverlay"):
        assert '"'+key+'"' in VALID and '"Input","'+key+'"' in P
    assert '"PauseDimOpacity=50\\r\\n"' in GEN
    assert '"PauseGrid=0\\r\\n"' in GEN
def test_alpha_blend_and_opt_in_grid_are_drawn_to_existing_directdraw_dc():
    assert "#define PAUSE_GRID_STEP 16" in P
    assert "AlphaBlend(dest,x,y,width,height,source,0,0,1,1,blend)" in P
    assert "BLENDFUNCTION blend={AC_SRC_OVER,0,alpha,0}" in P
    assert "(opacity*255+50)/100" in P
    assert "enabled_dim=1,enabled_grid=0,enabled_banner=1,opacity=50;" in P
    assert 'GetPrivateProfileIntA("Input","PauseDimOpacity",50,path)' in P
    assert "const BLENDFUNCTION grid_blend={AC_SRC_OVER,0,28,0};" in P
    assert "RGB(100,104,124)" in P
    assert "enabled_dim && enabled_grid" in P
    assert "canvas=*content_rect;" in P
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
