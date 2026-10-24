"""The real DirectDraw PAUSED surface has no grid and a configurable
optional black shade (default 60%, 0 disables shade but not pause/banner)."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
P=(ROOT/"pause_surface.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
EXAMPLE=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
LEGACY_FILES=("pause_dim.c","pause_dim.h","pause_overlay.c","pause_overlay.h")


def test_grid_removed_from_live_renderer_and_ini_schema():
    assert "PauseGrid" not in P
    assert "PauseGrid" not in VALID
    assert "PauseGrid" not in GEN
    assert all(not (ROOT/name).exists() for name in LEGACY_FILES)
    assert "PAUSE_GRID_STEP" not in P
    assert "enabled_grid" not in P
    assert "grid_blend" not in P
    assert "PauseGrid=" not in EXAMPLE
    assert "pause_surface.c" in BUILD
    assert "pause_dim.c" not in BUILD


def test_default_60_percent_opacity_is_optional_in_ini():
    assert "static int enabled_dim=1,enabled_banner=1,opacity=60;" in P
    assert "opacity=60;" in P
    assert 'GetPrivateProfileIntA("Input","PauseDimOpacity",60,path)' in P
    assert "if(opacity<0 || opacity>90)opacity=60;" in P
    assert '"PauseDimOpacity"' in VALID
    assert 'return spider_ini_uint(value,0,90);' in VALID
    assert '"PauseDimOpacity=' not in GEN
    assert "PauseDimOpacity=" not in EXAMPLE
    assert '{"Input","PauseDimOpacity"}' not in GEN  # preserve custom values at migration
    assert "if(deprecated_ini_key(e->section,e->key))continue;" in GEN


def test_opacity_zero_only_removes_dim_not_pause_and_panel():
    assert "if(enabled_dim && opacity>0)" in P
    assert "enabled_banner=1;" in P
    assert "if(!enabled_banner)return 1;" in P
    assert "SpiderPauseAny_Begin(hwnd)" in CORE
    assert "SpiderFocusMedia_Pause();" in CORE
    assert "SpiderFocusDSound_Pause();" in CORE
    assert "GetDC unsupported; keep pause without visual" in P
    assert "composition unavailable; game pause still functional" in P


def test_alpha_blend_on_existing_directdraw_surface():
    assert "AlphaBlend(dest,x,y,width,height,source,0,0,1,1,blend)" in P
    assert "BLENDFUNCTION blend={AC_SRC_OVER,0,alpha,0}" in P
    assert "(opacity*255+50)/100" in P
    assert "canvas=*content_rect;" in P
    assert "BG_DITHER_PERIOD" not in P
    assert "WS_EX_TRANSPARENT" not in P
    assert "-lmsimg32" in BUILD
    assert "SpiderPauseSurface_End();" in CORE
