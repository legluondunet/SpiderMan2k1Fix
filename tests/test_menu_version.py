"""Version label structural regressions; screenshot testing remains necessary."""
from pathlib import Path
import subprocess
import shutil

ROOT=Path(__file__).resolve().parents[1]
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
    assert '"1.0.0"' in (ROOT/"version_info.h").read_text()

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
    assert '"SpiderMan2k1Fix v" "1.0.0" "-" "local"' in result.stdout
