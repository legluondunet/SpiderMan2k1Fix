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
    assert "if(frame->lPitch<(LONG)row_bytes) return 0;" in ENC
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
