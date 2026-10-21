/* Passive uniform-alpha shade for the verified Spider-Man 2001 game.
 *
 * The 2001 SpideyPC.exe has no Windows 8 compatibility manifest. Win32
 * can reject a layered child for such a process on Windows 11.
 * A NON-TOPMOST, owned WS_POPUP layered window works without changing the
 * original executable/manifest, and follows the owner in normal Z-order.
 *
 * The validated PAUSE banner stays the original shaped child HWND. Since a
 * top-level popup would otherwise cover that child, EXCLUDE only its shaped
 * frame/letter pixels from the dimmer region; the banner stays fully bright.
 * No changes to DirectDraw, Bink, DirectSound, keyboard/mouse or activation.
 * This is a focus-pause-only experiment, opt-out with [Input] PauseDim=0.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pause_dim.h"
#include "pause_overlay.h"

#define DIM_CLASS L"SpiderMan2k1Fix_PauseDim"
#define DIM_DEFAULT_OPACITY 55 /* Percent black; scene still visible. */
#define DIM_GRID_STEP 16        /* Screen pixels: small comic-style cells. */
#define DIM_GRID_COLOR RGB(54,58,67) /* Subtle light-gray lines through alpha. */
#define BG_DITHER_PERIOD 8 /* Legacy Win32 child: quantized spatial opacity. */
#define BG_MAX_WIDTH 4096
#define BG_MAX_HEIGHT 2160
#define BG_DARK_COLOR RGB(11,9,18)
#define BG_GRID_COLOR RGB(71,63,83)

static HWND dim_hwnd=NULL;
static HWND dim_background_hwnd=NULL;
static int dim_enabled=1;
static int dim_opacity=DIM_DEFAULT_OPACITY;
static BYTE dim_alpha=140;
static int dim_grid_enabled=1;
static int dim_registered=0;
static void (*dim_log)(const char *)=NULL;

/* A top-level layered shade can float over native Wine desktop windows.
 * Instead, when Spider-Man is in the background, use an ordinary WS_CHILD
 * clipped to opaque one-pixel rows and the 16px grid. Empty pixels are
 * removed from the actual window region: the original DirectDraw frame
 * remains visible, without relying on layered-child alpha support.
 * Distributed row coverage approximates the configured shade opacity
 * (55% -> 4/8 rows = 50%); the foreground retains exact alpha 55%. */
static HRGN dim_background_region(int width,int height,int rows,int grid) {
  HRGN region=NULL,strip=NULL;
  int x,y;
  if(width<=0 || height<=0 || width>BG_MAX_WIDTH || height>BG_MAX_HEIGHT)
    return NULL;
  region=CreateRectRgn(0,0,0,0);
  if(!region)return NULL;
  for(y=0;y<height;y++) {
    if((y*5)%BG_DITHER_PERIOD>=rows && (!grid || y%DIM_GRID_STEP))continue;
    strip=CreateRectRgn(0,y,width,y+1);
    if(!strip || CombineRgn(region,region,strip,RGN_OR)==ERROR)goto failed;
    DeleteObject(strip);strip=NULL;
  }
  if(grid)for(x=0;x<width;x+=DIM_GRID_STEP) {
    strip=CreateRectRgn(x,0,x+1,height);
    if(!strip || CombineRgn(region,region,strip,RGN_OR)==ERROR)goto failed;
    DeleteObject(strip);strip=NULL;
  }
  return region;
failed:
  if(strip)DeleteObject(strip);
  DeleteObject(region);
  return NULL;
}

static void dim_background_paint(HDC dc,const RECT *bounds) {
  int x,y;
  HBRUSH dark=CreateSolidBrush(BG_DARK_COLOR);
  HPEN grid_pen=NULL;
  HGDIOBJ old_pen;
  int shade_rows=(dim_opacity*BG_DITHER_PERIOD+50)/100;
  if(shade_rows>BG_DITHER_PERIOD)shade_rows=BG_DITHER_PERIOD;
  if(dark) {
    for(y=0;y<bounds->bottom;y++)if((y*5)%BG_DITHER_PERIOD<shade_rows) {
      RECT line={0,y,bounds->right,y+1};
      FillRect(dc,&line,dark);
    }
    DeleteObject(dark);
  }
  if(!dim_grid_enabled)return;
  grid_pen=CreatePen(PS_SOLID,1,BG_GRID_COLOR);
  if(!grid_pen)return;
  old_pen=SelectObject(dc,grid_pen);
  for(x=0;x<bounds->right;x+=DIM_GRID_STEP) {
    MoveToEx(dc,x,0,NULL);
    LineTo(dc,x,bounds->bottom);
  }
  for(y=0;y<bounds->bottom;y+=DIM_GRID_STEP) {
    MoveToEx(dc,0,y,NULL);
    LineTo(dc,bounds->right,y);
  }
  SelectObject(dc,old_pen);
  DeleteObject(grid_pen);
}

static LRESULT CALLBACK dim_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
  switch(message) {
    case WM_ERASEBKGND:
      return 1; /* All pixels painted at once; no opaque pre-flash. */
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc=BeginPaint(window,&ps);
      if(dc) {
        RECT bounds;
        GetClientRect(window,&bounds);
        if(window==dim_background_hwnd) {
          dim_background_paint(dc,&bounds);
          EndPaint(window,&ps);
          return 0;
        }
        FillRect(dc,&bounds,(HBRUSH)GetStockObject(BLACK_BRUSH));
        if(dim_grid_enabled) {
          /* Draw only once when the pause shade is first painted: this
           * popup is static until the user resumes. Window region still
           * clips out the centered red outline and white letters. */
          HPEN pen=CreatePen(PS_SOLID,1,DIM_GRID_COLOR);
          if(pen) {
            HGDIOBJ previous=SelectObject(dc,pen);
            int x,y;
            int saved=SaveDC(dc);
            RECT center;
            /* Never rasterize the 16px grid behind ANY center text. The
             * popup is above the child glyphs under Wine, so clipping
             * the complete rectangular banner is far more reliable than
             * relying solely on GDI font-path masks at individual pixels. */
            if(saved && SpiderPauseOverlay_GetBounds(GetWindow(window,GW_OWNER),&center))
              ExcludeClipRect(dc,center.left-8,center.top-8,
                                  center.right+8,center.bottom+8);
            for(x=DIM_GRID_STEP;x<bounds.right;x+=DIM_GRID_STEP) {
              MoveToEx(dc,x,0,NULL);
              LineTo(dc,x,bounds.bottom);
            }
            for(y=DIM_GRID_STEP;y<bounds.bottom;y+=DIM_GRID_STEP) {
              MoveToEx(dc,0,y,NULL);
              LineTo(dc,bounds.right,y);
            }
            if(saved)RestoreDC(dc,saved);
            SelectObject(dc,previous);
            DeleteObject(pen);
          }
        }
        EndPaint(window,&ps);
      }
      return 0;
    }
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_NCHITTEST:
      return HTTRANSPARENT;
    case WM_NCDESTROY:
      if(dim_hwnd==window)dim_hwnd=NULL;
      if(dim_background_hwnd==window)dim_background_hwnd=NULL;
      break;
  }
  return DefWindowProcW(window,message,wparam,lparam);
}

void SpiderPauseDim_Init(void (*log_fn)(const char *)) {
  char path[MAX_PATH],*slash;
  DWORD n;
  dim_log=log_fn;
  dim_enabled=1;
  dim_opacity=DIM_DEFAULT_OPACITY;
  dim_grid_enabled=1;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(n && n<sizeof(path) && (slash=strrchr(path,'\\'))!=NULL &&
     (size_t)(slash-path)+sizeof("SpiderMan2k1Fix.ini")<=sizeof(path)) {
    strcpy(slash+1,"SpiderMan2k1Fix.ini");
    dim_enabled=GetPrivateProfileIntA("Input","PauseDim",1,path)==1;
    dim_opacity=GetPrivateProfileIntA("Input","PauseDimOpacity",DIM_DEFAULT_OPACITY,path);
    dim_grid_enabled=GetPrivateProfileIntA("Input","PauseGrid",1,path)==1;
  }
  /* The INI validator rejects values above 90 or below zero; keep a
   * secondary bound in this isolated module as defense in depth. */
  if(dim_opacity<0)dim_opacity=0;
  if(dim_opacity>90)dim_opacity=90;
  dim_alpha=(BYTE)((dim_opacity*255+50)/100);
  if(dim_log) {
    char detail[160];
    snprintf(detail,sizeof(detail),
      "FOCUS-DIM: %s; opacity=%d%% alpha=%u/255 grid=%s step=%dpx",
      dim_enabled?"enabled":"disabled",dim_opacity,
      (unsigned)dim_alpha,dim_grid_enabled?"on":"off",DIM_GRID_STEP);
    dim_log(detail);
  }
}

/* The validated 55% gridded background is retained. The center banner
 * is now ONE independently composited per-pixel-alpha popup with opaque
 * lettering; its entire rounded rectangle is excluded from this shade. */
void SpiderPauseDim_Show(HWND game_window) {
  WNDCLASSW cls;
  HINSTANCE instance;
  RECT bounds;
  POINT origin={0,0};
  HWND window;
  HRGN visible_region=NULL;
  int width,height;
  DWORD error;
  char detail[160];

  if(!dim_enabled || !SpiderPauseOverlay_GameForeground(game_window))return;
  if(dim_hwnd && IsWindow(dim_hwnd))return;
  dim_hwnd=NULL;
  instance=GetModuleHandleW(NULL);
  if(!dim_registered) {
    memset(&cls,0,sizeof(cls));
    cls.lpfnWndProc=dim_proc;
    cls.hInstance=instance;
    cls.lpszClassName=DIM_CLASS;
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {
      error=GetLastError();
      snprintf(detail,sizeof(detail),
          "FOCUS-DIM: class registration failed Win32=%lu; skipping dim layer",
          (unsigned long)error);
      if(dim_log)dim_log(detail);
      return;
    }
    dim_registered=1;
  }
  if(!GetClientRect(game_window,&bounds))return;
  width=bounds.right-bounds.left;
  height=bounds.bottom-bounds.top;
  if(width<250 || height<120 || !ClientToScreen(game_window,&origin))return;

  /* Create an owned top-level popup HIDDEN. A layered child can fail for
   * an unmanifested Windows-2001 executable. The owner (NOT a desktop-topmost window)
   * retains the overlay's normal desktop ordering and lifetime. */
  window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|
       WS_EX_TOOLWINDOW|WS_EX_NOPARENTNOTIFY,
       DIM_CLASS,L"",WS_POPUP,
       origin.x,origin.y,width,height,game_window,NULL,instance,NULL);
  if(!window) {
    error=GetLastError();
    snprintf(detail,sizeof(detail),
        "FOCUS-DIM: owned-popup creation failed Win32=%lu; continuing without shade",
        (unsigned long)error);
    if(dim_log)dim_log(detail);
    return;
  }

  /* Remove the entire opaque-lettered comic banner from this gridded
   * alpha layer. No per-glyph region masks, no overlapping 35% popup. */
  visible_region=CreateRectRgn(0,0,width,height);
  if(!visible_region ||
     !SpiderPauseOverlay_ExcludeFromRegion(game_window,visible_region) ||
     !SetWindowRgn(window,visible_region,FALSE)) {
    error=GetLastError();
    if(visible_region)DeleteObject(visible_region);
    snprintf(detail,sizeof(detail),
        "FOCUS-DIM: shaped cutout failed Win32=%lu; skipping shade",
        (unsigned long)error);
    if(dim_log)dim_log(detail);
    DestroyWindow(window);
    return;
  }
  visible_region=NULL; /* Owned by Win32 after successful SetWindowRgn. */

  /* Uniform alpha must be installed BEFORE showing the window. On any
   * error destroy the hidden popup, never flash an opaque black rectangle. */
  if(!SetLayeredWindowAttributes(window,0,dim_alpha,LWA_ALPHA)) {
    error=GetLastError();
    snprintf(detail,sizeof(detail),
        "FOCUS-DIM: alpha unavailable Win32=%lu; skipping opaque layer",
        (unsigned long)error);
    if(dim_log)dim_log(detail);
    DestroyWindow(window);
    return;
  }
  dim_hwnd=window;
  InvalidateRect(window,NULL,FALSE);
  UpdateWindow(window);
  if(!SpiderPauseOverlay_GameForeground(game_window)) {
    dim_hwnd=NULL;
    DestroyWindow(window);
    return;
  }
  ShowWindow(window,SW_SHOWNOACTIVATE);
  if(dim_log) {
    snprintf(detail,sizeof(detail),
      "FOCUS-DIM: game-owned alpha %u/255 opacity=%d%% grid=%s step=%dpx; whole central banner excluded",
      (unsigned)dim_alpha,dim_opacity,dim_grid_enabled?"on":"off",DIM_GRID_STEP);
    dim_log(detail);
  }
}

/* Paint an approximate dark tint + exact grid in a NONLAYERED game child.
 * Window region is sparse, so undrawn pixels expose SpideyPC's original
 * paused scene; it cannot escape the game rectangle or float over Thunar.
 * Created before the centered PAUSE child so the text remains above it. */
void SpiderPauseDim_ShowBackground(HWND game_window) {
  WNDCLASSW cls;
  HINSTANCE instance;
  HWND window;
  RECT bounds;
  HRGN region=NULL;
  int width,height,rows;
  DWORD error;
  char detail[164];
  if(!dim_enabled || !game_window || !IsWindow(game_window) ||
     !IsWindowVisible(game_window) || IsIconic(game_window))return;
  if(dim_background_hwnd && IsWindow(dim_background_hwnd))return;
  if(!GetClientRect(game_window,&bounds))return;
  width=bounds.right-bounds.left;
  height=bounds.bottom-bounds.top;
  rows=(dim_opacity*BG_DITHER_PERIOD+50)/100;
  if(rows>BG_DITHER_PERIOD)rows=BG_DITHER_PERIOD;
  if((rows==0 && !dim_grid_enabled) ||
     width<250 || height<120 || width>BG_MAX_WIDTH || height>BG_MAX_HEIGHT)
    return;
  instance=GetModuleHandleW(NULL);
  if(!dim_registered) {
    memset(&cls,0,sizeof(cls));
    cls.lpfnWndProc=dim_proc;
    cls.hInstance=instance;
    cls.lpszClassName=DIM_CLASS;
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {
      if(dim_log)dim_log("FOCUS-DIM: background child class registration failed");
      return;
    }
    dim_registered=1;
  }
  region=dim_background_region(width,height,rows,dim_grid_enabled);
  if(!region) {
    if(dim_log)dim_log("FOCUS-DIM: background dither/grid region unavailable");
    return;
  }
  /* WS_CHILD always follows the game's X11/Win32 desktop Z-order; no
   * layered, topmost or owned top-level popup may exist in background. */
  window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_NOPARENTNOTIFY,
     DIM_CLASS,L"",WS_CHILD|WS_CLIPSIBLINGS,
     0,0,width,height,game_window,NULL,instance,NULL);
  if(!window) {
    error=GetLastError();
    DeleteObject(region);
    snprintf(detail,sizeof(detail),
      "FOCUS-DIM: background child creation failed Win32=%lu",
      (unsigned long)error);
    if(dim_log)dim_log(detail);
    return;
  }
  if(!SetWindowRgn(window,region,FALSE)) {
    DeleteObject(region);
    DestroyWindow(window);
    if(dim_log)dim_log("FOCUS-DIM: background child region failed");
    return;
  }
  region=NULL; /* Region ownership transferred to Win32. */
  dim_background_hwnd=window;
  ShowWindow(window,SW_SHOWNOACTIVATE);
  InvalidateRect(window,NULL,FALSE);
  UpdateWindow(window);
  if(dim_log) {
    snprintf(detail,sizeof(detail),
      "FOCUS-DIM: background game-child grid=%s 16px, dither=%d/%d rows (requested=%d%%)",
      dim_grid_enabled?"on":"off",rows,BG_DITHER_PERIOD,dim_opacity);
    dim_log(detail);
  }
}

void SpiderPauseDim_Hide(void) {
  HWND window=dim_hwnd;
  HWND child=dim_background_hwnd;
  dim_hwnd=NULL;
  dim_background_hwnd=NULL;
  if(child && IsWindow(child))DestroyWindow(child);
  if(window && IsWindow(window))DestroyWindow(window);
  if((window || child) && dim_log)
    dim_log("FOCUS-DIM: foreground shade/background child destroyed");
}
