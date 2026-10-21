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
def test_large_native_spider_removed_without_changing_focus_pause():
    assert "#define ESC_DESIGN_W 1440" in P
    assert "#define ESC_DESIGN_H 1080" in P
    assert "esc_draw_motif" not in P
    assert "esc_motif_core" not in P
    assert "esc_tinted_quad" not in P
    assert "CreateEllipticRgn" not in P
    assert "esc_draw_motif(dc,tint_dc,&canvas)" not in P
    assert "canvas=*content_rect;" in P
    assert "IntersectClipRect(dc,canvas.left,canvas.top,canvas.right,canvas.bottom)" in P


def test_violet_ribbon_uses_traced_panel_and_bright_double_edge():
    assert "RGB(50,50,160),127" in P
    assert "esc_coordf(400.5f,ch,ESC_DESIGN_H)" in P
    assert "esc_coordf(675.0f,ch,ESC_DESIGN_H)" in P
    assert "esc_coordf(396.0f,ch,ESC_DESIGN_H)" in P
    assert "for(stripe=0;stripe<12;stripe++)" in P
    assert "RGB(41+3*stripe,36+2*stripe,106+stripe),24" in P
    assert "RGB(169,148,251),229" in P
    assert "RGB(231,220,255),217" in P
    assert "RGB(15,12,68),88" in P
    assert "original 8x8 texture pixels are NOT recovered" in P


def test_paused_title_is_inside_violet_panel_above_hint():
    assert 'L"PAUSED"' in P and 'L"PRESS ANY KEY TO RESUME"' in P
    assert "esc_coordf(400.5f,ch,ESC_DESIGN_H)" in P
    assert "esc_coordf(675.0f,ch,ESC_DESIGN_H)" in P
    assert "canvas.top+esc_coordf(445.0f,ch,ESC_DESIGN_H)" in P
    assert "canvas.top+esc_coordf(545.0f,ch,ESC_DESIGN_H)" in P
    assert "canvas.top+esc_coordf(161.0f,ch,ESC_DESIGN_H)" not in P
    assert "title=CreateFontW(-((62*ch+540)/1080)" in P
    assert "hint=CreateFontW(-((42*ch+540)/1080)" in P
    assert "RGB(240,47,69),4,0,0" in P
    assert "RGB(245,240,255),2,1," in P


def test_main_menu_style_red_cog_spider_icon_precedes_centered_resume_text():
    assert "pause_draw_spider_icon(dc,x,y+(length.cy-icon_size)/2,icon_size)" in P
    assert "full_width=length.cx+(with_spider?icon_size+gap:0)" in P
    assert "x+=icon_size+gap;" in P
    assert "esc_coordf(48.0f,ch,ESC_DESIGN_H)" in P
    assert "static const POINT teeth[8][4]" in P
    assert "for(leg=0;leg<8;leg++)" in P
    assert "Polygon(dc,scaled,4)" in P
    assert "CreateSolidBrush(RGB(238,34,39))" in P
    assert "CreateSolidBrush(RGB(28,7,16))" in P
    assert "CreateSolidBrush(RGB(255,174,151))" in P
    assert "Ellipse(dc," in P
    assert "SelectObject(dc,old_pen);" in P
    assert "SelectObject(dc,old_brush);" in P
    assert "long-legged silhouette" in P
    assert "ANTIALIASED_QUALITY" in P
    assert 'L"Arial Black"' in P
    assert "CreateWindow" not in P and "ExtractIconExW" not in P


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
