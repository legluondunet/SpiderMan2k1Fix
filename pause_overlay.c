/* Passive, opt-out pause label for Windows 11.
 *
 * The existing borderless DirectDraw window owns this overlay as a child:
 * it cannot float above other desktop windows, owns no activation, and is
 * destroyed automatically with the game window. No DirectDraw surface,
 * D3D7, Bink, DirectSound, DirectInput or game EXE modifications.
 *
 * It appears only after the focus pause is latched, is painted with GDI
 * while the pause loop pumps messages, and disappears before audio resumes.
 * A layered child window with a color key makes the entire interior truly
 * transparent, leaving only a red outline and white text over the game.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pause_overlay.h"

#define OVERLAY_CLASS L"SpiderMan2k1Fix_PauseOverlay"
/* This unique color is used ONLY for the invisible background pixels.
 * Layered child windows and LWA_COLORKEY are supported on Windows 8+.
 * The game continues to show through the border's unfilled interior. */
#define OVERLAY_TRANSPARENT RGB(1,2,3)
#define OVERLAY_RED RGB(220,38,46)

static HWND overlay_hwnd=NULL;
static int overlay_enabled=1;
static int overlay_resume_key=0;
static int overlay_registered=0;
static void (*overlay_log)(const char *)=NULL;

static void overlay_paint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC dc=BeginPaint(hwnd,&ps);
  RECT client,title_rect,hint_rect;
  HBRUSH transparent_brush;
  HPEN border;
  HGDIOBJ old_pen,old_brush,old_font;
  HFONT title_font,hint_font;
  const wchar_t *hint=L"Appuyez sur une touche";

  if(!dc)return;
  GetClientRect(hwnd,&client);
  if(overlay_resume_key==VK_SPACE)hint=L"Appuyez sur Espace";
  else if(overlay_resume_key==VK_F9)hint=L"Appuyez sur F9";

  /* Fill the color-key pixels, then draw ONLY the outline and white text.
   * Never paint an opaque background rectangle over the game. */
  transparent_brush=CreateSolidBrush(OVERLAY_TRANSPARENT);
  if(transparent_brush) {
    FillRect(dc,&client,transparent_brush);
    DeleteObject(transparent_brush);
  }
  border=CreatePen(PS_SOLID,3,OVERLAY_RED);
  if(border) {
    old_pen=SelectObject(dc,border);
    old_brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc,2,2,client.right-2,client.bottom-2,14,14);
    SelectObject(dc,old_brush);
    SelectObject(dc,old_pen);
    DeleteObject(border);
  }

  SetBkMode(dc,TRANSPARENT);
  title_font=CreateFontW(-28,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,
                          DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  hint_font=CreateFontW(-19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,
                         DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  title_rect=client;title_rect.left+=12;title_rect.right-=12;
  title_rect.top=12;title_rect.bottom=55;
  old_font=NULL;
  if(title_font)old_font=SelectObject(dc,title_font);
  SetTextColor(dc,RGB(255,255,255));
  DrawTextW(dc,L"PAUSE",-1,&title_rect,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
  if(old_font)SelectObject(dc,old_font);
  if(title_font)DeleteObject(title_font);

  hint_rect=client;hint_rect.left+=12;hint_rect.right-=12;
  hint_rect.top=56;hint_rect.bottom=client.bottom-9;
  old_font=NULL;
  if(hint_font)old_font=SelectObject(dc,hint_font);
  SetTextColor(dc,RGB(255,255,255));
  DrawTextW(dc,hint,-1,&hint_rect,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
  if(old_font)SelectObject(dc,old_font);
  if(hint_font)DeleteObject(hint_font);
  EndPaint(hwnd,&ps);
}

static LRESULT CALLBACK overlay_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
  switch(message) {
    case WM_ERASEBKGND:
      return 1; /* Full repaint in WM_PAINT, avoiding flicker. */
    case WM_PAINT:
      overlay_paint(hwnd);
      return 0;
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE; /* Never steal keyboard focus from the game/desktop. */
    case WM_NCHITTEST:
      return HTTRANSPARENT; /* Don't block parent-window mouse input. */
    case WM_NCDESTROY:
      if(overlay_hwnd==hwnd)overlay_hwnd=NULL;
      break;
  }
  return DefWindowProcW(hwnd,message,wparam,lparam);
}

void SpiderPauseOverlay_Init(void (*log_fn)(const char *)) {
  char path[MAX_PATH],*slash;
  DWORD n;
  overlay_log=log_fn;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(!n || n>=sizeof(path) || !(slash=strrchr(path,'\\')) ||
     (size_t)(slash-path)+sizeof("SpiderMan2k1Fix.ini")>sizeof(path))return;
  strcpy(slash+1,"SpiderMan2k1Fix.ini");
  overlay_enabled=GetPrivateProfileIntA("Input","PauseOverlay",1,path)==1;
  if(overlay_log)overlay_log(overlay_enabled ?
    "FOCUS-OVERLAY: transparent centered pause label enabled (no activation)" :
    "FOCUS-OVERLAY: pause label disabled by INI");
}

void SpiderPauseOverlay_Show(HWND game_window,int resume_key) {
  WNDCLASSW cls;
  RECT bounds;
  HINSTANCE instance;
  int width,height,banner_width,banner_height,x,y;
  HWND window;

  if(!overlay_enabled || !game_window || !IsWindow(game_window))return;
  if(overlay_hwnd && IsWindow(overlay_hwnd))return;
  overlay_hwnd=NULL;

  instance=GetModuleHandleW(NULL);
  if(!overlay_registered) {
    memset(&cls,0,sizeof(cls));
    cls.lpfnWndProc=overlay_proc;
    cls.hInstance=instance;
    cls.lpszClassName=OVERLAY_CLASS;
    cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {
      if(overlay_log)overlay_log("FOCUS-OVERLAY: class registration failed; continuing without label");
      return;
    }
    overlay_registered=1;
  }

  if(!GetClientRect(game_window,&bounds))return;
  width=bounds.right-bounds.left;
  height=bounds.bottom-bounds.top;
  if(width<250 || height<120)return;
  banner_width=420;
  if(banner_width>width-24)banner_width=width-24;
  banner_height=104;
  if(banner_height>height-16)banner_height=height-16;
  x=(width-banner_width)/2;
  y=(height-banner_height)/2;
  overlay_resume_key=resume_key;
  /* Create hidden so there is no momentary opaque flash. Color-key all
   * interior pixels before showing the layered, nonactivating child. */
  window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_NOPARENTNOTIFY|WS_EX_LAYERED,
       OVERLAY_CLASS,L"",WS_CHILD|WS_CLIPSIBLINGS,
       x,y,banner_width,banner_height,
       game_window,NULL,instance,NULL);
  if(!window){
    if(overlay_log)overlay_log("FOCUS-OVERLAY: creation failed; continuing focus pause");
    return;
  }
  if(!SetLayeredWindowAttributes(window,OVERLAY_TRANSPARENT,0,LWA_COLORKEY)) {
    if(overlay_log)overlay_log("FOCUS-OVERLAY: transparent color key unavailable; skipping overlay");
    DestroyWindow(window);
    return;
  }
  ShowWindow(window,SW_SHOWNOACTIVATE);
  overlay_hwnd=window;
  /* Ensure text is painted even while the game thread waits in Flip. */
  InvalidateRect(window,NULL,FALSE);
  UpdateWindow(window);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: transparent red-outline banner centered on paused game window");
}

void SpiderPauseOverlay_Hide(void) {
  HWND window=overlay_hwnd;
  if(!window)return;
  overlay_hwnd=NULL;
  if(IsWindow(window))DestroyWindow(window);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: hidden before focus-pause release");
}
