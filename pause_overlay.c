/* Passive, opt-out pause label for Windows 11.
 *
 * The existing borderless DirectDraw window owns this overlay as a child:
 * it cannot float above other desktop windows, owns no activation, and is
 * destroyed automatically with the game window. No DirectDraw surface,
 * D3D7, Bink, DirectSound, DirectInput or game EXE modifications.
 *
 * It appears only after the focus pause is latched, is painted with GDI
 * while the pause loop pumps messages, and disappears before audio resumes.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pause_overlay.h"

#define OVERLAY_CLASS L"SpiderMan2k1Fix_PauseOverlay"

static HWND overlay_hwnd=NULL;
static int overlay_enabled=1;
static int overlay_resume_key=0;
static int overlay_registered=0;
static void (*overlay_log)(const char *)=NULL;

static void overlay_paint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC dc=BeginPaint(hwnd,&ps);
  RECT client,title_rect,hint_rect;
  HBRUSH background;
  HPEN border;
  HGDIOBJ old_pen,old_brush,old_font;
  HFONT title_font,hint_font;
  const wchar_t *hint=L"Appuyez sur une touche";

  if(!dc)return;
  GetClientRect(hwnd,&client);
  if(overlay_resume_key==VK_SPACE)hint=L"Appuyez sur Espace";
  else if(overlay_resume_key==VK_F9)hint=L"Appuyez sur F9";

  background=CreateSolidBrush(RGB(22,25,32));
  border=CreatePen(PS_SOLID,2,RGB(214,177,83));
  if(background && border) {
    old_brush=SelectObject(dc,background);
    old_pen=SelectObject(dc,border);
    RoundRect(dc,0,0,client.right,client.bottom,16,16);
    SelectObject(dc,old_pen);
    SelectObject(dc,old_brush);
  } else FillRect(dc,&client,(HBRUSH)(COLOR_WINDOW+1));
  if(border)DeleteObject(border);
  if(background)DeleteObject(background);

  SetBkMode(dc,TRANSPARENT);
  title_font=CreateFontW(-25,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                          DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  hint_font=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                         DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  title_rect=client;title_rect.left+=10;title_rect.right-=10;title_rect.top=9;title_rect.bottom=43;
  old_font=NULL;
  if(title_font)old_font=SelectObject(dc,title_font);
  SetTextColor(dc,RGB(245,208,109));
  DrawTextW(dc,L"PAUSE",-1,&title_rect,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
  if(old_font)SelectObject(dc,old_font);
  if(title_font)DeleteObject(title_font);

  hint_rect=client;hint_rect.left+=8;hint_rect.right-=8;
  hint_rect.top=43;hint_rect.bottom-=7;
  old_font=NULL;
  if(hint_font)old_font=SelectObject(dc,hint_font);
  SetTextColor(dc,RGB(246,246,246));
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
    "FOCUS-OVERLAY: passive pause label enabled (right side; no activation)" :
    "FOCUS-OVERLAY: pause label disabled by INI");
}

void SpiderPauseOverlay_Show(HWND game_window,int resume_key) {
  WNDCLASSW cls;
  RECT bounds;
  HINSTANCE instance;
  int width,height,margin,left,top;
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
  margin=width/64;
  if(margin<12)margin=12;
  if(margin>30)margin=30;
  left=width*30/100;
  if(left<220)left=220;
  if(left>340)left=340;
  if(left>width-margin*2)left=width-margin*2;
  top=(height-84)/2;
  if(top<0)top=0;
  overlay_resume_key=resume_key;
  window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_NOPARENTNOTIFY,
       OVERLAY_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
       width-left-margin,top,left,84,
       game_window,NULL,instance,NULL);
  if(!window){
    if(overlay_log)overlay_log("FOCUS-OVERLAY: creation failed; continuing focus pause");
    return;
  }
  overlay_hwnd=window;
  /* Ensure text is painted even while the game thread waits in Flip. */
  InvalidateRect(window,NULL,FALSE);
  UpdateWindow(window);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: visible on right of paused game window");
}

void SpiderPauseOverlay_Hide(void) {
  HWND window=overlay_hwnd;
  if(!window)return;
  overlay_hwnd=NULL;
  if(IsWindow(window))DestroyWindow(window);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: hidden before focus-pause release");
}
