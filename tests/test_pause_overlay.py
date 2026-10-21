"""DirectDraw-only comic pause renderer (no overlay HWNDs)."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
P=(ROOT/"pause_surface.c").read_text()
H=(ROOT/"pause_surface.h").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
def pause():
    return CORE[CORE.index("static void pause_at_flip_if_background(void)"):
                CORE.index("static HRESULT WINAPI hooked_flip(")]
def test_no_new_overlay_windows_or_popup_modules_compiled():
    assert '"pause_surface.h"' in CORE
    assert '"pause_overlay.h"' not in CORE
    assert '"pause_dim.h"' not in CORE
    assert "pause_surface.c" in BUILD
    assert "pause_overlay.c" not in BUILD and "pause_dim.c" not in BUILD
    for word in ("CreateWindow","WS_CHILD","WS_EX_LAYERED","UpdateLayeredWindow","SetForegroundWindow"):
        assert word not in P
def test_english_solid_comic_text_drawn_directly():
    assert 'L"PAUSE"' in P and 'L"PRESS ANY KEY TO RESUME"' in P
    assert 'L"Arial Black"' in P
    assert "RGB(255,255,255)" in P
    assert "RGB(178,45,63)" in P
    assert "RoundRect(" in P
    assert "RGB(11,16,26),232" in P
    assert "#define PAUSE_PANEL_W 520" in P
    assert "#define PAUSE_PANEL_H 128" in P
    assert "ANTIALIASED_QUALITY" in P
    assert "RGB(224,227,234)" in P
    assert "width/2-75,box_top+67" in P
    assert "#define PAUSE_ICON_SIZE 96" in P
    assert "ExtractIconExW(exe,0,&large,&small,1)" in P
    assert "pause_draw_icon_watermark(dc,box_left+27," in P
    assert "DrawIconEx(black_dc" in P and "DrawIconEx(white_dc" in P
    assert "AC_SRC_ALPHA" in P and "CreateDIBSection(" in P
    assert "if(!pause_game_icon)return 0;" in P
    assert "game icon watermark unavailable; retaining plain banner" in P
def test_frame_saved_presented_and_restored_on_release():
    for word in ("IDirectDrawSurface7_GetDC(back,&dc)","IDirectDrawSurface7_ReleaseDC(back,dc)",
                 "BitBlt(saved_dc,0,0,width,height,dc,0,0,SRCCOPY)",
                 "present(primary,(LPRECT)primary_dest,back,NULL,DDBLT_WAIT,NULL)",
                 "IDirectDrawSurface7_AddRef(back)","IDirectDrawSurface7_Release(surface)"):
        assert word in P
    assert "SpiderPauseSurface_Begin(" in H and "SpiderPauseSurface_End(void)" in H
def test_paused_frame_is_displayed_without_focus():
    f=pause()
    assert f.index("SpiderPauseSurface_Begin(") < f.index("while(IsWindow(hwnd)")
    assert "SpiderPauseSurface_End();" in f
    assert "SpiderPauseOverlay_Show" not in f
    assert "SpiderPauseDim_Show" not in f
