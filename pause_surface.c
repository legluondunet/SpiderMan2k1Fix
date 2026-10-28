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
#include <shellapi.h> /* ExtractIconExW (native first executable icon group) */
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include "pause_surface.h"

#define ESC_DESIGN_W 1440
#define ESC_DESIGN_H 1080
#define PAUSE_MAX_W 4096
#define PAUSE_MAX_H 2160

static HDC saved_dc=NULL;
static HBITMAP saved_bitmap=NULL, saved_old=NULL;
static IDirectDrawSurface7 *saved_surface=NULL;
static HICON pause_game_icon=NULL; /* biggest embedded SpideyPC.exe icon */
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


/* No large motif behind the ribbon. Extract the original SpideyPC.exe
 * icon at runtime, never from a bundled .ico or a guessed resource ID.
 * The original ICO supplied for reference contains 16x16 and 32x32:
 * ExtractIconExW returns the OS-designated LARGE (32x32) icon group.
 * DrawIconEx scales that original color icon to our small ribbon marker. */
static int esc_coordf(float v,int size,int design) {
  float scaled=(v*(float)size)/(float)design;
  return (int)(scaled+(scaled>=0.0f?0.5f:-0.5f));
}
static void pause_load_game_icon(void) {
  WCHAR exe[MAX_PATH];
  DWORD n;
  HICON large=NULL,small=NULL;
  UINT extracted;
  if(pause_game_icon) {
    DestroyIcon(pause_game_icon);
    pause_game_icon=NULL;
  }
  n=GetModuleFileNameW(NULL,exe,MAX_PATH);
  if(!n || n>=MAX_PATH) {
    if(pause_log)pause_log("FOCUS-SURFACE: game EXE path unavailable; pause icon omitted");
    return;
  }
  extracted=ExtractIconExW(exe,0,&large,&small,1);
  if(extracted>0 && extracted!=UINT_MAX) {
    /* The LARGE HICON is the highest-resolution game icon supplied to
     * legacy shell extraction (32x32 if the EXE contains 16/32 only). */
    if(large) {
      pause_game_icon=large;
      large=NULL;
    } else if(small) {
      pause_game_icon=small;
      small=NULL;
    }
  }
  if(large)DestroyIcon(large);
  if(small)DestroyIcon(small);
  if(pause_log)pause_log(pause_game_icon?
    "FOCUS-SURFACE: original SpideyPC.exe large icon extracted for ribbon":
    "FOCUS-SURFACE: original EXE icon unavailable; retaining text-only ribbon");
}
static void pause_draw_game_icon(HDC dc,int x,int y,int size) {
  /* Decoration failure must never disable focus-loss pausing or text. */
  if(pause_game_icon && size>=8 &&
     !DrawIconEx(dc,x,y,pause_game_icon,size,size,0,NULL,DI_NORMAL) && pause_log)
    pause_log("FOCUS-SURFACE: DrawIconEx failed; pause text remains usable");
}

static int pause_draw_text(HDC dc,HFONT font,const wchar_t *label,
                           int box_left,int box_width,int y,COLORREF color,
                           int outline,int with_spider,int icon_size) {
  SIZE length={0,0};
  HGDIOBJ old;
  int x,dx,dy,len=lstrlenW(label),gap=0,full_width;
  int ok=1;
  if(!font)return 0;
  old=SelectObject(dc,font);
  if(!GetTextExtentPoint32W(dc,label,len,&length)) {
    SelectObject(dc,old);return 0;
  }
  if(with_spider && pause_game_icon)gap=icon_size/4;
  full_width=length.cx+((with_spider && pause_game_icon)?icon_size+gap:0);
  x=box_left+(box_width-full_width)/2;
  if(x<box_left+8)x=box_left+8;
  if(with_spider && pause_game_icon) {
    pause_draw_game_icon(dc,x,y+(length.cy-icon_size)/2,icon_size);
    x+=icon_size+gap;
  }
  SetTextColor(dc,RGB(7,4,15));
  for(dy=-outline;dy<=outline;dy++)
    for(dx=-outline;dx<=outline;dx++)
      if(dx*dx+dy*dy<=outline*outline && (dx || dy))
        TextOutW(dc,x+dx,y+dy,label,len);
  SetTextColor(dc,color);
  if(!TextOutW(dc,x,y,label,len))ok=0;
  SelectObject(dc,old);
  return ok;
}

static int pause_draw_composite(HDC dc,int width,int height,HDC tint_dc,
                                const RECT *content_rect) {
  RECT canvas={0,0,0,0};
  RECT ui_rect={0,0,0,0};
  HFONT title=NULL,hint=NULL;
  int saved,ok=1,cw,ch,ui_width,ui_max_width;
  int y_top,y_bottom,upper,lower,vertical,left_edge,right_edge;
  int stripe,height_band,band_top,band_end;
  canvas.right=width;canvas.bottom=height;
  if(content_rect &&
     content_rect->left>=0 && content_rect->top>=0 &&
     content_rect->right<=width && content_rect->bottom<=height &&
     content_rect->right>content_rect->left &&
     content_rect->bottom>content_rect->top)
    canvas=*content_rect;
  cw=canvas.right-canvas.left;ch=canvas.bottom-canvas.top;
  /* Keep the ESC ribbon at native 4:3 width even if 3D gameplay fills a
   * 16:9 surface. In an already centered 4:3 menu/content rectangle this
   * is an identity mapping; dimming and saved-frame restoration stay
   * full-frame. Limits only the ribbon, its borders and its text/icon. */
  ui_rect=canvas;
  ui_max_width=(ch*4)/3;
  ui_width=cw<ui_max_width?cw:ui_max_width;
  ui_rect.left=canvas.left+(cw-ui_width)/2;
  ui_rect.right=ui_rect.left+ui_width;

  /* PauseDimOpacity is intentionally independent of the visual design. */
  if(enabled_dim && opacity>0) {
    if(!pause_blend_color(dc,tint_dc,0,0,width,height,RGB(0,0,0),
                          (BYTE)((opacity*255+50)/100)))return 0;
  }
  if(!enabled_banner)return 1;

  saved=SaveDC(dc);
  if(!saved)return 0;
  IntersectClipRect(dc,canvas.left,canvas.top,canvas.right,canvas.bottom);

  /* Native violet ESC panel: original diffuse ARGB 0x7F3232A0 with
   * SRCALPHA / INVSRCALPHA; screen-space y=400.5..675 (1440x1080).
   * Slight blue-to-amethyst shading approximates the original menu's
   * blue-violet tint after our independent focus-loss dim (default 60%). */
  upper=canvas.top+esc_coordf(400.5f,ch,ESC_DESIGN_H);
  lower=canvas.top+esc_coordf(675.0f,ch,ESC_DESIGN_H);
  if(!pause_blend_color(dc,tint_dc,ui_rect.left,upper,ui_width,lower-upper,
                        RGB(50,50,160),127))ok=0;
  if(ok) {
    height_band=lower-upper;
    for(stripe=0;stripe<12;stripe++) {
      band_top=upper+(height_band*stripe)/12;
      band_end=upper+(height_band*(stripe+1))/12;
      if(band_end>band_top &&
         !pause_blend_color(dc,tint_dc,ui_rect.left,band_top,ui_width,
                            band_end-band_top,
                            RGB(41+3*stripe,36+2*stripe,106+stripe),24))
        ok=0;
    }
  }

  /* Measured edge strips: original 8x8 texture pixels are NOT recovered.
   * Two crisp lavender lines and a dark inner rim approximate the ESC
   * menu's luminous top and bottom. No new assets are distributed. */
  y_top=canvas.top+esc_coordf(396.0f,ch,ESC_DESIGN_H);
  y_bottom=canvas.top+esc_coordf(675.0f,ch,ESC_DESIGN_H);
  left_edge=ui_rect.left;
  right_edge=ui_rect.right;
  vertical=ch>=720?3:2;
  if(ok) {
    ok=pause_blend_color(dc,tint_dc,left_edge,y_top,ui_width,vertical,
                         RGB(169,148,251),229);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_top+vertical,ui_width,1,
                              RGB(231,220,255),217);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_top+vertical+2,ui_width,2,
                              RGB(15,12,68),88);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_bottom,ui_width,vertical,
                              RGB(168,144,250),228);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_bottom+vertical,ui_width,1,
                              RGB(224,212,255),215);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_top,vertical,
                              y_bottom-y_top+vertical,RGB(150,129,238),190);
    if(ok)ok=pause_blend_color(dc,tint_dc,right_edge-vertical,y_top,vertical,
                              y_bottom-y_top+vertical,RGB(150,129,238),190);
  }

  title=CreateFontW(-((62*ch+540)/1080),0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,
                    DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                    ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  hint=CreateFontW(-((42*ch+540)/1080),0,0,0,FW_BOLD,TRUE,FALSE,FALSE,
                   DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                   ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  if(!title || !hint)ok=0;
  if(ok) {
    SetBkMode(dc,TRANSPARENT);
    SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
    ok=pause_draw_text(dc,title,L"PAUSED",ui_rect.left,ui_width,
                       canvas.top+esc_coordf(445.0f,ch,ESC_DESIGN_H),
                       RGB(240,47,69),4,0,0);
    if(ok)
      ok=pause_draw_text(dc,hint,L"PRESS KEY OR GAMEPAD BUTTON TO RESUME",
                         ui_rect.left,ui_width,
                         canvas.top+esc_coordf(545.0f,ch,ESC_DESIGN_H),
                         RGB(245,240,255),2,1,
                         esc_coordf(48.0f,ch,ESC_DESIGN_H));
  }
  if(title)DeleteObject(title);
  if(hint)DeleteObject(hint);
  RestoreDC(dc,saved);
  return ok;
}

void SpiderPauseSurface_Init(void (*log_fn)(const char *)) {
  char path[MAX_PATH],*slash,info[150];
  DWORD n;
  pause_log=log_fn;
  /* Permanent PAUSED panel; only the background shade opacity is
   * configurable. The INI intentionally omits PauseDimOpacity by default.
   * 0 = no black shade (the PAUSED panel and gameplay pause remain active);
   * 1..90 = requested shade strength, otherwise 60% by default. */
  enabled_dim=1;
  enabled_banner=1;
  opacity=60;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(n>0 && n<sizeof(path) && (slash=strrchr(path,'\\'))!=NULL &&
     (size_t)(slash-path)+sizeof("SpiderMan2k1Fix.ini")<=sizeof(path)) {
    strcpy(slash+1,"SpiderMan2k1Fix.ini");
    opacity=GetPrivateProfileIntA("Input","PauseDimOpacity",60,path);
  }
  /* The strict INI validator accepts 0..90. Fail closed on anomalous
   * values in a missing/unreadable file without affecting focus pause. */
  if(opacity<0 || opacity>90)opacity=60;
  pause_load_game_icon();
  if(pause_log) {
    snprintf(info,sizeof(info),
      "FOCUS-SURFACE: native PAUSED banner enabled; background opacity=%d%% (0=disabled, default=60%%)",
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
       "FOCUS-SURFACE: frozen scene+%d%% shade + native-ESC-inspired violet ribbon and spider icon presented in game's DirectDraw surface",
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
