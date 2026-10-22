"""Native F9 success toast regression checks.

The asynchronous worker is the only producer, and the DirectDraw Flip game
thread is the only renderer. The overlay is drawn AFTER presenting the scene
to PRIMARY; PNG encoding reads only BACKBUFFER. It never writes into the
screenshot, and failed/queued captures must never trigger a success toast.
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
    assert "SPIDER_F9_TOAST_MS 3500u" in TOAST


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


def test_overlay_only_renders_after_present_on_primary_not_into_png():
    start = CORE.index("static HRESULT WINAPI hooked_flip(")
    flip = CORE[start:CORE.index("static void instrument_surface(",start)]
    assert "hr=original_blt(self,&rc,borderless_back,NULL,DDBLT_WAIT,NULL);" in flip
    assert "SpiderF9Toast_DrawAfterPresent((IDirectDrawSurface7 *)self," in flip
    assert flip.index("SpiderF9Toast_DrawAfterPresent") > flip.index(
        "hr=original_blt(self,&rc,borderless_back,NULL,DDBLT_WAIT,NULL);"
    )
    assert "InterlockedCompareExchange(&f9_native_png_enabled,0,0)==1" in flip
    assert "IDirectDrawSurface7_GetDC(primary,&dc)" in TOAST
    assert "GetDC(window)" in TOAST
    assert "IDirectDrawSurface7_GetDC(borderless_back" not in TOAST
    assert "SpiderF9Toast_DrawAfterPresent(" in HEADER


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
