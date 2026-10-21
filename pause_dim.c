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
#define DIM_PANEL_ALPHA 90  /* Extra 35% shade inside the banner. */
#define DIM_GRID_STEP 16        /* Screen pixels: small comic-style cells. */
#define DIM_GRID_COLOR RGB(54,58,67) /* Subtle light-gray lines through alpha. */

static HWND dim_hwnd=NULL;
static HWND dim_panel_hwnd=NULL;
static int dim_enabled=1;
static int dim_opacity=DIM_DEFAULT_OPACITY;
static BYTE dim_alpha=140;
static int dim_grid_enabled=1;
static int dim_registered=0;
static void (*dim_log)(const char *)=NULL;

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
        FillRect(dc,&bounds,(HBRUSH)GetStockObject(BLACK_BRUSH));
        if(dim_grid_enabled && window!=dim_panel_hwnd) {
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
      if(dim_panel_hwnd==window)dim_panel_hwnd=NULL;
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

/* The full-screen 55% shade remains as validated in PR #12. An OPTIONAL
 * secondary alpha popup deepens ONLY the center to about 71% black.
 * Both popups are owned by the game, nonactivating and non-topmost.
 * Because the original 2001 EXE cannot create layered child windows,
 * this is a separate *owned* popup, not a new child or DirectDraw hook.
 * The existing red/white shaped banner is cut out of BOTH regions so
 * neither the letters nor the border are dimmed. */
static void dim_show_center_panel(HWND game_window,HINSTANCE instance,
                                  POINT game_screen,int game_width,int game_height) {
  RECT box;
  HWND panel;
  HRGN region=NULL,rounded=NULL;
  int width,height;
  DWORD error;
  char detail[160];
  if(GetForegroundWindow()!=game_window ||
     !SpiderPauseOverlay_GetBounds(game_window,&box))return;
  width=box.right-box.left;
  height=box.bottom-box.top;
  if(width<100 || height<50 || box.left<0 || box.top<0 ||
     box.right>game_width || box.bottom>game_height)return;
  panel=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|
       WS_EX_TOOLWINDOW|WS_EX_NOPARENTNOTIFY,DIM_CLASS,L"",
       WS_POPUP,game_screen.x+box.left,game_screen.y+box.top,
       width,height,game_window,NULL,instance,NULL);
  if(!panel) {
    error=GetLastError();
    snprintf(detail,sizeof(detail),
       "FOCUS-DIM: optional center panel creation failed Win32=%lu",
       (unsigned long)error);
    if(dim_log)dim_log(detail);
    return;
  }
  region=CreateRectRgn(0,0,game_width,game_height);
  rounded=CreateRoundRectRgn(box.left+4,box.top+4,box.right-4,box.bottom-4,16,16);
  if(!region || !rounded ||
     !SpiderPauseOverlay_ExcludeFromRegion(game_window,region) ||
     CombineRgn(region,region,rounded,RGN_AND)==ERROR ||
     OffsetRgn(region,-box.left,-box.top)==ERROR ||
     !SetWindowRgn(panel,region,FALSE)) {
    error=GetLastError();
    if(region)DeleteObject(region);
    if(rounded)DeleteObject(rounded);
    DestroyWindow(panel);
    snprintf(detail,sizeof(detail),
       "FOCUS-DIM: optional center panel mask failed Win32=%lu",
       (unsigned long)error);
    if(dim_log)dim_log(detail);
    return;
  }
  region=NULL; /* Win32 owns successful SetWindowRgn region. */
  DeleteObject(rounded);
  if(!SetLayeredWindowAttributes(panel,0,DIM_PANEL_ALPHA,LWA_ALPHA)) {
    error=GetLastError();
    DestroyWindow(panel);
    snprintf(detail,sizeof(detail),
       "FOCUS-DIM: optional center panel alpha failed Win32=%lu",
       (unsigned long)error);
    if(dim_log)dim_log(detail);
    return;
  }
  dim_panel_hwnd=panel;
  InvalidateRect(panel,NULL,FALSE);
  UpdateWindow(panel);
  ShowWindow(panel,SW_SHOWNOACTIVATE);
  if(dim_log)dim_log("FOCUS-DIM: extra 35% alpha inside rounded center panel; PAUSE glyphs unshaded");
}

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

  if(!dim_enabled || !game_window || !IsWindow(game_window) ||
     GetForegroundWindow()!=game_window)return;
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

  /* Transparent hole for precisely the pixels of the previously installed
   * centered shaped banner: any top-level popup normally paints ABOVE
   * the owner's child HWNDs, so a plain alpha window would dim its text. */
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
  ShowWindow(window,SW_SHOWNOACTIVATE);
  if(dim_log) {
    snprintf(detail,sizeof(detail),
      "FOCUS-DIM: foreground-only owned popup alpha %u/255 opacity=%d%% grid=%s step=%dpx; center box grid-clipped",
      (unsigned)dim_alpha,dim_opacity,dim_grid_enabled?"on":"off",DIM_GRID_STEP);
    dim_log(detail);
  }
  dim_show_center_panel(game_window,instance,origin,width,height);
}

void SpiderPauseDim_Hide(void) {
  HWND panel=dim_panel_hwnd;
  HWND window=dim_hwnd;
  dim_panel_hwnd=NULL;
  dim_hwnd=NULL;
  if(panel && IsWindow(panel))DestroyWindow(panel);
  if(window && IsWindow(window))DestroyWindow(window);
  if((panel || window) && dim_log)
    dim_log("FOCUS-DIM: center panel and shade destroyed before pause release");
}
