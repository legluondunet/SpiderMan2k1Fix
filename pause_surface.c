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
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include "pause_surface.h"

#define PAUSE_GRID_STEP 16
#define PAUSE_PANEL_W 470
#define PAUSE_PANEL_H 108
#define PAUSE_MAX_W 4096
#define PAUSE_MAX_H 2160

static HDC saved_dc=NULL;
static HBITMAP saved_bitmap=NULL, saved_old=NULL;
static IDirectDrawSurface7 *saved_surface=NULL;
static int enabled_dim=1,enabled_grid=1,enabled_banner=1,opacity=55;
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

static int pause_draw_text(HDC dc,HFONT font,const wchar_t *label,
                           int box_left,int box_width,int y) {
  SIZE length={0,0};
  HGDIOBJ old;
  int x,dx,dy,len=lstrlenW(label);
  if(!font)return 0;
  old=SelectObject(dc,font);
  if(!GetTextExtentPoint32W(dc,label,len,&length)) {
    SelectObject(dc,old);return 0;
  }
  x=box_left+(box_width-length.cx)/2;
  if(x<box_left+8)x=box_left+8;
  SetTextColor(dc,RGB(6,5,12));
  for(dy=-1;dy<=1;dy++)for(dx=-1;dx<=1;dx++)
    if(dx || dy)TextOutW(dc,x+dx,y+dy,label,len);
  SetTextColor(dc,RGB(255,255,255));
  TextOutW(dc,x,y,label,len);
  SelectObject(dc,old);
  return 1;
}

static int pause_draw_composite(HDC dc,int width,int height,HDC tint_dc) {
  int x,y,box_left,box_top,box_width,box_height,saved;
  HPEN grid=NULL,border=NULL;
  HGDIOBJ old_pen,old_brush;
  HRGN rounded=NULL;
  HFONT title=NULL,hint=NULL;
  int ok=1;
  const int panel=(enabled_banner && width>=PAUSE_PANEL_W+40 &&
                   height>=PAUSE_PANEL_H+40);
  box_width=PAUSE_PANEL_W;
  box_height=PAUSE_PANEL_H;
  box_left=(width-box_width)/2;
  box_top=(height-box_height)/2;

  if(enabled_dim && opacity>0) {
    if(!pause_blend_color(dc,tint_dc,0,0,width,height,RGB(0,0,0),
                          (BYTE)((opacity*255+50)/100)))return 0;
  }
  if(enabled_dim && enabled_grid) {
    saved=SaveDC(dc);
    if(!saved)return 0;
    if(panel)
      ExcludeClipRect(dc,box_left-5,box_top-5,
                         box_left+box_width+5,box_top+box_height+5);
    grid=CreatePen(PS_SOLID,1,RGB(59,60,76));
    if(!grid) {RestoreDC(dc,saved);return 0;}
    old_pen=SelectObject(dc,grid);
    for(x=PAUSE_GRID_STEP;x<width;x+=PAUSE_GRID_STEP) {
      MoveToEx(dc,x,0,NULL);LineTo(dc,x,height);
    }
    for(y=PAUSE_GRID_STEP;y<height;y+=PAUSE_GRID_STEP) {
      MoveToEx(dc,0,y,NULL);LineTo(dc,width,y);
    }
    SelectObject(dc,old_pen);
    DeleteObject(grid);
    RestoreDC(dc,saved);
  }
  if(!panel)return 1;

  /* The local panel is a single GDI draw operation sequence directly on
   * the frozen game surface, not another child/popup/window-manager layer. */
  saved=SaveDC(dc);
  if(!saved)return 0;
  rounded=CreateRoundRectRgn(box_left,box_top,box_left+box_width,
                              box_top+box_height,20,20);
  if(!rounded) {RestoreDC(dc,saved);return 0;}
  ExtSelectClipRgn(dc,rounded,RGN_AND);
  if(!pause_blend_color(dc,tint_dc,box_left,box_top,
                        box_width,box_height,RGB(15,11,25),222))ok=0;
  RestoreDC(dc,saved);
  DeleteObject(rounded);
  if(!ok)return 0;

  border=CreatePen(PS_SOLID,2,RGB(178,45,63));
  if(!border)return 0;
  old_pen=SelectObject(dc,border);
  old_brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
  RoundRect(dc,box_left+1,box_top+1,box_left+box_width-1,
            box_top+box_height-1,20,20);
  SelectObject(dc,old_brush);
  SelectObject(dc,old_pen);
  DeleteObject(border);

  title=CreateFontW(-30,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                    NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  hint=CreateFontW(-20,0,0,0,FW_HEAVY,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                   OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                   NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  if(!title || !hint)ok=0;
  if(ok) {
    SetBkMode(dc,TRANSPARENT);
    SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
    ok=pause_draw_text(dc,title,L"PAUSE",box_left,box_width,box_top+12) &&
       pause_draw_text(dc,hint,L"PRESS ANY KEY TO RESUME",
                       box_left,box_width,box_top+65);
  }
  if(title)DeleteObject(title);
  if(hint)DeleteObject(hint);
  return ok;
}

void SpiderPauseSurface_Init(void (*log_fn)(const char *)) {
  char path[MAX_PATH],*slash;
  DWORD n;
  pause_log=log_fn;
  enabled_dim=1;enabled_grid=1;enabled_banner=1;opacity=55;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(n && n<sizeof(path) && (slash=strrchr(path,'\\')) &&
     (size_t)(slash-path)+sizeof("SpiderMan2k1Fix.ini")<=sizeof(path)) {
    strcpy(slash+1,"SpiderMan2k1Fix.ini");
    enabled_dim=GetPrivateProfileIntA("Input","PauseDim",1,path)==1;
    enabled_grid=GetPrivateProfileIntA("Input","PauseGrid",1,path)==1;
    enabled_banner=GetPrivateProfileIntA("Input","PauseOverlay",1,path)==1;
    opacity=GetPrivateProfileIntA("Input","PauseDimOpacity",55,path);
  }
  if(opacity<0)opacity=0;
  if(opacity>90)opacity=90;
  if(pause_log)pause_log("FOCUS-SURFACE: DirectDraw backbuffer pause renderer ready (no overlay HWND)");
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
                             const RECT *primary_dest,SpiderPauseBltFn present) {
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
  drawn=pause_draw_composite(dc,width,height,tint_dc);
  SelectObject(tint_dc,tint_old);tint_old=NULL;
  DeleteObject(tint);tint=NULL;
  DeleteDC(tint_dc);tint_dc=NULL;
  if(!drawn)goto fail;
  IDirectDrawSurface7_ReleaseDC(back,dc);dc=NULL;
  hr=present(primary,(LPRECT)primary_dest,back,NULL,DDBLT_WAIT,NULL);
  if(FAILED(hr)) {
    pause_log_hresult("pause frame present failed",hr);
    /* No visible changes were presented; do not overwrite the user's view. */
    goto fail;
  }
  IDirectDrawSurface7_AddRef(back);
  saved_surface=back;
  if(pause_log) {
    char line[180];
    snprintf(line,sizeof(line),
       "FOCUS-SURFACE: frozen scene+%d%% shade+%dpx grid+English banner presented in game's own DirectDraw surface",
       enabled_dim?opacity:0,enabled_grid?PAUSE_GRID_STEP:0);
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
