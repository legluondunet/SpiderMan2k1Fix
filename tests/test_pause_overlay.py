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
def test_native_esc_spider_uses_exact_traced_outer_quads():
    # Values are original 1440x1080 XY coordinates, before any 4:3
    # content_rect mapping. All eight *unique* outer geometry quads were
    # measured; duplicated back-facing triangles should NOT be overlaid.
    assert "#define ESC_DESIGN_W 1440" in P
    assert "#define ESC_DESIGN_H 1080" in P
    assert "esc_tinted_quad(dc,tint,view,left[i],mirror" in P
    assert "for(mirror=0;mirror<2;mirror++)" in P
    assert "for(i=0;i<4;i++)" in P
    assert "{340.3f,58.5f},{219.4f,0.0f},{0.0f,0.0f},{255.9f,234.0f}" in P
    assert "{255.9f,234.0f},{0.0f,0.0f},{0.0f,306.0f},{216.6f,333.0f}" in P
    assert "{210.9f,625.5f},{0.0f,657.0f},{0.0f,864.0f},{247.5f,810.0f}" in P
    assert "{306.6f,904.5f},{0.0f,904.5f},{0.0f,1053.0f},{430.3f,1053.0f}" in P
    assert "CreatePolygonRgn(pts,4,WINDING)" in P
    assert "RGB(152,64,32),85" in P
    assert "src=ZERO, dst=INVSRCCOLOR" in P
    assert "has NOT been traced in full" in P
    assert "CreateEllipticRgn(left,top,right,bottom)" in P
    assert "upper_outer" not in P and "lower_outer" not in P
    assert "esc_motif_polygon" not in P


def test_traced_panel_and_text_stay_in_game_surface():
    assert "RGB(50,50,160),127" in P
    assert "esc_coordf(396.0f,ch,ESC_DESIGN_H)" in P
    assert "esc_coordf(675.0f,ch,ESC_DESIGN_H)" in P
    assert "RGB(176,163,248),228" in P
    assert 'L"PAUSED"' in P and 'L"PRESS ANY KEY TO RESUME"' in P
    assert "RGB(238,42,64),3" in P and "RGB(244,240,255),1" in P
    assert 'L"Arial Black"' in P
    assert "ANTIALIASED_QUALITY" in P
    assert "pause_game_icon" not in P and "ExtractIconExW" not in P
    assert "canvas=*content_rect;" in P
    assert "IntersectClipRect(dc,canvas.left,canvas.top,canvas.right,canvas.bottom)" in P
    assert "RestoreDC(dc,saved)" in P
    assert "have not" in P and "recovered" in P

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
