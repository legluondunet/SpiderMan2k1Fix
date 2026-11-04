/* SpiderMan2k1Fix F9 lossless PNG encoder.
 * No external image package or imported gdiplus dependency at startup:
 * Win32 GDI+ flat functions are resolved dynamically on the encoder thread.
 * The DirectDraw surface is NEVER accessed by this worker.
 *
 * The permanent F9 capture path uses this encoder after SHA-verified
 * native dispatch interceptions, suppressing the old BMP writer where
 * the guarded PNG hooks are active.
 * The caller handles native hooks, the original F9 debounce and Lock/Unlock.
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
#include "spider_fix_paths.h"
#include "f9_toast.h"

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
  BOOL static_comic;
} SpiderPNGJob;

static volatile LONG spider_png_busy=0;
static volatile LONG spider_png_serial=0;

static void spider_png_log(SpiderPNGJob *job,const char *line) {
  if(job && job->log_fn)job->log_fn(line);
}

/* Check final on-disk PNG rather than trusting only GDI+ and rename status. */
static int spider_png_verify(const WCHAR *path) {
  static const BYTE magic[8]={137,80,78,71,13,10,26,10};
  BYTE prefix[8];
  DWORD got=0;
  LARGE_INTEGER length;
  HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,NULL);
  int good=0;
  if(f==INVALID_HANDLE_VALUE)return 0;
  if(GetFileSizeEx(f,&length) && length.QuadPart>24 &&
     ReadFile(f,prefix,sizeof(prefix),&got,NULL) && got==sizeof(prefix) &&
     memcmp(prefix,magic,sizeof(magic))==0)good=1;
  CloseHandle(f);
  return good;
}

static void spider_png_fail(SpiderPNGJob *job,const char *log,
                            const WCHAR *reason) {
  spider_png_log(job,log);
  SpiderF9Toast_PublishError(reason);
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
  DWORD error=0;
  LONG serial=InterlockedIncrement(&spider_png_serial);
  UINT n;
  int saved=0;
  char msg[320];

  if(!job)goto done;
  lib=LoadLibraryW(L"gdiplus.dll");
  if(!lib) {
    spider_png_fail(job,"F9-PNG: gdiplus.dll unavailable; screenshot not saved",L"GDI+ encoder unavailable");
    goto done;
  }
  start=(SpiderGdipStartupFn)GetProcAddress(lib,"GdiplusStartup");
  /* GetProcAddress uses FARPROC returning INT; the documented
   * GdiplusShutdown ABI returns void. Copy the function pointer's bits
   * without an incompatible function-pointer cast (-Wcast-function-type). */
  {
    FARPROC proc=GetProcAddress(lib,"GdiplusShutdown");
    memcpy(&shutdown,&proc,sizeof(shutdown));
  }
  bitmap_create=(SpiderGdipBitmapFn)GetProcAddress(lib,"GdipCreateBitmapFromScan0");
  image_save=(SpiderGdipSaveFn)GetProcAddress(lib,"GdipSaveImageToFile");
  image_dispose=(SpiderGdipDisposeFn)GetProcAddress(lib,"GdipDisposeImage");
  if(!start || !shutdown || !bitmap_create || !image_save || !image_dispose) {
    spider_png_fail(job,"F9-PNG: GDI+ encoder exports unavailable",L"PNG encoder unavailable");
    goto done;
  }
  memset(&input,0,sizeof(input));
  input.GdiplusVersion=1;
  if(start(&token,&input,NULL)!=0 || token==0) {
    spider_png_fail(job,"F9-PNG: GdiplusStartup failed",L"Cannot initialize PNG encoder");
    goto done;
  }
  if(bitmap_create((INT)job->width,(INT)job->height,(INT)job->stride,
                   SPIDER_PNG_PIXEL_FORMAT_32BPP_RGB,job->pixels,&bitmap)!=0 ||
     !bitmap) {
    spider_png_fail(job,"F9-PNG: bitmap conversion failed",L"Cannot convert screenshot pixels");
    goto done;
  }
  if(!spider_path_screenshots_w(exe,MAX_PATH)) {
    spider_png_fail(job,"F9-PNG: cannot create writable screenshots folder",
                    L"Cannot create screenshots folder (game or LOCALAPPDATA)");
    goto done;
  }
  if(swprintf(temp,MAX_PATH,L"%ls\\SpiderF9PNG-%lu-%ld.tmp",
              exe,(unsigned long)GetCurrentProcessId(),(long)serial)>=MAX_PATH) {
    spider_png_fail(job,"F9-PNG: temporary filename too long",L"Screenshot file path is too long");
    goto done;
  }
  if(image_save(bitmap,temp,&png_encoder,NULL)!=0) {
    spider_png_fail(job,"F9-PNG: PNG encoder rejected image",L"PNG encoding failed");
    DeleteFileW(temp);
    goto done;
  }

  /* Non-overwriting atomic publication. The GDI+ intermediate has PNG bytes,
   * not a legacy BMP; no BMP is ever created by our encoder. */
  for(n=0;n<10000u;n++) {
    if(swprintf(dest,MAX_PATH,L"%ls\\scrn%04u.png",exe,n)>=MAX_PATH)break;
    if(MoveFileExW(temp,dest,MOVEFILE_WRITE_THROUGH)) {
      if(!spider_png_verify(dest)) {
        spider_png_fail(job,"F9-PNG: final PNG verification failed",
                        L"Saved PNG could not be verified");
        DeleteFileW(dest);
        saved=2; /* failure already reported; no second popup */
        break;
      }
      snprintf(msg,sizeof(msg),"F9-PNG: saved screenshots/scrn%04u.png %lux%lu (native lossless)",
               (unsigned)n,(unsigned long)job->width,(unsigned long)job->height);
      spider_png_log(job,msg);
      /* Inform the game thread only after the final PNG exists. */
      if(job->static_comic)SpiderF9Toast_PublishSavedPathComic(dest);
      else SpiderF9Toast_PublishSavedPath(dest);
      saved=1;
      break;
    }
    error=GetLastError();
    if(error!=ERROR_ALREADY_EXISTS && error!=ERROR_FILE_EXISTS)break;
  }
  if(!saved) {
    snprintf(msg,sizeof(msg),"F9-PNG: unable to publish PNG Win32=%lu",
             (unsigned long)error);
    spider_png_fail(job,msg,(error==ERROR_ACCESS_DENIED)?
                    L"Access denied while saving PNG":
                    (error==ERROR_DISK_FULL || error==ERROR_HANDLE_DISK_FULL)?
                    L"Disk full while saving PNG":
                    L"Cannot write screenshot PNG");
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

static int spider_png_queue_internal(const DDSURFACEDESC2 *frame,
                                     void (*log_fn)(const char *),
                                     BOOL static_comic) {
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
    { SpiderF9Toast_PublishError(L"Unsupported screenshot pixel format"); return 0; }
  width=frame->dwWidth;
  height=frame->dwHeight;
  row_bytes=(SIZE_T)width*4u;
  if(frame->lPitch<(LONG)row_bytes) {
    SpiderF9Toast_PublishError(L"Invalid screenshot image layout");
    return 0;
  }
  if(InterlockedCompareExchange(&spider_png_busy,1,0)!=0) {
    if(log_fn)log_fn("F9-PNG: capture skipped; encoder still busy");
    SpiderF9Toast_PublishError(L"Previous screenshot is still saving");
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
  job->static_comic=static_comic;
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
  SpiderF9Toast_PublishError(L"Cannot allocate screenshot buffer or start encoder");
  return 0;
}

int SpiderF9PNG_Queue(const DDSURFACEDESC2 *frame,void (*log_fn)(const char *)) {
  return spider_png_queue_internal(frame,log_fn,FALSE);
}
int SpiderF9PNG_QueueComic(const DDSURFACEDESC2 *frame,
                           void (*log_fn)(const char *)) {
  return spider_png_queue_internal(frame,log_fn,TRUE);
}
