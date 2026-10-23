"""Regression checks for the two separately opted-in F9 PNG modes.

Dual-format comparison retains the original BMP. PNG-only redirects both
SHA-verified native F9 calls and does not invoke the BMP writer. Both use
copied pixels and an independent Win32 GDI+ encoder worker.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
ENC = (ROOT/"f9_png.c").read_text(encoding="utf-8")
HEADER = (ROOT/"f9_png.h").read_text(encoding="utf-8")
BUILD = (ROOT/"build.sh").read_text(encoding="utf-8")


def test_png_only_runs_at_sha_verified_f9_readback_with_explicit_opt_in():
    assert "static volatile LONG f9_native_png_enabled=0;" in CORE
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_F9_PNG"' in CORE
    assert "if(debug_log_enabled &&" in CORE
    assert "InterlockedExchange(&f9_backbuffer_fix_enabled,1);" in CORE
    assert "rva==0x001034D4u" in CORE
    assert "self==(void *)borderless_primary && borderless_back" in CORE
    assert "(flags&DDLOCK_READONLY)!=0" in CORE
    assert "if(InterlockedCompareExchange(&f9_native_png_enabled,0,0)==1)" in CORE
    assert "SpiderF9PNG_Queue(desc,log_line);" in CORE
    assert "if (!supported_exe()) {" in CORE


def test_encoder_copies_pixels_and_unblocks_directdraw_before_compression():
    assert "SpiderF9PNG_Queue(" in HEADER
    assert "memcpy(job->pixels+(SIZE_T)y*row_bytes," in ENC
    assert "base+(SIZE_T)y*(SIZE_T)frame->lPitch,row_bytes" in ENC
    assert "CreateThread(NULL,0,spider_png_thread,job,0,NULL)" in ENC
    worker = ENC.split("static DWORD WINAPI spider_png_thread(",1)[1].split(
        "int SpiderF9PNG_Queue(",1
    )[0]
    assert "IDirectDrawSurface7" not in worker
    assert "Lock(" not in worker
    assert "Unlock(" not in worker
    assert "GdipSaveImageToFile" in worker
    assert "MoveFileExW(temp,dest,MOVEFILE_WRITE_THROUGH)" in worker


def test_png_is_lossless_and_uses_os_encoder_without_new_static_dll_import():
    assert 'LoadLibraryW(L"gdiplus.dll")' in ENC
    assert 'GetProcAddress(lib,"GdiplusStartup")' in ENC
    assert 'GetProcAddress(lib,"GdipCreateBitmapFromScan0")' in ENC
    assert 'GetProcAddress(lib,"GdipSaveImageToFile")' in ENC
    assert "SPIDER_PNG_PIXEL_FORMAT_32BPP_RGB" in ENC
    assert "0x00ff0000u" in ENC
    assert "0x0000ff00u" in ENC
    assert "0x000000ffu" in ENC
    assert "if(frame->lPitch<(LONG)row_bytes) {" in ENC
    assert 'SpiderF9Toast_PublishError(L"Invalid screenshot image layout")' in ENC
    assert "GetModuleFileNameW(NULL,exe,MAX_PATH)" in ENC
    assert "f9_png.c" in BUILD
    assert "-lgdiplus" not in BUILD


def test_filename_and_failure_safety():
    assert "scrn%04u.png" in ENC
    assert "SpiderF9PNG-%lu-%ld.tmp" in ENC
    assert "for(n=0;n<10000u;n++)" in ENC
    assert "DeleteFileW(temp);" in ENC
    assert "InterlockedCompareExchange(&spider_png_busy,1,0)" in ENC
    assert "InterlockedExchange(&spider_png_busy,0)" in ENC
    assert "if(!worker)" in ENC
    assert "if(!saved)" in ENC


def test_dual_format_comparison_still_retains_original_bmp():
    assert "original BMP retained during test" in CORE
    assert "original_surface_lock(back,rect,desc,flags,event)" in CORE
    assert "original_surface_unlock(back,rect)" in CORE
    assert "return back_hr;" in CORE
    assert "Legacy BMP remains enabled" in CORE
    assert "spider_png_thread" in ENC

def test_png_only_native_f9_dispatch_is_strictly_opt_in_and_signature_verified():
    assert 'SPIDERMAN2K1FIX_F9_PNG_ONLY' in CORE
    assert 'static volatile LONG f9_png_only_enabled=0;' in CORE
    assert 'static void __cdecl hooked_f9_png_only(void)' in CORE
    assert 'static void install_f9_png_only_dispatch(void)' in CORE
    assert '0x0010A1DCu,{0xE8,0xFF,0x91,0xFF,0xFF}' in CORE
    assert '0x0010A62Bu,{0xE8,0xB0,0x8D,0xFF,0xFF}' in CORE
    assert '0x001033E0' in CORE or '0x1033E0' in CORE
    assert '(uintptr_t)base!=0x00400000u' in CORE
    assert 'memcmp(p[i],calls[i].original,sizeof(calls[i].original))!=0' in CORE
    assert 'VirtualProtect(p[0],5,PAGE_EXECUTE_READWRITE,&previous[0])' in CORE
    assert 'VirtualProtect(p[1],5,PAGE_EXECUTE_READWRITE,&previous[1])' in CORE
    assert 'p[i][0]=0xE8;' in CORE
    assert 'FlushInstructionCache(GetCurrentProcess(),p[0],5)' in CORE
    assert 'FlushInstructionCache(GetCurrentProcess(),p[1],5)' in CORE
    assert "if(InterlockedCompareExchange(&f9_png_only_enabled,0,0)==1)" in CORE
    assert 'install_f9_png_only_dispatch();' in CORE
    assert 'if(debug_log_enabled) {' in CORE
    assert 'supported_exe()' in CORE


def test_png_only_uses_existing_game_f9_dispatch_without_hooking_keyboard():
    dispatch = CORE.split("static void __cdecl hooked_f9_png_only(void)",1)[1].split(
        "static void install_f9_png_only_dispatch(void)",1
    )[0]
    assert 'IDirectDrawSurface7_Lock(back,NULL,&desc,' in dispatch
    assert 'IDirectDrawSurface7_Unlock(back,NULL)' in dispatch
    assert 'DDLOCK_READONLY|DDLOCK_WAIT' in dispatch
    assert 'SpiderF9PNG_Queue(&desc,log_line)' in dispatch
    assert 'IDirectDrawSurface7_AddRef(back)' in dispatch
    assert 'IDirectDrawSurface7_Release(back)' in dispatch
    assert 'f9_png_capture_active' in dispatch
    assert 'CreateFile' not in dispatch
    assert 'GetAsyncKeyState' not in dispatch
    assert 'original_surface_lock(' not in dispatch
    assert 'original_surface_unlock(' not in dispatch
    assert 'F9-PNG-ONLY: backbuffer capture=' in dispatch


def test_png_only_bypasses_original_bmp_without_touching_unrelated_game_code():
    patch = CORE.split('static void install_f9_png_only_dispatch(void)',1)[1].split(
        'static void instrument_surface(IDirectDrawSurface7 *surface);',1
    )[0]
    assert '0x0010A1DCu' in patch and '0x0010A62Bu' in patch
    assert '0x001033E0' in CORE or '0x1033E0' in CORE
    assert 'only those 5-byte CALL operands' in CORE
    assert 'legacy BMP writer bypassed' in CORE
    assert 'IDirectDrawSurface7_Lock(back' in CORE
    assert 'VirtualProtect(p[0],5,previous[0],&ignored)' in CORE
    assert 'VirtualProtect(p[1],5,previous[1],&ignored)' in CORE
    assert 'return;' in patch
    # The correct, already validated old BMP path is still available when
    # SPIDERMAN2K1FIX_F9_PNG_ONLY is not explicitly selected.
    assert 'rva==0x001034D4u' in CORE
    assert 'rva==0x00103537u' in CORE


def test_main_menu_f9_is_deferred_until_complete_flip_and_never_reads_primary():
    # On native Windows PRIMARY-DC returned magenta; BACK is cleared at the
    # end of every menu Flip, so capturing immediately on the F9 key is black.
    assert "static volatile LONG f9_menu_png_pending=0;" in CORE
    assert "static void f9_capture_completed_menu_flip(LONG flip,BOOL rendered_menu)" in CORE
    assert 'F9-PNG-MENU: deferred until next completed menu Flip' in CORE
    assert "f9_capture_menu_presented_frame()" not in CORE
    assert "f9_capture_presented_dc(" not in CORE
    assert "IDirectDrawSurface7_GetDC(borderless_primary,&dc)" not in CORE
    assert 'source=PRIMARY-DC' not in CORE
    dispatch = CORE.split("static void __cdecl hooked_f9_png_only(void)",1)[1].split(
        "static void install_f9_png_only_dispatch(void)",1)[0]
    assert "int movie=f9_capture_decoded_movie_frame();" in dispatch
    assert "if(movie==0)" in dispatch
    assert "InterlockedCompareExchange(&f9_menu_png_pending,1,0)==0" in dispatch
    assert "InterlockedExchange(&f9_menu_png_requested_flip," in dispatch
    assert "back=borderless_back;" in dispatch
    assert dispatch.index("if(movie==0)") < dispatch.index("back=borderless_back;")


def test_menu_screenshot_captures_only_rendered_menu_before_present_and_clear():
    start = CORE.index("static HRESULT WINAPI hooked_flip(")
    flip = CORE[start:CORE.index("static void instrument_surface(",start)]
    pending = CORE.index("static void f9_capture_completed_menu_flip(")
    capture = CORE[pending:CORE.index("/* Source-frame capture for intro FMVs only.",pending)]
    assert "menu_draws>=3 && movie_blits==0" in flip
    assert "BOOL f9_still_ready=menu_active && menu_draws>=1 && movie_blits==0" in flip
    assert "f9_capture_completed_menu_flip(count,f9_still_ready);" in flip
    assert "count-movie_last>3" in flip
    assert "f9_capture_completed_menu_flip(count,f9_still_ready);" in flip
    assert flip.index("f9_capture_completed_menu_flip(count,f9_still_ready);") < flip.index(
        "hr=original_blt(self,&rc,present_source,NULL,DDBLT_WAIT,NULL);")
    assert flip.index("f9_capture_completed_menu_flip(count,f9_still_ready);") < flip.index(
        "cleared=original_blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fill);")
    assert "if(!rendered_menu)return;" in capture
    assert "flip-requested>120" in capture
    assert "IDirectDrawSurface7_Lock(back,NULL,&desc,DDLOCK_READONLY|DDLOCK_WAIT,NULL)" in capture
    assert "SpiderF9PNG_Queue(&desc,log_line);" in capture
    assert "IDirectDrawSurface7_Unlock(back,NULL);" in capture
    assert "InterlockedExchange(&f9_menu_png_pending,0);" in capture
    assert "F9-PNG-MENU: completed Flip=" in capture
    assert "GetDC(" not in capture
    assert "GetAsyncKeyState" not in capture


def test_intro_movie_uses_decoded_blt_surface_not_broken_primary_gdi():
    assert "static IDirectDrawSurface7 *f9_movie_source=NULL;" in CORE
    assert "if(SUCCEEDED(hr) && f9_movie_present && source)" in CORE
    assert "IDirectDrawSurface7_AddRef(source);" in CORE
    assert "IDirectDrawSurface7_Release(f9_movie_source);" in CORE
    assert "f9_movie_drop_source();" in CORE
    assert "static int f9_capture_decoded_movie_frame(void)" in CORE
    assert "IDirectDrawSurface7_Lock(source,NULL,&sd," in CORE
    assert "IDirectDrawSurface7_Unlock(source,NULL)" in CORE
    assert "F9-PNG-MOVIE: decoded source=" in CORE
    assert "F9-PNG-MOVIE: decoded RGB screenshot" in CORE
    dispatch = CORE.split("static void __cdecl hooked_f9_png_only(void)",1)[1].split(
        "static void install_f9_png_only_dispatch(void)",1)[0]
    assert "int movie=f9_capture_decoded_movie_frame();" in dispatch
    assert "if(movie==0)" in dispatch
    assert "InterlockedCompareExchange(&f9_menu_png_pending,1,0)==0" in dispatch
    assert "back=borderless_back;" in dispatch


def test_intro_decoded_rgb_conversion_is_mask_aware_and_preserves_gameplay():
    start = CORE.index("static int f9_capture_decoded_movie_frame(void)")
    end = CORE.index("static void __cdecl hooked_f9_png_only(void)",start)
    movie = CORE[start:end]
    assert "bpp!=16 && bpp!=32" in movie
    assert "DDPF_RGB" in movie
    assert "f9_movie_prepare_mask(rm,&rs,&rmax)" in movie
    assert "f9_movie_prepare_mask(gm,&gs,&gmax)" in movie
    assert "f9_movie_prepare_mask(bm,&bs,&bmax)" in movie
    assert "f9_movie_expand(pixel,rm,rs,rmax)" in movie
    assert "f9_movie_expand(pixel,gm,gs,gmax)" in movie
    assert "f9_movie_expand(pixel,bm,bs,bmax)" in movie
    assert "menu_map.left" in movie and "menu_map.bottom" in movie
    assert "calloc((size_t)dw*(size_t)dh,4u)" in movie
    assert "SpiderF9PNG_Queue(&frame,log_line)" in movie
    assert "free(pixels)" in movie
    assert "f9_movie_last_flip" in movie
    assert "if(!source || (current_flip-movie_flip)>8u)return 0;" in movie
    assert 'L"Unsupported decoded movie pixel format"' in movie


def test_static_comic_context_follows_each_png_job_without_changing_encoder():
    assert "SpiderF9PNG_QueueComic(" in HEADER
    assert "BOOL static_comic;" in ENC
    assert "job->static_comic=static_comic;" in ENC
    assert "if(job->static_comic)SpiderF9Toast_PublishSavedPathComic(dest);" in ENC
    assert "else SpiderF9Toast_PublishSavedPath(dest);" in ENC
    assert "return spider_png_queue_internal(frame,log_fn,FALSE);" in ENC
    assert "return spider_png_queue_internal(frame,log_fn,TRUE);" in ENC
    assert "if(MoveFileExW(temp,dest,MOVEFILE_WRITE_THROUGH))" in ENC
    assert "spider_png_verify(dest)" in ENC

def test_readme_reports_wine_png_only_validation_without_claiming_windows():
    readme=(ROOT/"README.md").read_text(encoding="utf-8")
    section=readme.split("### Optional native F9-to-PNG screenshots",1)[1]
    section=section.split("### Native Windows 11: experimental F9 screenshot repair",1)[0]
    assert "build #717" in section
    assert "scrn0010.png" in section
    assert "disabled by default" in section
    assert "Windows 11" in section
    assert "not" in section.lower()
