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
def test_original_esc_inspired_geometry_and_text():
    assert "ESC_DESIGN_W 1920" in P and "ESC_DESIGN_H 1080" in P
    assert "esc_draw_motif(dc,tint_dc,&canvas);" in P
    assert "CreatePolygonRgn(pts,count,WINDING)" in P
    assert "CreateEllipticRgn(left,top,right,bottom)" in P
    assert "RGB(50,50,160),127" in P
    assert "esc_coord(396,ch,ESC_DESIGN_H)" in P
    assert "esc_coord(675,ch,ESC_DESIGN_H)" in P
    assert "RGB(176,163,248),228" in P
    assert 'L"PAUSED"' in P and 'L"PRESS ANY KEY TO RESUME"' in P
    assert "RGB(238,42,64)" in P and "RGB(244,240,255)" in P
    assert 'L"Arial Black"' in P
    assert "ANTIALIASED_QUALITY" in P
    assert "pause_game_icon" not in P and "ExtractIconExW" not in P
    assert "canvas=*content_rect;" in P
    assert "IntersectClipRect(dc,canvas.left,canvas.top,canvas.right,canvas.bottom)" in P
    assert "Restor" in P
    # Cosmetic components are approximate; exact 8x8 border pixels are
    # deliberately not bundled/redistributed with the patch.
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
