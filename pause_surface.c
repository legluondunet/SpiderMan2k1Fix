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
#define PAUSE_PANEL_W 520
#define PAUSE_PANEL_H 128
#define PAUSE_ICON_SIZE 96
#define PAUSE_MAX_W 4096
#define PAUSE_MAX_H 2160

static HDC saved_dc=NULL;
static HBITMAP saved_bitmap=NULL, saved_old=NULL;
static IDirectDrawSurface7 *saved_surface=NULL;
static HICON pause_game_icon=NULL; /* first icon group from the verified SpideyPC.exe */
static int enabled_dim=1,enabled_grid=0,enabled_banner=1,opacity=50;
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


/* Draw the supported game's own embedded icon as a translucent watermark.
 * Icons from old Windows games may contain a monochrome AND mask instead of
 * usable 32-bit alpha. Render once over BLACK and once over WHITE to recover
 * coverage independently of the icon's original bit depth:
 *   black = alpha * color, white = black + (1-alpha) * 255.
 * The black image is then premultiplied and can safely be AlphaBlended.
 * This is an optional decoration; failure must NEVER prevent pausing.
 * Draw into the game's DirectDraw DC, without overlay windows or files. */
static int pause_draw_icon_watermark(HDC dest,int x,int y) {
  BITMAPINFO bmi;
  HDC black_dc=NULL,white_dc=NULL;
  HBITMAP black_bmp=NULL,white_bmp=NULL;
  HGDIOBJ black_old=NULL,white_old=NULL;
  DWORD *black_pixels=NULL,*white_pixels=NULL;
  const int count=PAUSE_ICON_SIZE*PAUSE_ICON_SIZE;
  BLENDFUNCTION blend={AC_SRC_OVER,0,88,AC_SRC_ALPHA};
  int i,visible=0,ok=0;
  if(!pause_game_icon)return 0;
  memset(&bmi,0,sizeof(bmi));
  bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth=PAUSE_ICON_SIZE;
  bmi.bmiHeader.biHeight=-PAUSE_ICON_SIZE; /* top-down, tightly packed */
  bmi.bmiHeader.biPlanes=1;
  bmi.bmiHeader.biBitCount=32;
  bmi.bmiHeader.biCompression=BI_RGB;
  black_dc=CreateCompatibleDC(dest);
  white_dc=CreateCompatibleDC(dest);
  if(!black_dc || !white_dc)goto cleanup;
  black_bmp=CreateDIBSection(black_dc,&bmi,DIB_RGB_COLORS,
                             (void **)&black_pixels,NULL,0);
  white_bmp=CreateDIBSection(white_dc,&bmi,DIB_RGB_COLORS,
                             (void **)&white_pixels,NULL,0);
  if(!black_bmp || !white_bmp || !black_pixels || !white_pixels)
    goto cleanup;
  black_old=SelectObject(black_dc,black_bmp);
  white_old=SelectObject(white_dc,white_bmp);
  if(!black_old || black_old==HGDI_ERROR ||
     !white_old || white_old==HGDI_ERROR)goto cleanup;
  for(i=0;i<count;i++) {
    black_pixels[i]=0x00000000u;
    white_pixels[i]=0x00FFFFFFu;
  }
  GdiFlush();
  if(!DrawIconEx(black_dc,0,0,pause_game_icon,PAUSE_ICON_SIZE,
                 PAUSE_ICON_SIZE,0,NULL,DI_NORMAL) ||
     !DrawIconEx(white_dc,0,0,pause_game_icon,PAUSE_ICON_SIZE,
                 PAUSE_ICON_SIZE,0,NULL,DI_NORMAL))goto cleanup;
  GdiFlush();
  for(i=0;i<count;i++) {
    DWORD b=black_pixels[i],w=white_pixels[i];
    int bb=(int)(b&255u),bg=(int)((b>>8)&255u),br=(int)((b>>16)&255u);
    int wb=(int)(w&255u),wg=(int)((w>>8)&255u),wr=(int)((w>>16)&255u);
    int diff=((wb-bb)+(wg-bg)+(wr-br)+1)/3;
    int alpha;
    if(diff<0)diff=0;
    if(diff>255)diff=255;
    alpha=255-diff;
    if(alpha)visible=1;
    /* black RGB is premultiplied, compatible with AC_SRC_ALPHA. */
    black_pixels[i]=(b&0x00FFFFFFu)|((DWORD)alpha<<24);
  }
  if(visible) {
    GdiFlush();
    ok=AlphaBlend(dest,x,y,PAUSE_ICON_SIZE,PAUSE_ICON_SIZE,
                  black_dc,0,0,PAUSE_ICON_SIZE,PAUSE_ICON_SIZE,blend)!=0;
  }
cleanup:
  if(black_dc && black_old && black_old!=HGDI_ERROR)
    SelectObject(black_dc,black_old);
  if(white_dc && white_old && white_old!=HGDI_ERROR)
    SelectObject(white_dc,white_old);
  if(black_bmp)DeleteObject(black_bmp);
  if(white_bmp)DeleteObject(white_bmp);
  if(black_dc)DeleteDC(black_dc);
  if(white_dc)DeleteDC(white_dc);
  return ok;
}

static int pause_draw_text(HDC dc,HFONT font,const wchar_t *label,
                           int box_left,int box_width,int y,COLORREF color) {
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
  SetTextColor(dc,color);
  TextOutW(dc,x,y,label,len);
  SelectObject(dc,old);
  return 1;
}

static int pause_draw_composite(HDC dc,int width,int height,HDC tint_dc,
                                const RECT *content_rect) {
  int x,y,box_left,box_top,box_width,box_height,saved;
  HPEN border=NULL,separator=NULL;
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
  /* Optional legacy 16px grid (default OFF): the original opaque grid overwhelmed the artwork and crossed the black
   * pillarboxes. Keep the same 16px spacing, but blend faint lines only over
   * the current 4:3 menu/game image (full canvas for widescreen gameplay).
   * This changes only the frozen pause frame, not any live rendering hook. */
  if(enabled_dim && enabled_grid) {
    const RECT pixel={0,0,1,1};
    const BLENDFUNCTION grid_blend={AC_SRC_OVER,0,28,0};
    int grid_left=0,grid_top=0,grid_right=width,grid_bottom=height;
    HBRUSH grid_brush;
    saved=SaveDC(dc);
    if(!saved)return 0;
    if(content_rect && content_rect->left>=0 && content_rect->top>=0 &&
       content_rect->right<=width && content_rect->bottom<=height &&
       content_rect->right>content_rect->left &&
       content_rect->bottom>content_rect->top) {
      grid_left=content_rect->left;
      grid_top=content_rect->top;
      grid_right=content_rect->right;
      grid_bottom=content_rect->bottom;
      IntersectClipRect(dc,grid_left,grid_top,grid_right,grid_bottom);
    }
    if(panel)
      ExcludeClipRect(dc,box_left-5,box_top-5,
                         box_left+box_width+5,box_top+box_height+5);
    grid_brush=CreateSolidBrush(RGB(100,104,124));
    if(!grid_brush) {RestoreDC(dc,saved);return 0;}
    if(!FillRect(tint_dc,&pixel,grid_brush)) {
      DeleteObject(grid_brush);RestoreDC(dc,saved);return 0;
    }
    DeleteObject(grid_brush);
    for(x=grid_left+PAUSE_GRID_STEP;x<grid_right;x+=PAUSE_GRID_STEP) {
      if(!AlphaBlend(dc,x,grid_top,1,grid_bottom-grid_top,
                     tint_dc,0,0,1,1,grid_blend)) {
        RestoreDC(dc,saved);return 0;
      }
    }
    for(y=grid_top+PAUSE_GRID_STEP;y<grid_bottom;y+=PAUSE_GRID_STEP) {
      if(!AlphaBlend(dc,grid_left,y,grid_right-grid_left,1,
                     tint_dc,0,0,1,1,grid_blend)) {
        RestoreDC(dc,saved);return 0;
      }
    }
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
                        box_width,box_height,RGB(11,16,26),232))ok=0;
  RestoreDC(dc,saved);
  DeleteObject(rounded);
  if(!ok)return 0;

  /* Optional real Spider-Man EXE icon, faint and clear of the centered text. */
  if(pause_game_icon && !pause_draw_icon_watermark(dc,box_left+27,
                                                   box_top+16) && pause_log)
    pause_log("FOCUS-SURFACE: game icon watermark unavailable; retaining plain banner");

  border=CreatePen(PS_SOLID,2,RGB(178,45,63));
  if(!border)return 0;
  old_pen=SelectObject(dc,border);
  old_brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
  RoundRect(dc,box_left+1,box_top+1,box_left+box_width-1,
            box_top+box_height-1,20,20);
  SelectObject(dc,old_brush);
  SelectObject(dc,old_pen);
  DeleteObject(border);

  /* Slightly larger title, calm secondary line and a small red divider.
   * Grayscale antialiasing avoids the harsh jagged lettering of the first
   * DirectDraw screenshot without introducing a compositor popup. */
  title=CreateFontW(-36,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                    ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  hint=CreateFontW(-18,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                   OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                   ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  if(!title || !hint)ok=0;
  if(ok) {
    SetBkMode(dc,TRANSPARENT);
    SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
    ok=pause_draw_text(dc,title,L"PAUSE",box_left,box_width,box_top+14,
                       RGB(255,255,255));
    separator=CreatePen(PS_SOLID,1,RGB(138,54,72));
    if(!separator)ok=0;
    if(separator) {
      old_pen=SelectObject(dc,separator);
      MoveToEx(dc,width/2-75,box_top+67,NULL);
      LineTo(dc,width/2+75,box_top+67);
      SelectObject(dc,old_pen);
      DeleteObject(separator);
    }
    if(ok)
      ok=pause_draw_text(dc,hint,L"PRESS ANY KEY TO RESUME",
                         box_left,box_width,box_top+79,RGB(224,227,234));
  }
  if(title)DeleteObject(title);
  if(hint)DeleteObject(hint);
  return ok;
}

void SpiderPauseSurface_Init(void (*log_fn)(const char *)) {
  char path[MAX_PATH],*slash;
  DWORD n;
  pause_log=log_fn;
  enabled_dim=1;enabled_grid=0;enabled_banner=1;opacity=50;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(n && n<sizeof(path) && (slash=strrchr(path,'\\')) &&
     (size_t)(slash-path)+sizeof("SpiderMan2k1Fix.ini")<=sizeof(path)) {
    strcpy(slash+1,"SpiderMan2k1Fix.ini");
    enabled_dim=GetPrivateProfileIntA("Input","PauseDim",1,path)==1;
    enabled_grid=GetPrivateProfileIntA("Input","PauseGrid",0,path)==1;
    enabled_banner=GetPrivateProfileIntA("Input","PauseOverlay",1,path)==1;
    opacity=GetPrivateProfileIntA("Input","PauseDimOpacity",50,path);
  }
  /* Extract, do not guess a resource ID: reuse the same first embedded
   * icon group used by the taskbar fix. No adjacent .ico or packaged assets. */
  if(pause_game_icon) { DestroyIcon(pause_game_icon); pause_game_icon=NULL; }
  {
    WCHAR exe[MAX_PATH];
    HICON large=NULL,small=NULL;
    DWORD path_len=GetModuleFileNameW(NULL,exe,MAX_PATH);
    if(path_len && path_len<MAX_PATH) {
      UINT extracted=ExtractIconExW(exe,0,&large,&small,1);
      if(extracted>0 && extracted!=UINT_MAX && large)
        pause_game_icon=large;
      else if(large)DestroyIcon(large);
      if(small)DestroyIcon(small);
    }
  }
  if(pause_log)pause_log(pause_game_icon ?
    "FOCUS-SURFACE: SpideyPC.exe icon ready for optional watermark" :
    "FOCUS-SURFACE: game icon missing; plain PAUSE banner retained");
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
                             const RECT *primary_dest,const RECT *content_rect,
                             SpiderPauseBltFn present) {
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
  drawn=pause_draw_composite(dc,width,height,tint_dc,content_rect);
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
       "FOCUS-SURFACE: frozen scene+%d%% shade+%dpx grid+English banner%s presented in game's own DirectDraw surface",
       enabled_dim?opacity:0,enabled_grid?PAUSE_GRID_STEP:0,
       pause_game_icon?"+game icon watermark":"");
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
