/* Spider-Man 2001: paused frame composited INSIDE DirectDraw, not desktop.
 *
 * The original game has an unmanifested 2001 Win32 window. Multiple
 * foreground/background alpha popups and child windows caused Wine/Xfce
 * compositing artifacts and focus leakage. A single GDI composition directly
 * into the game's validated DirectDraw7 BACKBUFFER avoids any overlay HWND.
 *
 * The saved backbuffer is restored when the focus pause ends. Media and game
 * still stop via the existing guarded Flip-boundary pause code, not here.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "spider_fix_paths.h"
#include "f9_toast.h" /* exact shared F9 notification painter */
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include "pause_surface.h"

#define PAUSE_MAX_W 4096
#define PAUSE_MAX_H 2160

static HDC saved_dc=NULL;
static HBITMAP saved_bitmap=NULL, saved_old=NULL;
static IDirectDrawSurface7 *saved_surface=NULL;
static int enabled_dim=1,enabled_banner=1,opacity=60;
static void (*pause_log)(const char *)=NULL;

static void pause_log_hresult(const char *step,HRESULT hr) {
  char line[170];
  if(!pause_log)return;
  snprintf(line,sizeof(line),"FOCUS-SURFACE: %s hr=0x%08lX",step,
           (unsigned long)hr);
  pause_log(line);
}

static int pause_blend_color(HDC dest,HDC source,int x,int y,int width,
                             int height,COLORREF color,BYTE alpha) {
  BLENDFUNCTION blend={AC_SRC_OVER,0,alpha,0};
  HBRUSH brush;
  RECT pixel={0,0,1,1};
  int result;
  brush=CreateSolidBrush(color);
  if(!brush)return 0;
  FillRect(source,&pixel,brush);
  DeleteObject(brush);
  result=AlphaBlend(dest,x,y,width,height,source,0,0,1,1,blend)!=0;
  return result;
}


/* Hold the original frozen backbuffer and optional dim, then paint
 * the EXACT F9 screenshot notification. This is a single bottom-left
 * panel with only PAUSED: no full-width ESC ribbon, resume instructions
 * or new HWND. F9 owns the shared GDI typography, 3-sided lavender
 * border, opacity and native 32px SpideyPC.exe icon. */
static int pause_draw_composite(HDC dc,int width,int height,HDC tint_dc,
                                const RECT *content_rect,int gamepad_available) {
  (void)content_rect; /* F9 panels use full-frame, flush-left anchoring. */
  (void)gamepad_available; /* Resume methods remain unchanged but unlabelled. */
  if(enabled_dim && opacity>0) {
    if(!pause_blend_color(dc,tint_dc,0,0,width,height,RGB(0,0,0),
                          (BYTE)((opacity*255+50)/100)))return 0;
  }
  if(!enabled_banner)return 1;
  return SpiderF9Toast_DrawPausedPanel(dc,(DWORD)width,(DWORD)height)!=FALSE;
}

void SpiderPauseSurface_Init(void (*log_fn)(const char *)) {
  char path[MAX_PATH],info[150];
  pause_log=log_fn;
  /* Permanent single-label PAUSED panel; only background shade opacity is
   * configurable. The INI intentionally omits PauseDimOpacity by default.
   * 0 = no black shade (the PAUSED panel and gameplay pause remain active);
   * 1..90 = requested shade strength, otherwise 60% by default. */
  enabled_dim=1;
  enabled_banner=1;
  opacity=60;
  if(spider_path_ini_a(path,sizeof(path)))
    opacity=GetPrivateProfileIntA("Input","PauseDimOpacity",60,path);
  /* The strict INI validator accepts 0..90. Fail closed on anomalous
   * values in a missing/unreadable file without affecting focus pause. */
  if(opacity<0 || opacity>90)opacity=60;
  if(pause_log) {
    snprintf(info,sizeof(info),
      "FOCUS-SURFACE: screenshot-style PAUSED panel enabled; background opacity=%d%% (0=disabled, default=60%%)",
      opacity);
    pause_log(info);
  }
}

void SpiderPauseSurface_End(void) {
  IDirectDrawSurface7 *surface=saved_surface;
  if(surface && saved_dc && saved_bitmap) {
    HDC dc=NULL;
    HRESULT hr=IDirectDrawSurface7_GetDC(surface,&dc);
    if(SUCCEEDED(hr) && dc) {
      DDSURFACEDESC2 desc;
      memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
      if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(surface,&desc)))
        if(!BitBlt(dc,0,0,(int)desc.dwWidth,(int)desc.dwHeight,
                   saved_dc,0,0,SRCCOPY) && pause_log)
          pause_log("FOCUS-SURFACE: warning restoring original backbuffer pixels failed");
      IDirectDrawSurface7_ReleaseDC(surface,dc);
    } else pause_log_hresult("GetDC restore unavailable",hr);
  }
  if(saved_dc && saved_old)SelectObject(saved_dc,saved_old);
  if(saved_bitmap)DeleteObject(saved_bitmap);
  if(saved_dc)DeleteDC(saved_dc);
  saved_dc=NULL;saved_bitmap=NULL;saved_old=NULL;
  saved_surface=NULL;
  if(surface)IDirectDrawSurface7_Release(surface);
}

int SpiderPauseSurface_Begin(IDirectDrawSurface7 *back,
                             IDirectDrawSurface7 *primary,
                             const RECT *primary_dest,const RECT *content_rect,
                             SpiderPauseBltFn present,
                             int gamepad_available) {
  DDSURFACEDESC2 desc;
  HDC dc=NULL,tint_dc=NULL;
  HBITMAP tint=NULL,tint_old=NULL;
  HRESULT hr;
  int width,height,drawn=0;
  if(!back || !primary || !primary_dest || !present)return 0;
  if(saved_surface)return 1; /* One pause latch, one snapshot. */
  if(!enabled_dim && !enabled_banner)return 0;
  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  hr=IDirectDrawSurface7_GetSurfaceDesc(back,&desc);
  if(FAILED(hr) || desc.dwWidth<320 || desc.dwHeight<240 ||
     desc.dwWidth>PAUSE_MAX_W || desc.dwHeight>PAUSE_MAX_H) {
    pause_log_hresult("invalid backbuffer geometry",hr);
    return 0;
  }
  width=(int)desc.dwWidth;height=(int)desc.dwHeight;
  hr=IDirectDrawSurface7_GetDC(back,&dc);
  if(FAILED(hr) || !dc) {
    pause_log_hresult("backbuffer GetDC unsupported; keep pause without visual",hr);
    return 0;
  }
  saved_dc=CreateCompatibleDC(dc);
  if(!saved_dc)goto fail;
  saved_bitmap=CreateCompatibleBitmap(dc,width,height);
  if(!saved_bitmap)goto fail;
  saved_old=(HBITMAP)SelectObject(saved_dc,saved_bitmap);
  if(!saved_old || saved_old==(HBITMAP)HGDI_ERROR) {
    saved_old=NULL;goto fail;
  }
  if(!BitBlt(saved_dc,0,0,width,height,dc,0,0,SRCCOPY))goto fail;
  tint_dc=CreateCompatibleDC(dc);
  if(!tint_dc)goto fail;
  tint=CreateCompatibleBitmap(dc,1,1);
  if(!tint)goto fail;
  tint_old=(HBITMAP)SelectObject(tint_dc,tint);
  if(!tint_old || tint_old==(HBITMAP)HGDI_ERROR) {
    tint_old=NULL;goto fail;
  }
  drawn=pause_draw_composite(dc,width,height,tint_dc,content_rect,
                              gamepad_available);
  SelectObject(tint_dc,tint_old);tint_old=NULL;
  DeleteObject(tint);tint=NULL;
  DeleteDC(tint_dc);tint_dc=NULL;
  if(!drawn)goto fail;
  IDirectDrawSurface7_ReleaseDC(back,dc);dc=NULL;
  /* Hold a separate backbuffer reference before presenting: this also
   * guarantees the original pixels are restored if the primary Blt fails. */
  IDirectDrawSurface7_AddRef(back);
  saved_surface=back;
  hr=present(primary,(LPRECT)primary_dest,back,NULL,DDBLT_WAIT,NULL);
  if(FAILED(hr)) {
    pause_log_hresult("pause frame present failed",hr);
    /* No visible changes were presented; do not overwrite the user's view. */
    goto fail;
  }
  if(pause_log) {
    char line[180];
    snprintf(line,sizeof(line),
       "FOCUS-SURFACE: frozen scene+%d%% shade + F9-style lavender PAUSED panel presented in game's DirectDraw surface",
       enabled_dim?opacity:0);
    pause_log(line);
  }
  return 1;
fail:
  if(tint_dc && tint_old)SelectObject(tint_dc,tint_old);
  if(tint)DeleteObject(tint);
  if(tint_dc)DeleteDC(tint_dc);
  if(dc) {
    if(saved_dc && saved_bitmap && saved_old)
      BitBlt(dc,0,0,width,height,saved_dc,0,0,SRCCOPY);
    IDirectDrawSurface7_ReleaseDC(back,dc);
  }
  SpiderPauseSurface_End();
  if(pause_log)pause_log("FOCUS-SURFACE: composition unavailable; game pause still functional");
  return 0;
}
