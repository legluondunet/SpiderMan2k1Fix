"""F9 PNG notification regression checks.

The DirectDraw Flip compositor handles normal moving gameplay. A separate,
input-transparent window is allowed only if a static comic page has no Flip.
Neither display route may modify the PNG's original backbuffer.
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
    assert "WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST" in TOAST
    assert "WS_EX_LAYERED|WS_EX_TRANSPARENT" in TOAST
    assert "toast_popup_thread(" in TOAST
    assert "toast_displayed_generation!=gen" in TOAST
    assert "GetForegroundWindow()!=game" in TOAST
    assert "UpdateLayeredWindow(" in TOAST
    assert "DestroyWindow(popup)" in TOAST
    assert "GetAsyncKeyState" not in TOAST
    assert "SWP_NOACTIVATE|SWP_SHOWWINDOW" in TOAST
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
    assert "#define SPIDER_F9_TOAST_MS 2000u" in TOAST
    assert "#define SPIDER_F9_TOAST_GRACE_MS 10000u" in TOAST
    assert "toast_published_at=GetTickCount();" in TOAST
    assert "toast_expires_at=0; /* Display starts the countdown. */" in TOAST
    assert "if(!toast_expires_at)toast_expires_at=GetTickCount()+SPIDER_F9_TOAST_MS;" in TOAST
    assert "if(toast_generation==generation)" in TOAST
    assert "toast_displayed_generation=generation;" in TOAST
    assert "SpiderF9Toast_GetDisplayStatus(&published,&displayed);" in CORE
    assert "F9-TOAST: generation=%lu first subsequent Flip=" in CORE
    assert "F9-TOAST: generation=%lu FIRST displayed at Flip=" in CORE


def test_static_comic_fallback_does_not_modify_game_video_or_input_hooks():
    assert "SpiderF9Toast_SetGameWindow(hwnd,log_line);" in CORE
    assert "SpiderF9Toast_SetGameWindow(HWND hwnd,void (*logger)(const char*));" in HEADER
    assert "f9_comic_last_draw_tick" not in CORE
    assert "SpiderF9Toast_GetComposedPanel" not in CORE
    blt = CORE.split("static HRESULT WINAPI hooked_blt(", 1)[1].split(
        "/* The surface-creation hook remains responsible", 1)[0]
    assert "F9-TOAST-COMIC: PRIMARY Blt" not in blt
    assert "original_blt(self,&dest_panel,panel_stage" not in blt


def test_no_flip_comic_toast_is_shown_without_directdraw_or_input_hooks():
    assert "Sleep(delay_ms);" in TOAST
    assert "const DWORD delay_ms=160u;" in TOAST
    assert "toast_displayed_generation!=gen" in TOAST
    assert "if(!ready || !game || !IsWindow(game) ||" in TOAST
    assert "toast_popup_wndproc" in TOAST
    assert "if(msg==WM_NCHITTEST)return HTTRANSPARENT;" in TOAST
    assert "if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;" in TOAST
    assert "WS_EX_LAYERED|WS_EX_TRANSPARENT" in TOAST
    assert "UpdateLayeredWindow(" in TOAST
    assert "SPIDER_F9_TOAST_MS 2000u" in TOAST
    assert "toast_popup_generation" in TOAST
    assert "if((DWORD)InterlockedCompareExchange(&toast_popup_generation,0,0)==generation)" in TOAST
    assert "F9-TOAST-STATIC: generation=" in TOAST
    popup = TOAST.split("static DWORD WINAPI toast_popup_thread(LPVOID arg) {",1)[1].split(
        "IDirectDrawSurface7 *SpiderF9Toast_CompositeFrame(",1)[0]
    assert "IDirectDrawSurface7" not in popup
    assert "IDirectDrawSurface7_Lock" not in popup
    assert "GetDC(NULL)" in popup
    assert "CreateDIBSection(" in popup
    assert "toast_draw_dc(mem," in popup


def test_comic_screenshot_toast_is_popup_exclusive_and_self_expiring():
    assert "SpiderF9PNG_QueueComic(&desc,log_line)" in CORE
    assert "f9_comic_last_draw_flip" in CORE
    assert "flip-last<=2" in CORE
    assert "toast_current_is_comic=comic;" in TOAST
    assert "if(comic)toast_start_no_flip_fallback(generation);" in TOAST
    assert "void SpiderF9Toast_PublishSavedPathComic(" in TOAST
    start = TOAST.index("IDirectDrawSurface7 *SpiderF9Toast_CompositeFrame(")
    part = TOAST[start:]
    assert "if(comic) {" in part
    assert "toast_discard_stage();" in part
    assert part.index("if(comic) {") < part.index("toast_create_stage(")
    assert "ShowWindow(popup,SW_HIDE);" in TOAST
    assert "DestroyWindow(popup);" in TOAST
    assert "SWP_HIDEWINDOW" in TOAST
    assert "F9-TOAST-STATIC: generation=%lu hidden and destroyed after 2s" in TOAST
    assert "#define SPIDER_F9_TOAST_MS 2000u" in TOAST
    assert "SpiderF9Toast_PublishSavedPath(dest);" in ENC
    assert "SpiderF9Toast_PublishSavedPathComic(dest);" in ENC


def test_popup_is_not_owned_by_non_pumping_game_window():
    assert 'NULL,NULL,instance,NULL); /* never own game HWND across threads */' in TOAST
    assert "cross-thread" in TOAST
    assert "ShowWindow(popup,SW_HIDE);" in TOAST
    assert "DestroyWindow(popup);" in TOAST
