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
#define DIM_ALPHA 102 /* 40% opaque black; underlying scene retains 60% light. */

static HWND dim_hwnd=NULL;
static int dim_enabled=1;
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
      break;
  }
  return DefWindowProcW(window,message,wparam,lparam);
}

void SpiderPauseDim_Init(void (*log_fn)(const char *)) {
  char path[MAX_PATH],*slash;
  DWORD n;
  dim_log=log_fn;
  dim_enabled=1;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(n && n<sizeof(path) && (slash=strrchr(path,'\\'))!=NULL &&
     (size_t)(slash-path)+sizeof("SpiderMan2k1Fix.ini")<=sizeof(path)) {
    strcpy(slash+1,"SpiderMan2k1Fix.ini");
    dim_enabled=GetPrivateProfileIntA("Input","PauseDim",1,path)==1;
  }
  if(dim_log)dim_log(dim_enabled ?
      "FOCUS-DIM: uniform-alpha shade enabled (40% black; only during focus pause)" :
      "FOCUS-DIM: disabled via [Input] PauseDim=0");
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

  if(!dim_enabled || !game_window || !IsWindow(game_window))return;
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
  if(!SetLayeredWindowAttributes(window,0,DIM_ALPHA,LWA_ALPHA)) {
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
  if(dim_log)dim_log("FOCUS-DIM: owned popup alpha 102/255, center message cut out, shade shown without activation");
}

void SpiderPauseDim_Hide(void) {
  HWND window=dim_hwnd;
  if(!window)return;
  dim_hwnd=NULL;
  if(IsWindow(window))DestroyWindow(window);
  if(dim_log)dim_log("FOCUS-DIM: shade destroyed before pause release");
}
