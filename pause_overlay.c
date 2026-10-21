/* Spider-Man 2001 nonactivating focus-pause indicator.
 *
 * Draw only the comic-style text and a thin red outline in a shaped child
 * window. SetWindowRgn physically EXCLUDES every interior/background pixel;
 * unlike a color-keyed layered child, the underlying DirectDraw
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
#define OVERLAY_WIDTH 520
#define OVERLAY_HEIGHT 136

static HWND overlay_hwnd=NULL;
static int overlay_enabled=1;
static int overlay_registered=0;
static int overlay_title_x=0,overlay_title_y=0;
static int overlay_hint_x=0,overlay_hint_y=0;
static HFONT overlay_title_font=NULL,overlay_hint_font=NULL;
static void (*overlay_log)(const char *)=NULL;

static const wchar_t *overlay_hint(void) {
  return L"PRESS ANY KEY TO RESUME";
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
  /* Oversize each glyph's region so neither alpha popup can cross the white
   * pixels (Wine/GDI text rasterization may differ slightly from PathToRegion).
   * This extra guard prevents the grid/dimmer from eating letter strokes. */
  for(dy=-5;dy<=5 && ok;dy++)for(dx=-5;dx<=5;dx++) {
    if(dx*dx+dy*dy>26)continue;
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
  overlay_text_pos(dc,overlay_title_font,L"PAUSE",width,17,
                   &overlay_title_x,&overlay_title_y);
  overlay_text_pos(dc,overlay_hint_font,overlay_hint(),width,78,
                   &overlay_hint_x,&overlay_hint_y);

  outline=CreateRoundRectRgn(0,0,width,height,16,16);
  inside=CreateRoundRectRgn(4,4,width-4,height-4,12,12);
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
  for(dy=-3;dy<=3;dy++)for(dx=-3;dx<=3;dx++) {
    if((dx==0 && dy==0) || dx*dx+dy*dy>10)continue;
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
  pen=CreatePen(PS_SOLID,4,OVERLAY_RED);
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
    "FOCUS-OVERLAY: English bold any-key banner, drawn only when game is foreground" :
    "FOCUS-OVERLAY: pause label disabled by INI");
}

void SpiderPauseOverlay_Show(HWND game_window) {
  WNDCLASSW cls;
  RECT bounds;
  HINSTANCE instance;
  int width,height,banner_width,banner_height,x,y;
  HWND window;

  if(!overlay_enabled || !game_window || !IsWindow(game_window) ||
     GetForegroundWindow()!=game_window)return;
  if(overlay_hwnd && IsWindow(overlay_hwnd))return;
  overlay_hwnd=NULL;
  instance=GetModuleHandleW(NULL);

  if(!overlay_registered) {
    memset(&cls,0,sizeof(cls));
    cls.lpfnWndProc=overlay_proc;
    cls.hInstance=instance;
    cls.lpszClassName=OVERLAY_CLASS;
    cls.hCursor=LoadCursorA(NULL,IDC_ARROW);
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

  /* The game uses comic-style italic block letters. Arial Black is a
   * system TrueType font, so it also supports the precise glyph-region
   * cutout; do not ship or load third-party font files. */
  overlay_title_font=CreateFontW(-31,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,
                                 DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  overlay_hint_font=CreateFontW(-25,0,0,0,FW_HEAVY,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
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
  if(overlay_log)overlay_log("FOCUS-OVERLAY: foreground-only English PAUSE; solid bold wake text with expanded glyph exclusion");
}

void SpiderPauseOverlay_Hide(void) {
  HWND window=overlay_hwnd;
  if(!window)return;
  overlay_hwnd=NULL;
  if(IsWindow(window))DestroyWindow(window);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: hidden before focus-pause release");
}


int SpiderPauseOverlay_ExcludeFromRegion(HWND game_window,HRGN visible_region) {
  HRGN banner_region;
  POINT origin={0,0};
  int ok=0;
  if(!visible_region || !game_window || !IsWindow(game_window))return 0;
  /* PauseOverlay=0 is legitimate: in that case, dim the whole frame. */
  if(!overlay_hwnd || !IsWindow(overlay_hwnd))return 1;
  if(GetParent(overlay_hwnd)!=game_window)return 0;
  banner_region=CreateRectRgn(0,0,0,0);
  if(!banner_region)return 0;
  if(GetWindowRgn(overlay_hwnd,banner_region)!=ERROR) {
    /* The shaped child is centered in parent CLIENT coordinates. */
    MapWindowPoints(overlay_hwnd,game_window,&origin,1);
    if(OffsetRgn(banner_region,origin.x,origin.y)!=ERROR &&
       CombineRgn(visible_region,visible_region,banner_region,RGN_DIFF)!=ERROR)
      ok=1;
  }
  DeleteObject(banner_region);
  return ok;
}

/* Returns the centered banner dimensions in game CLIENT pixels. The dimmer
 * can create a separately-alpha-blended dark panel underneath it while
 * cutting out the red frame and white letters exactly. */
int SpiderPauseOverlay_GetBounds(HWND game_window,RECT *bounds) {
  RECT game;
  if(!bounds || !game_window || !IsWindow(game_window))return 0;
  if(!overlay_hwnd || !IsWindow(overlay_hwnd) ||
     GetParent(overlay_hwnd)!=game_window)return 0;
  if(!GetWindowRect(overlay_hwnd,bounds) ||
     !GetClientRect(game_window,&game))return 0;
  {
    POINT pts[2]={{bounds->left,bounds->top},{bounds->right,bounds->bottom}};
    MapWindowPoints(HWND_DESKTOP,game_window,pts,2);
    bounds->left=pts[0].x;bounds->top=pts[0].y;
    bounds->right=pts[1].x;bounds->bottom=pts[1].y;
  }
  return bounds->left>=game.left && bounds->top>=game.top &&
         bounds->right<=game.right && bounds->bottom<=game.bottom;
}
