/* SpiderMan2k1Fix native lossless screenshot prototype.
 * No external image package or imported gdiplus dependency at startup:
 * Win32 GDI+ flat functions are resolved dynamically on the encoder thread.
 * The DirectDraw surface is NEVER accessed by this worker.
 *
 * Experimental: this first pass deliberately preserves the old BMP output,
 * to verify correct color/orientation and Wine interoperability BEFORE the
 * native game's BMP routine is disabled by a separately signature-verified
 * game patch. Do not claim the old F9 BMP has been suppressed yet.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "f9_png.h"

#define SPIDER_PNG_PIXEL_FORMAT_32BPP_RGB 0x00022009
#define SPIDER_PNG_MAX_DIMENSION 8192u

/* GDI+ documented flat ABI. Use own small private declarations to avoid
 * depending on whether a particular MinGW package ships C++ GDI+ headers. */
typedef struct SpiderGdipStartupInput {
  UINT32 GdiplusVersion;
  void *DebugEventCallback;
  BOOL SuppressBackgroundThread;
  BOOL SuppressExternalCodecs;
} SpiderGdipStartupInput;
typedef int  (WINAPI *SpiderGdipStartupFn)(ULONG_PTR *,const SpiderGdipStartupInput *,void *);
typedef void (WINAPI *SpiderGdipShutdownFn)(ULONG_PTR);
typedef int  (WINAPI *SpiderGdipBitmapFn)(INT,INT,INT,INT,BYTE *,void **);
typedef int  (WINAPI *SpiderGdipSaveFn)(void *,const WCHAR *,const CLSID *,const void *);
typedef int  (WINAPI *SpiderGdipDisposeFn)(void *);

typedef struct SpiderPNGJob {
  BYTE *pixels;
  DWORD width,height;
  LONG stride;
  void (*log_fn)(const char *);
} SpiderPNGJob;

static volatile LONG spider_png_busy=0;
static volatile LONG spider_png_serial=0;

static void spider_png_log(SpiderPNGJob *job,const char *line) {
  if(job && job->log_fn)job->log_fn(line);
}

static DWORD WINAPI spider_png_thread(void *ctx) {
  static const CLSID png_encoder={
    0x557cf406,0x1a04,0x11d3,
    {0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}
  };
  SpiderPNGJob *job=(SpiderPNGJob *)ctx;
  SpiderGdipStartupFn start=NULL;
  SpiderGdipShutdownFn shutdown=NULL;
  SpiderGdipBitmapFn bitmap_create=NULL;
  SpiderGdipSaveFn image_save=NULL;
  SpiderGdipDisposeFn image_dispose=NULL;
  SpiderGdipStartupInput input;
  HMODULE lib=NULL;
  ULONG_PTR token=0;
  void *bitmap=NULL;
  WCHAR exe[MAX_PATH],temp[MAX_PATH],dest[MAX_PATH];
  WCHAR *slash;
  DWORD len,error=0;
  LONG serial=InterlockedIncrement(&spider_png_serial);
  UINT n;
  int saved=0;
  char msg[320];

  if(!job)goto done;
  lib=LoadLibraryW(L"gdiplus.dll");
  if(!lib) {
    spider_png_log(job,"F9-PNG: gdiplus.dll unavailable; screenshot not saved");
    goto done;
  }
  start=(SpiderGdipStartupFn)GetProcAddress(lib,"GdiplusStartup");
  shutdown=(SpiderGdipShutdownFn)GetProcAddress(lib,"GdiplusShutdown");
  bitmap_create=(SpiderGdipBitmapFn)GetProcAddress(lib,"GdipCreateBitmapFromScan0");
  image_save=(SpiderGdipSaveFn)GetProcAddress(lib,"GdipSaveImageToFile");
  image_dispose=(SpiderGdipDisposeFn)GetProcAddress(lib,"GdipDisposeImage");
  if(!start || !shutdown || !bitmap_create || !image_save || !image_dispose) {
    spider_png_log(job,"F9-PNG: GDI+ encoder exports unavailable");
    goto done;
  }
  memset(&input,0,sizeof(input));
  input.GdiplusVersion=1;
  if(start(&token,&input,NULL)!=0 || token==0) {
    spider_png_log(job,"F9-PNG: GdiplusStartup failed");
    goto done;
  }
  if(bitmap_create((INT)job->width,(INT)job->height,(INT)job->stride,
                   SPIDER_PNG_PIXEL_FORMAT_32BPP_RGB,job->pixels,&bitmap)!=0 ||
     !bitmap) {
    spider_png_log(job,"F9-PNG: bitmap conversion failed");
    goto done;
  }
  len=GetModuleFileNameW(NULL,exe,MAX_PATH);
  if(!len || len>=MAX_PATH) {
    spider_png_log(job,"F9-PNG: cannot resolve game directory");
    goto done;
  }
  slash=wcsrchr(exe,L'\\');
  if(!slash) {
    spider_png_log(job,"F9-PNG: cannot locate game directory separator");
    goto done;
  }
  *slash=L'\0';
  if(swprintf(temp,MAX_PATH,L"%ls\\SpiderF9PNG-%lu-%ld.tmp",
              exe,(unsigned long)GetCurrentProcessId(),(long)serial)>=MAX_PATH) {
    spider_png_log(job,"F9-PNG: temporary filename too long");
    goto done;
  }
  if(image_save(bitmap,temp,&png_encoder,NULL)!=0) {
    spider_png_log(job,"F9-PNG: PNG encoder rejected image");
    DeleteFileW(temp);
    goto done;
  }

  /* Non-overwriting atomic publication. The GDI+ intermediate has PNG bytes,
   * not a legacy BMP; no BMP is ever created by our encoder. */
  for(n=0;n<10000u;n++) {
    if(swprintf(dest,MAX_PATH,L"%ls\\scrn%04u.png",exe,n)>=MAX_PATH)break;
    if(MoveFileExW(temp,dest,MOVEFILE_WRITE_THROUGH)) {
      snprintf(msg,sizeof(msg),"F9-PNG: saved scrn%04u.png %lux%lu (native lossless)",
               (unsigned)n,(unsigned long)job->width,(unsigned long)job->height);
      spider_png_log(job,msg);
      saved=1;
      break;
    }
    error=GetLastError();
    if(error!=ERROR_ALREADY_EXISTS && error!=ERROR_FILE_EXISTS)break;
  }
  if(!saved) {
    snprintf(msg,sizeof(msg),"F9-PNG: unable to publish PNG Win32=%lu",
             (unsigned long)error);
    spider_png_log(job,msg);
    DeleteFileW(temp);
  }

done:
  if(bitmap && image_dispose)image_dispose(bitmap);
  if(token && shutdown)shutdown(token);
  if(lib)FreeLibrary(lib);
  if(job) {
    free(job->pixels);
    free(job);
  }
  InterlockedExchange(&spider_png_busy,0);
  return 0;
}

int SpiderF9PNG_Queue(const DDSURFACEDESC2 *frame,void (*log_fn)(const char *)) {
  SpiderPNGJob *job;
  DWORD width,height,y;
  SIZE_T row_bytes,total;
  const BYTE *base;
  HANDLE worker;

  if(!frame || !frame->lpSurface ||
     frame->dwWidth==0 || frame->dwHeight==0 ||
     frame->dwWidth>SPIDER_PNG_MAX_DIMENSION ||
     frame->dwHeight>SPIDER_PNG_MAX_DIMENSION ||
     frame->ddpfPixelFormat.dwRGBBitCount!=32 ||
     frame->ddpfPixelFormat.dwRBitMask!=0x00ff0000u ||
     frame->ddpfPixelFormat.dwGBitMask!=0x0000ff00u ||
     frame->ddpfPixelFormat.dwBBitMask!=0x000000ffu)
    return 0;
  width=frame->dwWidth;
  height=frame->dwHeight;
  row_bytes=(SIZE_T)width*4u;
  if(frame->lPitch<(LONG)row_bytes) return 0;
  if(InterlockedCompareExchange(&spider_png_busy,1,0)!=0) {
    if(log_fn)log_fn("F9-PNG: capture skipped; encoder still busy");
    return 0;
  }
  job=(SpiderPNGJob*)calloc(1,sizeof(*job));
  if(!job)goto fail;
  total=row_bytes*(SIZE_T)height;
  job->pixels=(BYTE*)malloc(total);
  if(!job->pixels) {
    free(job);
    goto fail;
  }
  job->width=width;
  job->height=height;
  job->stride=(LONG)row_bytes;
  job->log_fn=log_fn;
  base=(const BYTE*)frame->lpSurface;
  /* Copy while locked. The worker can never read the DirectDraw surface. */
  for(y=0;y<height;y++)
    memcpy(job->pixels+(SIZE_T)y*row_bytes,
           base+(SIZE_T)y*(SIZE_T)frame->lPitch,row_bytes);
  worker=CreateThread(NULL,0,spider_png_thread,job,0,NULL);
  if(!worker) {
    free(job->pixels);
    free(job);
    goto fail;
  }
  CloseHandle(worker);
  if(log_fn)log_fn("F9-PNG: frame copied to independent memory; async encoder started");
  return 1;
fail:
  InterlockedExchange(&spider_png_busy,0);
  if(log_fn)log_fn("F9-PNG: buffer allocation/thread creation failed");
  return 0;
}
