"""Native F9 screenshot notification regression checks.

The asynchronous worker is the only producer, and the DirectDraw Flip game
thread is the only renderer. The overlay is composited on a distinct offscreen surface BEFORE a single
PRIMARY blit. PNG encoding reads only the pristine 3D BACKBUFFER. Failed or
queued captures must never trigger a success toast.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
ENC = (ROOT/"f9_png.c").read_text(encoding="utf-8")
TOAST = (ROOT/"f9_toast.c").read_text(encoding="utf-8")
HEADER = (ROOT/"f9_toast.h").read_text(encoding="utf-8")
BUILD = (ROOT/"build.sh").read_text(encoding="utf-8")


def test_screenshot_confirmation_is_english_and_includes_full_path():
    assert 'L"Screenshot saved to:"' in TOAST
    assert 'SpiderF9Toast_PublishSavedPath(const WCHAR *full_path)' in TOAST
    assert 'memcpy(toast_path,full_path,(len+1u)*sizeof(WCHAR));' in TOAST
    assert 'L"\\\\' not in TOAST or "toast_wrap_path" in TOAST
    assert "toast_wrap_path(" in TOAST
    assert "DrawTextW(" not in TOAST  # No truncating label/path drawing
    assert "SPIDER_F9_TOAST_MS 2000u" in TOAST


def test_confirmation_published_only_after_image_is_actually_saved():
    commit = "if(MoveFileExW(temp,dest,MOVEFILE_WRITE_THROUGH)) {"
    start = ENC.index(commit)
    part = ENC[start:ENC.index("if(!saved)",start)]
    assert "spider_png_log(job,msg);" in part
    assert "SpiderF9Toast_PublishSavedPath(dest);" in part
    assert part.index("SpiderF9Toast_PublishSavedPath(dest);") > part.index(commit)
    assert "SpiderF9Toast_PublishSavedPath" not in ENC[:start]
    assert "GetFileAttributesW(full_path)" in TOAST
    assert "SpiderF9Toast_PublishSavedPath" in HEADER


def test_toast_is_composited_before_one_primary_blt_without_touching_png_source():
    start = CORE.index("static HRESULT WINAPI hooked_flip(")
    flip = CORE[start:CORE.index("static void instrument_surface(",start)]
    assert "SpiderF9Toast_CompositeFrame(borderless_back,game_hwnd,desktop_w,desktop_h)" in flip
    assert "hr=original_blt(self,&rc,present_source,NULL,DDBLT_WAIT,NULL);" in flip
    assert flip.index("SpiderF9Toast_CompositeFrame(") < flip.index(
        "hr=original_blt(self,&rc,present_source,NULL,DDBLT_WAIT,NULL);")
    assert "SpiderF9Toast_DrawAfterPresent" not in flip
    assert "IDirectDrawSurface7_GetDDInterface(back,(void**)&dd)" in TOAST
    assert "DDSCAPS_OFFSCREENPLAIN" in TOAST
    assert "IDirectDrawSurface7_Blt(toast_stage,NULL,back,NULL,DDBLT_WAIT,NULL)" in TOAST
    assert "IDirectDrawSurface7_GetDC(toast_stage,&dc)" in TOAST
    assert "IDirectDrawSurface7_ReleaseDC(toast_stage,dc)" in TOAST
    assert "IDirectDrawSurface7_GetDC(back,&dc)" not in TOAST
    assert "IDirectDrawSurface7_GetDC(primary,&dc)" not in TOAST
    assert "GetDC(window)" not in TOAST
    assert "SpiderF9Toast_CompositeFrame(" in HEADER


def test_staging_surface_lifetime_and_failure_preserves_gameplay():
    assert "toast_discard_stage();" in TOAST
    assert "toast_stage_width!=width || toast_stage_height!=height" in TOAST
    assert "DDSCAPS_SYSTEMMEMORY" in TOAST
    assert "DDSCAPS_VIDEOMEMORY" in TOAST
    assert "SpiderF9Toast_ResetPresentation();" in CORE
    assert "hr=original_blt(self,&rc,borderless_back,NULL,DDBLT_WAIT,NULL);" in CORE
    assert "return NULL; /* Present the untouched game frame if composing fails." in TOAST
    assert "GetForegroundWindow()!=window" in TOAST



def test_thread_safe_success_expiration_and_no_window_focus_intrusion():
    assert "static SRWLOCK toast_mutex=SRWLOCK_INIT;" in TOAST
    assert "AcquireSRWLockExclusive(&toast_mutex)" in TOAST
    assert "ReleaseSRWLockExclusive(&toast_mutex)" in TOAST
    assert "AcquireSRWLockShared(&toast_mutex)" in TOAST
    assert "ReleaseSRWLockShared(&toast_mutex)" in TOAST
    assert "(LONG)(expires-GetTickCount())>0" in TOAST
    assert "GetForegroundWindow()!=window" in TOAST
    assert "CreateWindow" not in TOAST
    assert "GetAsyncKeyState" not in TOAST
    assert "SetWindowPos" not in TOAST
    assert "f9_toast.c" in BUILD


def test_existing_game_hooks_and_png_image_bytes_unchanged():
    assert "0x0010A1DCu,{0xE8,0xFF,0x91,0xFF,0xFF}" in CORE
    assert "0x0010A62Bu,{0xE8,0xB0,0x8D,0xFF,0xFF}" in CORE
    assert "SpiderF9PNG_Queue(&desc,log_line);" in CORE
    assert "pause_at_flip_if_background();" in CORE
    assert "maybe_correct_adaptive_hud(" in CORE
    assert "memcpy(job->pixels+(SIZE_T)y*row_bytes," in ENC
    assert "CreateThread(NULL,0,spider_png_thread,job,0,NULL)" in ENC


def test_failed_png_is_reported_and_final_file_is_verified():
    assert "SpiderF9Toast_PublishError" in HEADER
    assert 'L"Screenshot failed:"' in TOAST
    assert "spider_png_verify(dest)" in ENC
    assert "GetFileSizeEx(f,&length)" in ENC
    assert "ReadFile(f,prefix,sizeof(prefix),&got,NULL)" in ENC
    assert "SpiderF9Toast_PublishSavedPath(dest);" in ENC
    assert ENC.index("spider_png_verify(dest)") < ENC.index("SpiderF9Toast_PublishSavedPath(dest);")
    assert "DeleteFileW(dest);" in ENC
    assert 'L"Access denied while saving PNG"' in ENC
    assert 'L"Disk full while saving PNG"' in ENC


def test_heading_uses_real_wide_string_length_and_pause_palette():
    assert 'TextOutW(dc,x+pad,y+pad,heading,' in TOAST
    assert '(int)wcslen(heading));' in TOAST
    assert 'sizeof(heading)/sizeof(heading[0])' not in TOAST
    assert 'RGB(50,50,160)' in TOAST
    assert 'RGB(169,148,251)' in TOAST
    assert 'RGB(231,220,255)' in TOAST
    assert 'RGB(245,240,255)' in TOAST

def test_pause_style_transparency_uses_rounded_alpha_blend_offscreen():
    assert "SPIDER_F9_PANEL_ALPHA 165u" in TOAST
    assert "CreateCompatibleBitmap(dc,1,1)" in TOAST
    assert "CreateRoundRectRgn(" in TOAST
    assert "ExtSelectClipRgn(dc,rounded,RGN_AND)" in TOAST
    assert "AlphaBlend(dc,panel->left,panel->top," in TOAST
    assert "GetStockObject(HOLLOW_BRUSH)" in TOAST
    assert "CreateSolidBrush(RGB(50,50,160))" in TOAST
    assert "SpiderF9Toast_CompositeFrame(" in TOAST
    assert "IDirectDrawSurface7_Blt(toast_stage,NULL,back,NULL,DDBLT_WAIT,NULL)" in TOAST
    assert "SpiderF9Toast_DrawAfterPresent" not in TOAST


def test_toast_timer_starts_on_first_actual_display_in_sparse_comic_scenes():
    assert "#define SPIDER_F9_TOAST_MS 2500u" in TOAST
    assert "#define SPIDER_F9_TOAST_GRACE_MS 10000u" in TOAST
    assert "toast_published_at=GetTickCount();" in TOAST
    assert "toast_expires_at=0; /* No countdown before first actual presentation. */" in TOAST
    assert "if(!toast_expires_at)toast_expires_at=GetTickCount()+SPIDER_F9_TOAST_MS;" in TOAST
    assert "if(toast_generation==generation)" in TOAST
    assert "toast_displayed_generation=generation;" in TOAST
    assert "SpiderF9Toast_GetDisplayStatus(&published,&displayed);" in CORE
    assert "F9-TOAST: generation=%lu first subsequent Flip=" in CORE
    assert "F9-TOAST: generation=%lu FIRST displayed at Flip=" in CORE


def test_comic_late_blt_can_only_reapply_cached_panel_without_gdi_on_primary():
    assert "SpiderF9Toast_GetComposedPanel(" in HEADER
    assert "BOOL SpiderF9Toast_GetComposedPanel(" in TOAST
    assert "toast_panel_region=panel;" in TOAST
    assert "toast_panel_valid=TRUE;" in TOAST
    assert "toast_panel_valid=FALSE;" in TOAST
    assert "published==shown" in TOAST
    assert "static volatile LONG f9_comic_last_draw_tick=0;" in CORE
    assert "InterlockedExchange(&f9_comic_last_draw_tick,(LONG)GetTickCount());" in CORE
    blt=CORE.split("static HRESULT WINAPI hooked_blt(",1)[1].split(
        "/* The surface-creation hook remains responsible",1)[0]
    assert "self==(void*)borderless_primary" in blt
    assert "SpiderF9Toast_GetComposedPanel(&panel_stage,&panel)" in blt
    assert "IntersectRect(&overlap,presented_dest,&dest_panel)" in blt
    assert "original_blt(self,&dest_panel,panel_stage,&panel," in blt
    assert "F9-TOAST-COMIC: PRIMARY Blt" in blt
    assert "GetDC(" not in blt
