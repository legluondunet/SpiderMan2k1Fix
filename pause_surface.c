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
#define ESC_DESIGN_W 1440
#define ESC_DESIGN_H 1080
#define PAUSE_MAX_W 4096
#define PAUSE_MAX_H 2160

static HDC saved_dc=NULL;
static HBITMAP saved_bitmap=NULL, saved_old=NULL;
static IDirectDrawSurface7 *saved_surface=NULL;
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


/* Full-screen spider artwork removed by design. Keep only a small,
 * self-drawn red cog/spider emblem at the start of the resume instruction.
 * No texture extraction, Windows icon (Spidey face), or new popup HWND. */
static int esc_coordf(float v,int size,int design) {
  float scaled=(v*(float)size)/(float)design;
  return (int)(scaled+(scaled>=0.0f?0.5f:-0.5f));
}
static POINT pause_icon_point(int x,int y,int size,int px,int py) {
  POINT p;
  p.x=x+MulDiv(px,size,64);
  p.y=y+MulDiv(py,size,64);
  return p;
}
static int pause_draw_spider_icon(HDC dc,int x,int y,int size) {
  /* The original main-menu mark is a compact red cog/spider emblem,
   * not a long-legged silhouette. Reconstruct a ring with eight short
   * angular teeth, a dark hollow center, and a small specular highlight.
   * This is hand-drawn GDI art: no extracted game asset is distributed. */
  static const POINT teeth[8][4]={
    {{26,16},{25,3},{39,3},{38,16}},       /* top */
    {{42,18},{51,8},{60,17},{49,27}},      /* top right */
    {{48,26},{61,25},{61,39},{48,38}},     /* right */
    {{48,39},{60,47},{51,56},{42,46}},     /* bottom right */
    {{26,48},{25,61},{39,61},{38,48}},     /* bottom */
    {{22,46},{13,56},{4,47},{16,39}},      /* bottom left */
    {{16,26},{3,25},{3,39},{16,38}},       /* left */
    {{16,27},{4,17},{13,8},{22,18}}       /* top left */
  };
  POINT scaled[4];
  HBRUSH red=NULL,dark=NULL,shine=NULL,old_brush=NULL;
  HPEN outline=NULL,old_pen=NULL;
  int leg,i,ok=1;
  if(size<8)return 1;
  red=CreateSolidBrush(RGB(238,34,39));
  dark=CreateSolidBrush(RGB(28,7,16));
  shine=CreateSolidBrush(RGB(255,174,151));
  outline=CreatePen(PS_SOLID,size>=42?2:1,RGB(52,5,12));
  if(!red || !dark || !shine || !outline) {
    if(red)DeleteObject(red);
    if(dark)DeleteObject(dark);
    if(shine)DeleteObject(shine);
    if(outline)DeleteObject(outline);
    return 0;
  }
  old_brush=(HBRUSH)SelectObject(dc,red);
  old_pen=(HPEN)SelectObject(dc,outline);
  /* Compact outer teeth, in place of the former spidery X-shaped legs. */
  for(leg=0;leg<8;leg++) {
    for(i=0;i<4;i++)
      scaled[i]=pause_icon_point(x,y,size,teeth[leg][i].x,teeth[leg][i].y);
    if(!Polygon(dc,scaled,4))ok=0;
  }
  if(!Ellipse(dc,x+MulDiv(13,size,64),y+MulDiv(13,size,64),
               x+MulDiv(51,size,64),y+MulDiv(51,size,64)))ok=0;
  /* The true mark has a dark hollow center, not a vertical spider body. */
  SelectObject(dc,dark);
  if(!Ellipse(dc,x+MulDiv(22,size,64),y+MulDiv(22,size,64),
               x+MulDiv(42,size,64),y+MulDiv(42,size,64)))ok=0;
  /* A tiny inner highlight keeps the emblem legible at 48px. */
  SelectObject(dc,shine);
  if(!Ellipse(dc,x+MulDiv(36,size,64),y+MulDiv(26,size,64),
               x+MulDiv(40,size,64),y+MulDiv(30,size,64)))ok=0;
  SelectObject(dc,old_pen);
  SelectObject(dc,old_brush);
  DeleteObject(outline);
  DeleteObject(shine);
  DeleteObject(dark);
  DeleteObject(red);
  return ok;
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
  if(with_spider)gap=icon_size/4;
  full_width=length.cx+(with_spider?icon_size+gap:0);
  x=box_left+(box_width-full_width)/2;
  if(x<box_left+8)x=box_left+8;
  if(with_spider) {
    if(!pause_draw_spider_icon(dc,x,y+(length.cy-icon_size)/2,icon_size))
      ok=0;
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
  HFONT title=NULL,hint=NULL;
  int saved,ok=1,cw,ch;
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

  /* PauseDimOpacity is intentionally independent of the visual design. */
  if(enabled_dim && opacity>0) {
    if(!pause_blend_color(dc,tint_dc,0,0,width,height,RGB(0,0,0),
                          (BYTE)((opacity*255+50)/100)))return 0;
  }
  /* Optional legacy grid remains an opt-in and is OFF by default. */
  if(enabled_dim && enabled_grid) {
    int x,y,step_x=PAUSE_GRID_STEP,step_y=PAUSE_GRID_STEP;
    const BLENDFUNCTION grid_blend={AC_SRC_OVER,0,28,0};
    HBRUSH brush=CreateSolidBrush(RGB(100,104,124));
    RECT pixel={0,0,1,1};
    if(brush) {
      FillRect(tint_dc,&pixel,brush);
      DeleteObject(brush);
      saved=SaveDC(dc);
      if(saved) {
        IntersectClipRect(dc,canvas.left,canvas.top,canvas.right,canvas.bottom);
        for(x=canvas.left+step_x;x<canvas.right;x+=step_x)
          AlphaBlend(dc,x,canvas.top,1,ch,tint_dc,0,0,1,1,grid_blend);
        for(y=canvas.top+step_y;y<canvas.bottom;y+=step_y)
          AlphaBlend(dc,canvas.left,y,cw,1,tint_dc,0,0,1,1,grid_blend);
        RestoreDC(dc,saved);
      }
    }
  }
  if(!enabled_banner)return 1;

  saved=SaveDC(dc);
  if(!saved)return 0;
  IntersectClipRect(dc,canvas.left,canvas.top,canvas.right,canvas.bottom);

  /* Native violet ESC panel: original diffuse ARGB 0x7F3232A0 with
   * SRCALPHA / INVSRCALPHA; screen-space y=400.5..675 (1440x1080).
   * Slight blue-to-amethyst shading approximates the original menu's
   * blue-violet tint after our independent 50% focus-loss dim. */
  upper=canvas.top+esc_coordf(400.5f,ch,ESC_DESIGN_H);
  lower=canvas.top+esc_coordf(675.0f,ch,ESC_DESIGN_H);
  if(!pause_blend_color(dc,tint_dc,canvas.left,upper,cw,lower-upper,
                        RGB(50,50,160),127))ok=0;
  if(ok) {
    height_band=lower-upper;
    for(stripe=0;stripe<12;stripe++) {
      band_top=upper+(height_band*stripe)/12;
      band_end=upper+(height_band*(stripe+1))/12;
      if(band_end>band_top &&
         !pause_blend_color(dc,tint_dc,canvas.left,band_top,cw,
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
  left_edge=canvas.left;
  right_edge=canvas.right;
  vertical=ch>=720?3:2;
  if(ok) {
    ok=pause_blend_color(dc,tint_dc,left_edge,y_top,cw,vertical,
                         RGB(169,148,251),229);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_top+vertical,cw,1,
                              RGB(231,220,255),217);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_top+vertical+2,cw,2,
                              RGB(15,12,68),88);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_bottom,cw,vertical,
                              RGB(168,144,250),228);
    if(ok)ok=pause_blend_color(dc,tint_dc,left_edge,y_bottom+vertical,cw,1,
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
    ok=pause_draw_text(dc,title,L"PAUSED",canvas.left,cw,
                       canvas.top+esc_coordf(445.0f,ch,ESC_DESIGN_H),
                       RGB(240,47,69),4,0,0);
    if(ok)
      ok=pause_draw_text(dc,hint,L"PRESS ANY KEY TO RESUME",
                         canvas.left,cw,
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
       "FOCUS-SURFACE: frozen scene+%d%% shade + native-ESC-inspired violet ribbon and spider icon (grid=%d) presented in game's DirectDraw surface",
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
