"""Version label structural regressions; screenshot testing remains necessary."""
from pathlib import Path
import subprocess
import shutil

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"spiderman2k1fix.c").read_text()
MENU=(ROOT/"menu_version.c").read_text()
BUILD=(ROOT/"build.sh").read_text()

def test_build_uses_short_commit_and_fallback():
    assert "git rev-parse --short=7 HEAD" in BUILD
    assert "build_commit=local" in BUILD
    assert 'menu_version.c' in BUILD
    assert 'SPIDERMAN2K1FIX_COMMIT' in BUILD

def test_version_in_both_dlls():
    assert '#include "version_info.h"' in CORE
    assert '#include "version_info.h"' in MENU
    assert '#include "version_info.h"' in (ROOT/"winmm_proxy.c").read_text()
    assert '"0.0.0"' in (ROOT/"version_info.h").read_text()

def test_menu_classifier_blocks_movies_and_gameplay():
    assert "menu_active && menu_draws>=3 && movie_blits==0" in CORE
    assert "count-movie_last>3" in CORE
    assert "menu_version_movie_blits" in CORE
    assert "menu_version_last_movie_flip" in CORE

def test_surface_and_window_draw_paths():
    assert "SpiderMenuVersion_DrawToSurface(borderless_back" in CORE
    assert "SpiderMenuVersion_DrawToWindow(game_hwnd" in CORE
    assert "IDirectDrawSurface7_GetDC(" in MENU
    assert "IDirectDrawSurface7_ReleaseDC(" in MENU
    assert "content->right-pad-measure.cx" in MENU

def test_input_and_presentation_preserved():
    assert "SpiderInputFocus_OnFlip(count);" in CORE
    assert "SpiderFrameLimiter_Wait(&g_frame_limiter);" in CORE
    assert "original_blt(self,&rc,borderless_back,NULL,DDBLT_WAIT,NULL)" in CORE

def test_preprocessor_version_expansion():
    if not shutil.which("gcc"):
        return
    src='#include "version_info.h"\nconst char *v=SPIDERMAN2K1FIX_VERSION_LABEL;\n'
    result=subprocess.run(["gcc","-E","-P","-x","c","-I",str(ROOT),"-"],
                          input=src,text=True,capture_output=True,check=True)
    assert '"SpiderMan2k1Fix v" "0.0.0" "-" "local"' in result.stdout

def test_comic_style_matches_original_menu_without_changing_version():
    # Visual effect is limited to GDI glyphs in the existing menu footer.
    assert 'CreateFontA(-font_pixels,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,' in MENU
    assert 'ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,"Arial Black"' in MENU
    assert 'font_pixels=larger(10,(height*8+240)/480)' in MENU
    assert 'outline_pixels=larger(1,(height+599)/1200)' in MENU
    assert 'RGB(19,13,38)' in MENU
    assert 'RGB(138,120,214)' in MENU
    assert 'RGB(204,191,231)' not in MENU
    assert 'TextOutA(dc,x+dx*outline_pixels,y+dy*outline_pixels,label,len)' in MENU
    assert 'result=TextOutA(dc,x,y,label,len)' in MENU
    assert MENU.index('RGB(19,13,38)') < MENU.index('RGB(138,120,214)')
    assert 'const char *label=SPIDERMAN2K1FIX_VERSION_LABEL' in MENU

def test_version_footer_stays_in_menu_client_rect_and_preserves_dc_state():
    assert 'GetTextExtentPoint32A(dc,label,len,&measure)' in MENU
    assert 'x=content->right-pad-measure.cx-outline_pixels' in MENU
    assert 'y=content->bottom-pad-measure.cy-outline_pixels' in MENU
    assert 'x<content->left+pad+outline_pixels' in MENU
    assert 'SetBkMode(dc,TRANSPARENT)' in MENU
    assert 'SetTextColor(dc,old_color)' in MENU
    assert 'SetBkMode(dc,old_background)' in MENU
    assert 'SelectObject(dc,old_font)' in MENU
    assert 'if(!dc || height<240 || width<320)' in MENU
    assert 'SpiderMenuVersion_DrawToSurface' in MENU
    assert 'SpiderMenuVersion_DrawToWindow' in MENU
    assert 'WS_EX_TOPMOST' not in MENU
    assert 'CreateWindow' not in MENU
    assert 'SetForegroundWindow' not in MENU
    assert 'DirectDrawCreate' not in MENU

def test_footer_matches_reference_inactive_menu_violet_and_small_scale():
    # Visual reference 1920x1080 screenshot of menu at user workstation:
    # OPTIONS, GALLERY and TRAINING core fill (138,120,214). At a real
    # fitted 1200px-tall content area, use a 20px font instead of 25px.
    assert 'RGB(138,120,214)' in MENU
    assert 'RGB(204,191,231)' not in MENU
    assert 'font_pixels=larger(10,(height*8+240)/480)' in MENU
    assert 'outline_pixels=larger(1,(height+599)/1200)' in MENU
    assert 'RGB(19,13,38)' in MENU
