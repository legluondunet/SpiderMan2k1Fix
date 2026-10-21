/* Spider-Man 2001 nonactivating focus-pause indicator.
 *
 * Draw only the comic-style text and a thin red outline in a shaped child
 * window. SetWindowRgn physically EXCLUDES every interior/background pixel;
 * unlike a layered child with LWA_COLORKEY, the underlying DirectDraw
 * picture remains visible even on display drivers which compose a black
 * rectangle under a color-keyed window.
 *
 * No game-rendering hooks or changes to Bink, DirectSound or DirectInput.
 * The pause indicator is destroyed before normal gameplay resumes.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pause_overlay.h"

#define OVERLAY_CLASS L"SpiderMan2k1Fix_PauseOverlay"
#define OVERLAY_RED RGB(221,34,45)
#define OVERLAY_WHITE RGB(255,255,255)
#define OVERLAY_DARK RGB(18,16,28)
#define OVERLAY_WIDTH 440
#define OVERLAY_HEIGHT 112

static HWND overlay_hwnd=NULL;
static int overlay_enabled=1;
static int overlay_resume_key=0;
static int overlay_registered=0;
static int overlay_title_x=0,overlay_title_y=0;
static int overlay_hint_x=0,overlay_hint_y=0;
static HFONT overlay_title_font=NULL,overlay_hint_font=NULL;
static void (*overlay_log)(const char *)=NULL;

static const wchar_t *overlay_hint(void) {
  if(overlay_resume_key==VK_SPACE)return L"APPUYEZ SUR ESPACE";
  if(overlay_resume_key==VK_F9)return L"APPUYEZ SUR F9";
  return L"APPUYEZ SUR UNE TOUCHE";
}

/* TrueType glyph outlines can be turned into GDI window regions using
 * BeginPath/TextOut/EndPath/PathToRegion. This allows an unfilled window
 * with ONLY opaque glyph strokes and the border, without color-keying. */
static int overlay_add_text_region(HDC dc,HRGN visible,HFONT font,
                                   const wchar_t *string,int x,int y) {
  HRGN letters,shifted;
  HGDIOBJ old;
  int dx,dy,ok=1;
  if(!font)return 0;
  old=SelectObject(dc,font);
  if(!BeginPath(dc)) {SelectObject(dc,old);return 0;}
  if(!TextOutW(dc,x,y,string,lstrlenW(string)) || !EndPath(dc)) {
    AbortPath(dc);
    SelectObject(dc,old);
    return 0;
  }
  letters=PathToRegion(dc);
  SelectObject(dc,old);
  if(!letters)return 0;
  shifted=CreateRectRgn(0,0,0,0);
  if(!shifted){DeleteObject(letters);return 0;}
  /* Expand visible region to also contain the dark comic-book text outline. */
  for(dy=-2;dy<=2 && ok;dy++)for(dx=-2;dx<=2;dx++) {
    if(dx*dx+dy*dy>5)continue;
    if(CombineRgn(shifted,letters,NULL,RGN_COPY)==ERROR ||
       OffsetRgn(shifted,dx,dy)==ERROR ||
       CombineRgn(visible,visible,shifted,RGN_OR)==ERROR) {
      ok=0;break;
    }
  }
  DeleteObject(shifted);
  DeleteObject(letters);
  return ok;
}

static void overlay_text_pos(HDC dc,HFONT font,const wchar_t *string,
                             int width,int y,int *x_result,int *y_result) {
  SIZE size={0,0};
  HGDIOBJ old=SelectObject(dc,font);
  if(!GetTextExtentPoint32W(dc,string,lstrlenW(string),&size))size.cx=0;
  SelectObject(dc,old);
  *x_result=(width-size.cx)/2;
  if(*x_result<8)*x_result=8;
  *y_result=y;
}

static int overlay_apply_region(HWND window,int width,int height) {
  HDC dc;
  HRGN outline=NULL,inside=NULL;
  int ok=0;
  if(!overlay_title_font || !overlay_hint_font)return 0;
  dc=GetDC(window);
  if(!dc)return 0;
  SetBkMode(dc,TRANSPARENT);
  SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
  overlay_text_pos(dc,overlay_title_font,L"PAUSE",width,14,
                   &overlay_title_x,&overlay_title_y);
  overlay_text_pos(dc,overlay_hint_font,overlay_hint(),width,68,
                   &overlay_hint_x,&overlay_hint_y);

  outline=CreateRoundRectRgn(0,0,width,height,16,16);
  inside=CreateRoundRectRgn(3,3,width-3,height-3,13,13);
  if(outline && inside &&
     CombineRgn(outline,outline,inside,RGN_DIFF)!=ERROR &&
     overlay_add_text_region(dc,outline,overlay_title_font,L"PAUSE",
                             overlay_title_x,overlay_title_y) &&
     overlay_add_text_region(dc,outline,overlay_hint_font,overlay_hint(),
                             overlay_hint_x,overlay_hint_y)) {
    /* SetWindowRgn takes ownership of the region on success. If it fails,
     * never show a rectangular child which might obscure the game. */
    if(SetWindowRgn(window,outline,FALSE)) {
      outline=NULL;
      ok=1;
    }
  }
  if(inside)DeleteObject(inside);
  if(outline)DeleteObject(outline);
  ReleaseDC(window,dc);
  return ok;
}

static void overlay_draw_text(HDC dc,HFONT font,const wchar_t *string,int x,int y) {
  HGDIOBJ old=SelectObject(dc,font);
  int dx,dy;
  SetTextColor(dc,OVERLAY_DARK);
  for(dy=-2;dy<=2;dy++)for(dx=-2;dx<=2;dx++) {
    if((dx==0 && dy==0) || dx*dx+dy*dy>5)continue;
    TextOutW(dc,x+dx,y+dy,string,lstrlenW(string));
  }
  SetTextColor(dc,OVERLAY_WHITE);
  TextOutW(dc,x,y,string,lstrlenW(string));
  SelectObject(dc,old);
}

static void overlay_paint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC dc=BeginPaint(hwnd,&ps);
  RECT client;
  HPEN pen;
  HGDIOBJ old_pen,old_brush;
  if(!dc)return;
  GetClientRect(hwnd,&client);
  SetBkMode(dc,TRANSPARENT);
  SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
  pen=CreatePen(PS_SOLID,3,OVERLAY_RED);
  if(pen) {
    old_pen=SelectObject(dc,pen);
    old_brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc,0,0,client.right,client.bottom,16,16);
    SelectObject(dc,old_brush);
    SelectObject(dc,old_pen);
    DeleteObject(pen);
  }
  if(overlay_title_font)
    overlay_draw_text(dc,overlay_title_font,L"PAUSE",overlay_title_x,overlay_title_y);
  if(overlay_hint_font)
    overlay_draw_text(dc,overlay_hint_font,overlay_hint(),overlay_hint_x,overlay_hint_y);
  EndPaint(hwnd,&ps);
}

static void overlay_release_fonts(void) {
  if(overlay_title_font) {DeleteObject(overlay_title_font);overlay_title_font=NULL;}
  if(overlay_hint_font) {DeleteObject(overlay_hint_font);overlay_hint_font=NULL;}
}

static LRESULT CALLBACK overlay_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
  switch(message) {
    case WM_ERASEBKGND:
      return 1; /* The region has no background to erase. */
    case WM_PAINT:
      overlay_paint(hwnd);
      return 0;
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_NCHITTEST:
      return HTTRANSPARENT; /* Clicks pass to the actual game window. */
    case WM_NCDESTROY:
      if(overlay_hwnd==hwnd)overlay_hwnd=NULL;
      overlay_release_fonts();
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
    "FOCUS-OVERLAY: centered outlined comic-style hint enabled (unfilled window region)" :
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
  banner_width=OVERLAY_WIDTH;
  if(banner_width>width-24)banner_width=width-24;
  banner_height=OVERLAY_HEIGHT;
  if(banner_height>height-16)banner_height=height-16;
  x=(width-banner_width)/2;
  y=(height-banner_height)/2;
  overlay_resume_key=resume_key;

  /* The game uses comic-style italic block letters. Arial Black is a
   * system TrueType font, so it also supports the precise glyph-region
   * cutout; do not ship or load third-party font files. */
  overlay_title_font=CreateFontW(-31,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,
                                 DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  overlay_hint_font=CreateFontW(-19,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,
                                DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  if(!overlay_title_font || !overlay_hint_font) {
    overlay_release_fonts();
    if(overlay_log)overlay_log("FOCUS-OVERLAY: comic fonts unavailable; skipping indicator");
    return;
  }

  window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_NOPARENTNOTIFY,
       OVERLAY_CLASS,L"",WS_CHILD|WS_CLIPSIBLINGS,
       x,y,banner_width,banner_height,
       game_window,NULL,instance,NULL);
  if(!window) {
    overlay_release_fonts();
    if(overlay_log)overlay_log("FOCUS-OVERLAY: creation failed; continuing focus pause");
    return;
  }
  if(!overlay_apply_region(window,banner_width,banner_height)) {
    if(overlay_log)overlay_log("FOCUS-OVERLAY: window region unavailable; skipping opaque overlay");
    DestroyWindow(window);
    return;
  }
  overlay_hwnd=window;
  ShowWindow(window,SW_SHOWNOACTIVATE);
  InvalidateRect(window,NULL,FALSE);
  UpdateWindow(window);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: outlined comic-style PAUSE centered; empty area cut out");
}

void SpiderPauseOverlay_Hide(void) {
  HWND window=overlay_hwnd;
  if(!window)return;
  overlay_hwnd=NULL;
  if(IsWindow(window))DestroyWindow(window);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: hidden before focus-pause release");
}
