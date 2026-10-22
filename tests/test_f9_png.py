"""Constrain the opt-in F9 PNG prototype without claiming BMP suppression.

The confirmed Windows fix used the game's own F9 Lock at RVA 0x1034D4.
The first PNG stage clones the backbuffer pixels to private memory, then
writes a lossless PNG on a worker using only Win32's built-in GDI+ DLL.
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


def test_first_stage_must_not_pretend_legacy_bmp_is_suppressed():
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


def test_f9_menu_and_intro_capture_visible_presentation_not_3d_backbuffer():
    assert "static int f9_capture_menu_presented_frame(void)" in CORE
    assert "IDirectDrawSurface7_GetDC(borderless_primary,&dc)" in CORE
    assert "IDirectDrawSurface7_ReleaseDC(borderless_primary,dc)" in CORE
    assert "GetDC(game_hwnd)" in CORE
    assert "ReleaseDC(game_hwnd,dc)" in CORE
    assert 'f9_capture_presented_dc(dc,0,0,"WINDOW-DC")' in CORE
    assert 'f9_capture_presented_dc(dc,borderless_monitor_x,' in CORE
    assert "SpiderF9PNG_Queue(&frame,log_line)" in CORE
    dispatch = CORE.split("static void __cdecl hooked_f9_png_only(void)",1)[1].split(
        "static void install_f9_png_only_dispatch(void)",1
    )[0]
    assert "if(InterlockedCompareExchange(&menu_upscale_active,0,0)==1)" in dispatch
    assert "f9_capture_menu_presented_frame();" in dispatch
    assert dispatch.index("f9_capture_menu_presented_frame();") < dispatch.index("back=borderless_back;")
    assert "IDirectDrawSurface7_Lock(back,NULL,&desc," in dispatch


def test_f9_menu_readback_uses_safe_top_down_32bpp_dib():
    begin = CORE.index("static int f9_capture_presented_dc(")
    end = CORE.index("static void __cdecl hooked_f9_png_only(",begin)
    part = CORE[begin:end]
    assert "bitmap.bmiHeader.biHeight=-(LONG)desktop_h;" in part
    assert "bitmap.bmiHeader.biBitCount=32;" in part
    assert "bitmap.bmiHeader.biCompression=BI_RGB;" in part
    assert "CreateDIBSection(source_dc,&bitmap,DIB_RGB_COLORS,&pixels,NULL,0)" in part
    assert "BitBlt(memory_dc,0,0,(int)desktop_w,(int)desktop_h," in part
    assert "frame.lPitch=(LONG)desktop_w*4;" in part
    assert "frame.ddpfPixelFormat.dwRBitMask=0x00ff0000u;" in part
    assert "frame.ddpfPixelFormat.dwGBitMask=0x0000ff00u;" in part
    assert "frame.ddpfPixelFormat.dwBBitMask=0x000000ffu;" in part
    assert "SpiderF9PNG_Queue(&frame,log_line)" in part
    assert "SelectObject(memory_dc,previous);" in part
    assert "DeleteObject(image);" in part
    assert "DeleteDC(memory_dc);" in part
    assert "IDirectDrawSurface7_Lock(borderless_primary" not in part
    assert "SpiderF9Toast_PublishError" in part
