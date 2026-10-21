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


static int pause_draw_text(HDC dc,HFONT font,const wchar_t *label,
                           int box_left,int box_width,int y,COLORREF color,
                           int outline) {
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
  for(dy=-outline;dy<=outline;dy++)
    for(dx=-outline;dx<=outline;dx++)
      if(dx*dx+dy*dy<=outline*outline && (dx || dy))
        TextOutW(dc,x+dx,y+dy,label,len);
  SetTextColor(dc,color);
  TextOutW(dc,x,y,label,len);
  SelectObject(dc,old);
  return 1;
}

/* ESC artwork geometry in the original 1440x1080 D3D7 4:3 viewport.
 * The 2026-10-21 native DrawPrimitive log recorded four exact quads PER
 * SIDE with D3DBLEND_ZERO / D3DBLEND_INVSRCCOLOR (src=1,dst=4).
 * Reversed-winding duplicate calls are the same visible quad, NOT extra
 * opaque layers. GDI compositing below is an appearance approximation:
 * native multiplicative color blending is not directly supported by
 * AlphaBlend, and the compact central body consists of smaller primitives
 * not included in this bounded trace. NEVER claim those pixels are exact.
 * The violet panel and four 8x8 border strip positions are also traced.
 *
 * Coordinates stay on the actual content rectangle (4:3 pillarbox-safe);
 * no new HWNDs, input hooks, Direct3D state changes, or game file writes.
 */
typedef struct EscNativePoint {
  float x,y;
} EscNativePoint;
static int esc_coordf(float v,int size,int design) {
  float scaled=(v*(float)size)/(float)design;
  return (int)(scaled+(scaled>=0.0f?0.5f:-0.5f));
}
static void esc_tinted_quad(HDC dc,HDC tint,const RECT *view,
                            const EscNativePoint corners[4],int mirror,
                            COLORREF tint_color,BYTE alpha) {
  POINT pts[4];
  HRGN region;
  int i,l,r,t,b,state;
  for(i=0;i<4;i++) {
    float x=mirror?(float)ESC_DESIGN_W-corners[i].x:corners[i].x;
    pts[i].x=view->left+esc_coordf(x,view->right-view->left,ESC_DESIGN_W);
    pts[i].y=view->top+esc_coordf(corners[i].y,
                                  view->bottom-view->top,ESC_DESIGN_H);
  }
  l=r=pts[0].x;t=b=pts[0].y;
  for(i=1;i<4;i++) {
    if(pts[i].x<l)l=pts[i].x;
    if(pts[i].x>r)r=pts[i].x;
    if(pts[i].y<t)t=pts[i].y;
    if(pts[i].y>b)b=pts[i].y;
  }
  if(l>=r || t>=b)return;
  region=CreatePolygonRgn(pts,4,WINDING);
  if(!region)return;
  state=SaveDC(dc);
  if(state) {
    if(ExtSelectClipRgn(dc,region,RGN_AND)!=ERROR)
      pause_blend_color(dc,tint,l,t,r-l+1,b-t+1,tint_color,alpha);
    RestoreDC(dc,state);
  }
  DeleteObject(region);
}
static void esc_motif_core(HDC dc,HDC tint,const RECT *view) {
  /* The narrow center/body shape has NOT been traced in full; temporarily
   * use a subdued, clipped oval instead of the old huge faux gear spokes.
   * Do not redistribute textures or claim this geometry is game-exact. */
  int w=view->right-view->left,h=view->bottom-view->top;
  int left=view->left+esc_coordf(395.0f,w,ESC_DESIGN_W);
  int right=view->left+esc_coordf(1045.0f,w,ESC_DESIGN_W);
  int top=view->top+esc_coordf(65.0f,h,ESC_DESIGN_H);
  int bottom=view->top+esc_coordf(902.0f,h,ESC_DESIGN_H);
  HRGN region=CreateEllipticRgn(left,top,right,bottom);
  int state;
  if(!region)return;
  state=SaveDC(dc);
  if(state) {
    if(ExtSelectClipRgn(dc,region,RGN_AND)!=ERROR)
      pause_blend_color(dc,tint,left,top,right-left,bottom-top,
                        RGB(142,62,39),45);
    RestoreDC(dc,state);
  }
  DeleteObject(region);
}
static void esc_draw_motif(HDC dc,HDC tint,const RECT *view) {
  /* Exact original ordered quad vertices (first of each duplicate pair).
   * LEFT half: captured seq=3873,3867,3851,3849 respectively.
   * RIGHT half is obtained by reflecting about the x=720 center line.
   * All eight original quads have diffuse ARGB 0x80868620.
   * D3D src=ZERO, dst=INVSRCCOLOR cannot be matched exactly by GDI alpha,
   * so apply a subdued rust/orange tint, instead of inventing extra legs. */
  static const EscNativePoint left[4][4]={
    {{340.3f,58.5f},{219.4f,0.0f},{0.0f,0.0f},{255.9f,234.0f}},
    {{255.9f,234.0f},{0.0f,0.0f},{0.0f,306.0f},{216.6f,333.0f}},
    {{210.9f,625.5f},{0.0f,657.0f},{0.0f,864.0f},{247.5f,810.0f}},
    {{306.6f,904.5f},{0.0f,904.5f},{0.0f,1053.0f},{430.3f,1053.0f}}
  };
  int mirror,i;
  esc_motif_core(dc,tint,view);
  for(mirror=0;mirror<2;mirror++)
    for(i=0;i<4;i++)
      esc_tinted_quad(dc,tint,view,left[i],mirror,RGB(152,64,32),85);
}

static int pause_draw_composite(HDC dc,int width,int height,HDC tint_dc,
                                const RECT *content_rect) {
  RECT canvas={0,0,0,0};
  HFONT title=NULL,hint=NULL;
  int saved,ok=1,cw,ch;
  int y_top,y_bottom,upper,lower,vertical,left_edge,right_edge;
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
  esc_draw_motif(dc,tint_dc,&canvas);

  /* Original panel draw: diffuse 0x7F3232A0, src=SRCALPHA,
   * dst=INVSRCALPHA, x=-5.6..1442.8, y=400.5..675.0.
   * GDI α=127 is a close compositing equivalent for this filled quad. */
  upper=canvas.top+esc_coordf(400.5f,ch,ESC_DESIGN_H);
  lower=canvas.top+esc_coordf(675.0f,ch,ESC_DESIGN_H);
  if(!pause_blend_color(dc,tint_dc,canvas.left,upper,cw,lower-upper,
                        RGB(50,50,160),127))ok=0;

  /* Border geometry from trace. The native 8x8 edge texels have not
   * yet been recovered; the line/rim treatment is intentionally a
   * visual placeholder, not a representation of extracted assets. */
  y_top=canvas.top+esc_coordf(396.0f,ch,ESC_DESIGN_H);
  y_bottom=canvas.top+esc_coordf(687.0f,ch,ESC_DESIGN_H);
  left_edge=canvas.left;
  right_edge=canvas.right;
  vertical=(ch>=720)?3:2;
  if(ok) {
    pause_blend_color(dc,tint_dc,left_edge,y_top,cw,vertical,
                      RGB(176,163,248),228);
    pause_blend_color(dc,tint_dc,left_edge,y_bottom,cw,vertical,
                      RGB(176,163,248),228);
    pause_blend_color(dc,tint_dc,left_edge,y_top,vertical,
                      y_bottom-y_top+vertical,RGB(152,137,237),192);
    pause_blend_color(dc,tint_dc,right_edge-vertical,y_top,vertical,
                      y_bottom-y_top+vertical,RGB(152,137,237),192);
  }

  title=CreateFontW(-((53*ch+540)/1080),0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,
                    DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                    ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  hint=CreateFontW(-((34*ch+540)/1080),0,0,0,FW_BOLD,TRUE,FALSE,FALSE,
                   DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                   ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  if(!title || !hint)ok=0;
  if(ok) {
    SetBkMode(dc,TRANSPARENT);
    SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
    ok=pause_draw_text(dc,title,L"PAUSED",canvas.left,cw,
                       canvas.top+esc_coordf(171.0f,ch,ESC_DESIGN_H),
                       RGB(238,42,64),3);
    if(ok)
      ok=pause_draw_text(dc,hint,L"PRESS ANY KEY TO RESUME",
                         canvas.left,cw,
                         canvas.top+esc_coordf(515.0f,ch,ESC_DESIGN_H),
                         RGB(244,240,255),1);
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
       "FOCUS-SURFACE: frozen scene+%d%% shade + native-ESC-inspired geometric pause (grid=%d) presented in game's DirectDraw surface",
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
