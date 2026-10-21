/* Spider-Man 2001: one-piece game-owned focus-pause banner.
 *
 * Avoid shaped child HWND + alpha popups: under Wine, individual glyph
 * region masks leave dark fragments and can paint stale pixels into the
 * paused DirectDraw frame. Instead compose background, border and full-white
 * lettering into ONE premultiplied 32bpp bitmap and send it to an owned
 * nonactivating popup with UpdateLayeredWindow(ULW_ALPHA).
 *
 * Foreground: retain the proven per-pixel-alpha popup.
 * Background: show a conventional opaque WS_CHILD, clipped to the parent
 * game client. Child z-order follows the game, so it cannot float ABOVE a
 * foreground Wine/Linux desktop app. It cannot use layered-child alpha
 * because the original 2001 game lacks a Windows 8 manifest.
 * Original SpideyPC.exe, DirectDraw, Bink, DirectSound and input untouched.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pause_overlay.h"

#define OVERLAY_CLASS L"SpiderMan2k1Fix_ComicPause"
#define OVERLAY_WIDTH 470
#define OVERLAY_HEIGHT 108
#define PANEL_R 18
#define PANEL_G 14
#define PANEL_B 29
#define PANEL_ALPHA 188 /* subdued comic banner; background still visible */
#define PANEL_CORNER 10

static HWND overlay_hwnd=NULL;
static HWND overlay_game=NULL;
static int overlay_enabled=1,overlay_registered=0;
static void (*overlay_log)(const char *)=NULL;

int SpiderPauseOverlay_GameForeground(HWND game_window) {
  if(!game_window || !IsWindow(game_window) ||
     !IsWindowVisible(game_window) || IsIconic(game_window))return 0;
  /* Checking GetActiveWindow as well as the Windows foreground owner
   * prevents stale Wine foreground state from leaving a popup over a
   * native Linux application. No synthetic activation is ever issued. */
  return GetForegroundWindow()==game_window &&
         GetActiveWindow()==game_window;
}

static void child_draw_text(HDC dc,HFONT font,const wchar_t *value,int width,int y) {
  SIZE sz={0,0};
  HGDIOBJ previous;
  int x,dx,dy;
  if(!font)return;
  previous=SelectObject(dc,font);
  if(GetTextExtentPoint32W(dc,value,lstrlenW(value),&sz)) {
    x=(width-sz.cx)/2;
    if(x<12)x=12;
    SetTextColor(dc,RGB(8,7,16));
    for(dy=-1;dy<=1;dy++)for(dx=-1;dx<=1;dx++)
      if(dx||dy)TextOutW(dc,x+dx,y+dy,value,lstrlenW(value));
    SetTextColor(dc,RGB(255,255,255));
    TextOutW(dc,x,y,value,lstrlenW(value));
  }
  SelectObject(dc,previous);
}

static void child_paint(HWND window) {
  PAINTSTRUCT ps;
  HDC dc=BeginPaint(window,&ps);
  HFONT title=NULL,hint=NULL;
  HPEN pen=NULL;
  HBRUSH fill=NULL;
  HGDIOBJ previous_pen=NULL,previous_fill=NULL;
  RECT rect;
  if(!dc)return;
  GetClientRect(window,&rect);
  SetBkMode(dc,TRANSPARENT);
  SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
  fill=CreateSolidBrush(RGB(PANEL_R,PANEL_G,PANEL_B));
  pen=CreatePen(PS_SOLID,2,RGB(174,47,64));
  if(fill && pen) {
    previous_fill=SelectObject(dc,fill);
    previous_pen=SelectObject(dc,pen);
    RoundRect(dc,1,1,rect.right-1,rect.bottom-1,20,20);
    SelectObject(dc,previous_pen);
    SelectObject(dc,previous_fill);
  } else if(fill) FillRect(dc,&rect,fill);
  title=CreateFontW(-30,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,
                    DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                    NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  hint=CreateFontW(-19,0,0,0,FW_HEAVY,FALSE,FALSE,FALSE,
                   DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                   NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  child_draw_text(dc,title,L"PAUSE",rect.right,13);
  child_draw_text(dc,hint,L"PRESS ANY KEY TO RESUME",rect.right,66);
  if(title)DeleteObject(title);
  if(hint)DeleteObject(hint);
  if(pen)DeleteObject(pen);
  if(fill)DeleteObject(fill);
  EndPaint(window,&ps);
}

static LRESULT CALLBACK overlay_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
  switch(msg) {
    case WM_PAINT:
      if(GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD) {
        child_paint(window);
        return 0;
      }
      break;
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_NCHITTEST:return HTTRANSPARENT;
    case WM_ERASEBKGND:return 1;
    case WM_NCDESTROY:
      if(overlay_hwnd==window) {overlay_hwnd=NULL;overlay_game=NULL;}
      break;
  }
  return DefWindowProcW(window,msg,wp,lp);
}

static int overlay_ensure_class(HINSTANCE instance) {
  WNDCLASSW cls;
  if(overlay_registered)return 1;
  memset(&cls,0,sizeof(cls));
  cls.lpfnWndProc=overlay_proc;
  cls.hInstance=instance;
  cls.lpszClassName=OVERLAY_CLASS;
  if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {
    if(overlay_log)overlay_log("FOCUS-OVERLAY: window class registration failed");
    return 0;
  }
  overlay_registered=1;
  return 1;
}

static void text_at(HDC dc,HFONT font,const wchar_t *value,int width,int y) {
  SIZE size={0,0};
  HGDIOBJ previous=SelectObject(dc,font);
  int x,dy,dx;
  if(!GetTextExtentPoint32W(dc,value,lstrlenW(value),&size)) {
    SelectObject(dc,previous);return;
  }
  x=(width-size.cx)/2;
  if(x<12)x=12;
  /* A single dark outline—not wide multi-pass raster masks. GDI's text
   * and its shadow belong to the same ULW bitmap as the opaque white face. */
  SetTextColor(dc,RGB(8,7,16));
  for(dy=-1;dy<=1;dy++)for(dx=-1;dx<=1;dx++)
    if(dx || dy)TextOutW(dc,x+dx,y+dy,value,lstrlenW(value));
  SetTextColor(dc,RGB(255,255,255));
  TextOutW(dc,x,y,value,lstrlenW(value));
  SelectObject(dc,previous);
}

/* The 32bpp GDI DIB has B,G,R and an alpha byte. Since traditional GDI
 * doesn't set alpha, choose alpha explicitly after drawing. Only the dark
 * interior is translucent; the red border and glyph strokes are 100%
 * opaque. Premultiply every semi-transparent BGRA channel for ULW_ALPHA. */
static int render_banner(HWND window,POINT origin,int width,int height) {
  HDC screen=NULL,dc=NULL;
  HBITMAP bitmap=NULL,old_bitmap=NULL;
  HFONT title=NULL,hint=NULL;
  HBRUSH panel_brush=NULL;
  HPEN border=NULL;
  HGDIOBJ old_pen=NULL,old_brush=NULL;
  BITMAPINFO info;
  DWORD *pixels=NULL;
  POINT src={0,0};
  SIZE size;
  BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
  int x,y,ok=0;
  const DWORD base=((DWORD)PANEL_R<<16)|((DWORD)PANEL_G<<8)|PANEL_B;

  memset(&info,0,sizeof(info));
  info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth=width;
  info.bmiHeader.biHeight=-height; /* top-down, easy alpha scan */
  info.bmiHeader.biPlanes=1;
  info.bmiHeader.biBitCount=32;
  info.bmiHeader.biCompression=BI_RGB;

  screen=GetDC(NULL);
  if(!screen)goto done;
  dc=CreateCompatibleDC(screen);
  if(!dc)goto done;
  bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
  if(!bitmap || !pixels)goto done;
  old_bitmap=(HBITMAP)SelectObject(dc,bitmap);

  /* Paint every pixel first, then specify exact alpha per pixel below. */
  panel_brush=CreateSolidBrush(RGB(PANEL_R,PANEL_G,PANEL_B));
  if(!panel_brush)goto done;
  {
    RECT full={0,0,width,height};
    FillRect(dc,&full,panel_brush);
  }

  border=CreatePen(PS_SOLID,2,RGB(174,47,64));
  if(!border)goto done;
  old_pen=SelectObject(dc,border);
  old_brush=SelectObject(dc,panel_brush);
  RoundRect(dc,1,1,width-1,height-1,20,20);
  SelectObject(dc,old_brush);
  SelectObject(dc,old_pen);
  old_brush=NULL;old_pen=NULL;

  title=CreateFontW(-30,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,
                    DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                    NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  hint=CreateFontW(-19,0,0,0,FW_HEAVY,FALSE,FALSE,FALSE,
                   DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                   NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial Black");
  if(!title||!hint)goto done;
  SetBkMode(dc,TRANSPARENT);
  SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
  text_at(dc,title,L"PAUSE",width,13);
  text_at(dc,hint,L"PRESS ANY KEY TO RESUME",width,66);

  for(y=0;y<height;y++)for(x=0;x<width;x++) {
    DWORD p=pixels[y*width+x]&0x00FFFFFF;
    unsigned int a;
    const int left=x<PANEL_CORNER,top=y<PANEL_CORNER;
    const int right=x>=width-PANEL_CORNER,bottom=y>=height-PANEL_CORNER;
    int qx=0,qy=0;
    if(left)qx=PANEL_CORNER-x;
    else if(right)qx=x-(width-PANEL_CORNER-1);
    if(top)qy=PANEL_CORNER-y;
    else if(bottom)qy=y-(height-PANEL_CORNER-1);
    if(qx>0 && qy>0 && qx*qx+qy*qy>PANEL_CORNER*PANEL_CORNER) {
      pixels[y*width+x]=0;
      continue;
    }
    a=(p==base)?PANEL_ALPHA:255;
    if(a!=255) {
      unsigned int b=(p&255)*a/255;
      unsigned int g=((p>>8)&255)*a/255;
      unsigned int r=((p>>16)&255)*a/255;
      p=(r<<16)|(g<<8)|b;
    }
    pixels[y*width+x]=(a<<24)|p;
  }
  size.cx=width;size.cy=height;
  /* This paints panel and lettering atomically. No color-key, child HWND
   * texture leak, or separate per-glyph negative region can appear. */
  ok=UpdateLayeredWindow(window,screen,&origin,&size,dc,&src,0,&blend,ULW_ALPHA)!=0;

done:
  if(title)DeleteObject(title);
  if(hint)DeleteObject(hint);
  if(border)DeleteObject(border);
  if(panel_brush)DeleteObject(panel_brush);
  if(old_bitmap)SelectObject(dc,old_bitmap);
  if(bitmap)DeleteObject(bitmap);
  if(dc)DeleteDC(dc);
  if(screen)ReleaseDC(NULL,screen);
  return ok;
}

void SpiderPauseOverlay_Init(void (*log_fn)(const char *)) {
  char path[MAX_PATH],*slash;
  DWORD n;
  overlay_log=log_fn;
  overlay_enabled=1;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(n && n<sizeof(path) && (slash=strrchr(path,'\\')) &&
     (size_t)(slash-path)+sizeof("SpiderMan2k1Fix.ini")<=sizeof(path)) {
    strcpy(slash+1,"SpiderMan2k1Fix.ini");
    overlay_enabled=GetPrivateProfileIntA("Input","PauseOverlay",1,path)==1;
  }
  if(overlay_log)overlay_log(overlay_enabled ?
      "FOCUS-OVERLAY: foreground translucent popup and background game-child banner enabled" :
      "FOCUS-OVERLAY: pause banner disabled via INI");
}

void SpiderPauseOverlay_Show(HWND game_window) {
  HINSTANCE instance;
  RECT game_rect;
  POINT screen_origin={0,0},banner;
  int width,height;
  HWND window;
  DWORD err;
  char detail[160];
  if(!overlay_enabled || !SpiderPauseOverlay_GameForeground(game_window))return;
  if(overlay_hwnd && IsWindow(overlay_hwnd))return;
  overlay_hwnd=NULL;
  instance=GetModuleHandleW(NULL);
  if(!overlay_ensure_class(instance))return;
  if(!GetClientRect(game_window,&game_rect) ||
     !ClientToScreen(game_window,&screen_origin))return;
  width=game_rect.right-game_rect.left;
  height=game_rect.bottom-game_rect.top;
  if(width<OVERLAY_WIDTH+24 || height<OVERLAY_HEIGHT+24)return;
  banner.x=screen_origin.x+(width-OVERLAY_WIDTH)/2;
  banner.y=screen_origin.y+(height-OVERLAY_HEIGHT)/2;

  /* Hidden, non-topmost and game-owned, never activated. */
  window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|
     WS_EX_TOOLWINDOW|WS_EX_NOPARENTNOTIFY,OVERLAY_CLASS,L"",
     WS_POPUP,banner.x,banner.y,OVERLAY_WIDTH,OVERLAY_HEIGHT,
     game_window,NULL,instance,NULL);
  if(!window) {
    err=GetLastError();
    snprintf(detail,sizeof(detail),
             "FOCUS-OVERLAY: owned popup creation failed Win32=%lu",(unsigned long)err);
    if(overlay_log)overlay_log(detail);
    return;
  }
  if(!render_banner(window,banner,OVERLAY_WIDTH,OVERLAY_HEIGHT)) {
    err=GetLastError();
    snprintf(detail,sizeof(detail),
             "FOCUS-OVERLAY: ULW_ALPHA failed Win32=%lu, skipping panel",(unsigned long)err);
    if(overlay_log)overlay_log(detail);
    DestroyWindow(window);
    return;
  }
  /* Recheck after GDI upload: foreground may have changed while painting. */
  if(!SpiderPauseOverlay_GameForeground(game_window)) {
    DestroyWindow(window);
    return;
  }
  overlay_hwnd=window;
  overlay_game=game_window;
  ShowWindow(window,SW_SHOWNOACTIVATE);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: one-piece English PAUSE banner visible; letters opaque; no shaped child");
}

/* A normal Win32 child remains geometrically and Z-order confined to
 * SpideyPC. This intentionally uses an OPAQUE plum fill, NOT unsupported
 * WS_EX_LAYERED on a legacy child. It will never cover Thunar/Firefox.
 * When the game becomes active the front-facing alpha popup replaces it. */
void SpiderPauseOverlay_ShowBackground(HWND game_window) {
  HINSTANCE instance;
  RECT bounds;
  HRGN rounded;
  HWND window;
  int width,height,x,y;
  DWORD err;
  char line[152];

  if(!overlay_enabled || !game_window || !IsWindow(game_window) ||
     !IsWindowVisible(game_window) || IsIconic(game_window))return;
  if(overlay_hwnd && IsWindow(overlay_hwnd))return;
  instance=GetModuleHandleW(NULL);
  if(!overlay_ensure_class(instance))return;
  if(!GetClientRect(game_window,&bounds))return;
  width=bounds.right-bounds.left;
  height=bounds.bottom-bounds.top;
  if(width<OVERLAY_WIDTH+24 || height<OVERLAY_HEIGHT+24)return;
  x=(width-OVERLAY_WIDTH)/2;
  y=(height-OVERLAY_HEIGHT)/2;

  window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_NOPARENTNOTIFY,
     OVERLAY_CLASS,L"",WS_CHILD|WS_CLIPSIBLINGS,
     x,y,OVERLAY_WIDTH,OVERLAY_HEIGHT,game_window,NULL,instance,NULL);
  if(!window) {
    err=GetLastError();
    snprintf(line,sizeof(line),
      "FOCUS-OVERLAY: background child creation failed Win32=%lu",
      (unsigned long)err);
    if(overlay_log)overlay_log(line);
    return;
  }
  rounded=CreateRoundRectRgn(0,0,OVERLAY_WIDTH,OVERLAY_HEIGHT,20,20);
  if(!rounded || !SetWindowRgn(window,rounded,FALSE)) {
    if(rounded)DeleteObject(rounded);
    DestroyWindow(window);
    if(overlay_log)overlay_log("FOCUS-OVERLAY: background child region failed");
    return;
  }
  /* SetWindowRgn takes ownership after success. */
  overlay_hwnd=window;
  overlay_game=game_window;
  ShowWindow(window,SW_SHOWNOACTIVATE);
  InvalidateRect(window,NULL,FALSE);
  UpdateWindow(window);
  if(overlay_log)overlay_log("FOCUS-OVERLAY: pause banner retained INSIDE background game child");
}

void SpiderPauseOverlay_Hide(void) {
  HWND window=overlay_hwnd;
  overlay_hwnd=NULL;
  overlay_game=NULL;
  if(window && IsWindow(window))DestroyWindow(window);
  if(window && overlay_log)overlay_log("FOCUS-OVERLAY: one-piece popup destroyed");
}

int SpiderPauseOverlay_GetBounds(HWND game_window,RECT *bounds) {
  RECT desktop;
  POINT pt[2];
  if(!game_window||!bounds||!overlay_hwnd||!IsWindow(overlay_hwnd) ||
     overlay_game!=game_window || !GetWindowRect(overlay_hwnd,&desktop))return 0;
  pt[0].x=desktop.left;pt[0].y=desktop.top;
  pt[1].x=desktop.right;pt[1].y=desktop.bottom;
  MapWindowPoints(HWND_DESKTOP,game_window,pt,2);
  bounds->left=pt[0].x;bounds->top=pt[0].y;
  bounds->right=pt[1].x;bounds->bottom=pt[1].y;
  return 1;
}

int SpiderPauseOverlay_ExcludeFromRegion(HWND game_window,HRGN visible_region) {
  RECT bounds;
  HRGN banner;
  int ok;
  if(!visible_region)return 0;
  if(!SpiderPauseOverlay_GetBounds(game_window,&bounds))return 1;
  /* Exclude the WHOLE panel from the gridded global shade, not per-glyph
   * masks; all white pixels now live inside one fully composited bitmap. */
  banner=CreateRoundRectRgn(bounds.left,bounds.top,bounds.right,bounds.bottom,20,20);
  if(!banner)return 0;
  ok=CombineRgn(visible_region,visible_region,banner,RGN_DIFF)!=ERROR;
  DeleteObject(banner);
  return ok;
}
