/* Passive uniform-alpha shade for the verified Spider-Man 2001 game.
 *
 * One black, uniformly alpha-blended WS_CHILD layer darkens the paused
 * frame. It does not intercept keyboard/mouse input, take activation,
 * modify DirectDraw surfaces, suspend threads or float above other apps.
 * The existing shaped red PAUSE banner is created AFTER this child and
 * thus stays fully bright and above the shade.
 *
 * Windows 8+ supports layered child windows; this Windows 11 feature
 * is opt-out and independently disabled with [Input] PauseDim=0.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pause_dim.h"

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
  HWND window;
  int width,height;

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
      if(dim_log)dim_log("FOCUS-DIM: class registration failed; skipping dim layer");
      return;
    }
    dim_registered=1;
  }
  if(!GetClientRect(game_window,&bounds))return;
  width=bounds.right-bounds.left;
  height=bounds.bottom-bounds.top;
  if(width<250 || height<120)return;

  /* Create HIDDEN: alpha must be installed successfully BEFORE the child is
   * shown. Failure is fail-safe: never present an opaque black rectangle. */
  window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|
       WS_EX_NOACTIVATE|WS_EX_NOPARENTNOTIFY,
       DIM_CLASS,L"",WS_CHILD|WS_CLIPSIBLINGS,
       0,0,width,height,game_window,NULL,instance,NULL);
  if(!window) {
    if(dim_log)dim_log("FOCUS-DIM: creation failed; continuing pause without shade");
    return;
  }
  if(!SetLayeredWindowAttributes(window,0,DIM_ALPHA,LWA_ALPHA)) {
    if(dim_log)dim_log("FOCUS-DIM: alpha unavailable; skipping opaque layer");
    DestroyWindow(window);
    return;
  }
  dim_hwnd=window;
  InvalidateRect(window,NULL,FALSE);
  UpdateWindow(window);
  ShowWindow(window,SW_SHOWNOACTIVATE);
  if(dim_log)dim_log("FOCUS-DIM: alpha 102/255 shade visible underneath PAUSE banner");
}

void SpiderPauseDim_Hide(void) {
  HWND window=dim_hwnd;
  if(!window)return;
  dim_hwnd=NULL;
  if(IsWindow(window))DestroyWindow(window);
  if(dim_log)dim_log("FOCUS-DIM: shade destroyed before pause release");
}
