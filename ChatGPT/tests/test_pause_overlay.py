"""Focus-loss pause UI shares the exact GDI painter with F9 screenshots.

This is intentionally source-backed: no new window or copy of the panel
geometry can drift away from the existing screenshot confirmation.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
P=(ROOT/"pause_surface.c").read_text(encoding="utf-8")
H=(ROOT/"pause_surface.h").read_text(encoding="utf-8")
TOAST=(ROOT/"f9_toast.c").read_text(encoding="utf-8")
TOAST_H=(ROOT/"f9_toast.h").read_text(encoding="utf-8")
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")


def pause():
    return CORE[CORE.index("static void pause_at_flip_if_background(void)"):
                CORE.index("static HRESULT WINAPI hooked_flip(")]


def test_paused_panel_uses_exact_f9_painter_without_copying_layout():
    # Both screenshot and PAUSED use one GDI painter, not independently
    # approximated panels which may drift in appearance.
    assert '"f9_toast.h"' in P
    assert '"f9_toast.c"' not in P
    assert "SpiderF9Toast_DrawPausedPanel(dc,(DWORD)width,(DWORD)height)" in P
    assert "BOOL SpiderF9Toast_DrawPausedPanel(HDC dc,DWORD width,DWORD height);" in TOAST_H
    assert "BOOL SpiderF9Toast_DrawPausedPanel(HDC dc,DWORD width,DWORD height) {" in TOAST
    assert 'toast_draw_dc(dc,width,height,L"PAUSED",SPIDER_F9_LABEL_PAUSED)' in TOAST
    assert "static BOOL toast_draw_dc(HDC dc,DWORD w,DWORD h," in TOAST
    assert "SPIDER_F9_LABEL_PAUSED=2" in TOAST
    assert 'L"PAUSED"' in TOAST
    assert 'L"SCREENSHOT SAVED"' in TOAST
    assert 'L"SCREENSHOT FAILED"' in TOAST
    assert "f9_toast.c" in BUILD and "pause_surface.c" in BUILD


def test_paused_notification_only_one_word_without_native_resume_hint():
    painter=P[P.index("static int pause_draw_composite("):
              P.index("void SpiderPauseSurface_Init(")]
    assert "SpiderF9Toast_DrawPausedPanel(" in painter
    assert "gamepad_available" in painter  # preserved signature
    assert "(void)gamepad_available;" in painter
    assert "pause_draw_text(" not in P
    assert "pause_draw_game_icon(" not in P
    assert 'L"PRESS ANY KEY TO RESUME"' not in P
    assert 'L"PRESS KEY OR GAMEPAD BUTTON TO RESUME"' not in P
    assert "native-ESC-inspired" not in P
    assert "ESC_DESIGN_W" not in P
    assert "ESC_DESIGN_H" not in P


def test_f9_frame_is_flush_left_bottom_translucent_lavender_and_original_icon():
    # With the shared F9 painter, this is automatically true for PAUSED.
    painter=TOAST.split("static BOOL toast_draw_dc(",1)[1].split(
        "BOOL SpiderF9Toast_DrawPausedPanel(",1)[0]
    assert "x=0; /* Flush left edge" in painter
    assert "y=(int)h-toast_max(16,(int)h/50)-box_h;" in painter
    assert "SPIDER_F9_PANEL_ALPHA 165u" in TOAST
    assert "toast_draw_translucent_panel(dc,&panel)" in painter
    assert "RGB(169,148,251)" in painter
    assert "RGB(245,240,255)" in painter
    assert "RECT top={panel.left,panel.top,panel.right,panel.top+1};" in painter
    assert "RECT right={panel.right-1,panel.top+1,panel.right,panel.bottom-1};" in painter
    assert "RECT bottom={panel.left,panel.bottom-1,panel.right,panel.bottom};" in painter
    assert "RECT left=" not in painter
    assert "toast_original_big_icon()" in painter
    assert "DrawIconEx(dc,x+pad,icon_y,game_icon," in painter
    assert 'L"Arial Black"' in painter


def test_optional_full_scene_dim_remains_independent_and_restorable():
    assert "static int enabled_dim=1,enabled_banner=1,opacity=60;" in P
    assert "if(enabled_dim && opacity>0)" in P
    assert "pause_blend_color(dc,tint_dc,0,0,width,height,RGB(0,0,0)" in P
    assert "if(!enabled_banner)return 1;" in P
    assert 'GetPrivateProfileIntA("Input","PauseDimOpacity",60,path)' in P
    assert "opacity<0 || opacity>90" in P
    assert "SpiderPauseSurface_End(void)" in H
    assert "BitBlt(saved_dc,0,0,width,height,dc,0,0,SRCCOPY)" in P
    assert "present(primary,(LPRECT)primary_dest,back,NULL,DDBLT_WAIT,NULL)" in P
    assert "IDirectDrawSurface7_AddRef(back)" in P
    assert "IDirectDrawSurface7_Release(surface)" in P


def test_pause_does_not_create_any_overlay_window_or_touch_foreground():
    assert '"pause_surface.h"' in CORE
    assert '"pause_overlay.h"' not in CORE
    assert '"pause_dim.h"' not in CORE
    assert "pause_surface.c" in BUILD and "pause_overlay.c" not in BUILD
    for word in ("CreateWindow", "WS_CHILD", "WS_EX_LAYERED",
                 "UpdateLayeredWindow", "SetForegroundWindow",
                 "SetActiveWindow", "SendInput(", "IDirectInputDevice"):
        assert word not in P


def test_focus_pause_audio_keyboard_and_sdl3_wake_unchanged():
    f=pause()
    assert f.index("SpiderFocusMedia_Pause();") < f.index("SpiderPauseSurface_Begin(")
    assert f.index("SpiderFocusDSound_Pause();") < f.index("SpiderPauseSurface_Begin(")
    assert f.index("SpiderPauseSurface_Begin(") < f.index("while(IsWindow(hwnd)")
    assert "SpiderPauseSurface_End();" in f
    assert "SpiderFocusDSound_Resume();" in f
    assert "SpiderFocusMedia_Resume();" in f
    assert "SpiderPauseAny_Begin(hwnd)" in f
