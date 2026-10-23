/* Short-lived F9 screenshot confirmation, normally composited into a
 * DirectDraw OFFSCREEN surface before one PRIMARY blit. Static comic panels
 * have no Flip, so ONLY then a no-focus, click-through, layered popup drawn
 * by a separate window thread displays the same artwork for two seconds.
 * Neither route modifies the screenshot backbuffer or the game's input.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "f9_toast.h"

#define SPIDER_F9_TOAST_MS 2000u
#define SPIDER_F9_TOAST_MAX_LINES 32
static SRWLOCK toast_mutex=SRWLOCK_INIT;
static WCHAR toast_path[MAX_PATH];
static BOOL toast_success=TRUE;
static DWORD toast_expires_at=0;
/* An idle comic panel may have no Flip while the worker saves its PNG.
 * Start the 2.0s timer on first ACTUAL presentation, with a 10s grace
 * window to avoid displaying an old notification far into the next scene. */
#define SPIDER_F9_TOAST_GRACE_MS 10000u
static DWORD toast_published_at=0;
static DWORD toast_generation=0;
static DWORD toast_displayed_generation=0;
static BOOL toast_current_is_comic=FALSE;
static HWND toast_game_window=NULL;
static void (*toast_logger)(const char*)=NULL;
static volatile LONG toast_popup_generation=0;
static DWORD WINAPI toast_popup_thread(LPVOID arg);

void SpiderF9Toast_SetGameWindow(HWND hwnd,void (*logger)(const char*)) {
  toast_game_window=hwnd;
  toast_logger=logger;
}
static void toast_start_no_flip_fallback(DWORD gen) {
  HANDLE worker;
  if(!toast_game_window)return;
  worker=CreateThread(NULL,0,toast_popup_thread,(LPVOID)(ULONG_PTR)gen,0,NULL);
  if(worker)CloseHandle(worker);
}

void SpiderF9Toast_GetDisplayStatus(DWORD *published,DWORD *displayed) {
  AcquireSRWLockShared(&toast_mutex);
  if(published)*published=toast_generation;
  if(displayed)*displayed=toast_displayed_generation;
  ReleaseSRWLockShared(&toast_mutex);
}

/* Comic context is delivered by each PNG worker, never shared between
 * concurrent screenshot requests. On comics, the window is the only renderer.
 * Moving frames retain their previous no-HWND DirectDraw presentation. */
static void toast_publish_saved(const WCHAR *full_path,BOOL comic) {
  DWORD attrs,generation;
  size_t len;
  if(!full_path)return;
  len=wcslen(full_path);
  if(!len || len>=MAX_PATH)return;
  attrs=GetFileAttributesW(full_path);
  if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&FILE_ATTRIBUTE_DIRECTORY))return;
  AcquireSRWLockExclusive(&toast_mutex);
  memcpy(toast_path,full_path,(len+1u)*sizeof(WCHAR));
  toast_success=TRUE;
  toast_current_is_comic=comic;
  toast_published_at=GetTickCount();
  toast_expires_at=0; /* Display starts the countdown. */
  generation=++toast_generation;
  ReleaseSRWLockExclusive(&toast_mutex);
  if(comic)toast_start_no_flip_fallback(generation);
}

void SpiderF9Toast_PublishSavedPath(const WCHAR *full_path) {
  toast_publish_saved(full_path,FALSE);
}
void SpiderF9Toast_PublishSavedPathComic(const WCHAR *full_path) {
  toast_publish_saved(full_path,TRUE);
}

void SpiderF9Toast_PublishError(const WCHAR *reason) {
  size_t len;
  if(!reason)return;
  len=wcslen(reason);
  if(!len || len>=MAX_PATH)return;
  AcquireSRWLockExclusive(&toast_mutex);
  memcpy(toast_path,reason,(len+1u)*sizeof(WCHAR));
  toast_success=FALSE;
  toast_current_is_comic=FALSE;
  toast_published_at=GetTickCount();
  toast_expires_at=0;
  ++toast_generation;
  ReleaseSRWLockExclusive(&toast_mutex);
}

static int toast_max(int a,int b) {return a>b?a:b;}
static int toast_min(int a,int b) {return a<b?a:b;}

/* Long path components are broken at a directory separator if possible;
 * otherwise use the last character that fits. Every UTF-16 code unit is
 * preserved; unlike DT_END_ELLIPSIS, no part of the path is hidden. */
static int toast_wrap_path(HDC dc,const WCHAR *text,int width,
                           int *starts,int *lengths) {
  int offset=0,lines=0,total=(int)wcslen(text);
  while(offset<total && lines<SPIDER_F9_TOAST_MAX_LINES) {
    int lo=1,hi=total-offset,fit=1,cut,i;
    SIZE measured;
    while(lo<=hi) {
      int mid=lo+(hi-lo)/2;
      if(GetTextExtentPoint32W(dc,text+offset,mid,&measured) &&
         measured.cx<=width) {fit=mid;lo=mid+1;}
      else hi=mid-1;
    }
    cut=fit;
    if(offset+cut<total && cut>=6) {
      for(i=cut-1;i>cut/2;i--) {
        if(text[offset+i]==L'\\') {cut=i+1;break;}
      }
    }
    starts[lines]=offset;
    lengths[lines]=cut;
    offset+=cut;
    ++lines;
  }
  return offset==total?lines:0;
}


/* Same native GDI AlphaBlend technique as the ESC pause ribbon: tint only a
 * rounded region on the distinct DirectDraw staging surface. The rendered
 * scene behind the notification remains visible and F9's original backbuffer
 * is never changed. */
#define SPIDER_F9_PANEL_ALPHA 165u /* ~65% opaque; 35% scene visible. */
static BOOL toast_draw_translucent_panel(HDC dc,const RECT *panel) {
  const BLENDFUNCTION blend={AC_SRC_OVER,0,SPIDER_F9_PANEL_ALPHA,0};
  HDC tint=NULL;
  HBITMAP pixel=NULL,old_pixel=NULL;
  HBRUSH color=NULL;
  HRGN rounded=NULL;
  RECT dot={0,0,1,1};
  int saved=0;
  BOOL ok=FALSE;
  if(!dc || !panel || panel->right<=panel->left ||
     panel->bottom<=panel->top)return FALSE;
  tint=CreateCompatibleDC(dc);
  if(!tint)goto done;
  pixel=CreateCompatibleBitmap(dc,1,1);
  color=CreateSolidBrush(RGB(50,50,160));
  rounded=CreateRoundRectRgn(panel->left,panel->top,panel->right,
                             panel->bottom,8,8);
  if(!pixel || !color || !rounded)goto done;
  old_pixel=(HBITMAP)SelectObject(tint,pixel);
  if(!old_pixel || old_pixel==(HBITMAP)HGDI_ERROR) {
    old_pixel=NULL;
    goto done;
  }
  if(!FillRect(tint,&dot,color))goto done;
  saved=SaveDC(dc);
  if(!saved)goto done;
  if(ExtSelectClipRgn(dc,rounded,RGN_AND)!=ERROR)
    ok=AlphaBlend(dc,panel->left,panel->top,
                  panel->right-panel->left,panel->bottom-panel->top,
                  tint,0,0,1,1,blend)!=FALSE;
  RestoreDC(dc,saved);
done:
  if(old_pixel)SelectObject(tint,old_pixel);
  if(rounded)DeleteObject(rounded);
  if(color)DeleteObject(color);
  if(pixel)DeleteObject(pixel);
  if(tint)DeleteDC(tint);
  return ok;
}

static BOOL toast_draw_dc(HDC dc,DWORD w,DWORD h,const WCHAR *path,BOOL success) {
  static const WCHAR saved_heading[]=L"Screenshot saved to:";
  static const WCHAR error_heading[]=L"Screenshot failed:";
  const WCHAR *heading=success?saved_heading:error_heading;
  int font_px,pad,step,max_w,max_path_w,box_w,box_h;
  int starts[SPIDER_F9_TOAST_MAX_LINES],lengths[SPIDER_F9_TOAST_MAX_LINES];
  int lines,path_len,x,y,i;
  HFONT font,old_font;
  HBRUSH brush;
  HPEN border,old_pen;
  HGDIOBJ old_brush;
  SIZE heading_size,path_size;
  RECT panel;
  int old_mode;
  COLORREF old_color;
  BOOL ok=FALSE;
  if(!dc || !path || w<320 || h<240 || w>8192 || h>8192)return FALSE;
  font_px=toast_min(19,toast_max(12,(int)h/75));
  pad=toast_max(9,font_px/2+3);
  step=font_px+6;
  font=CreateFontW(-font_px,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,
                   DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                   CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  if(!font)return FALSE;
  old_font=(HFONT)SelectObject(dc,font);
  if(!old_font || old_font==HGDI_ERROR)goto font_done;
  memset(&heading_size,0,sizeof(heading_size));
  memset(&path_size,0,sizeof(path_size));
  path_len=(int)wcslen(path);
  if(path_len==0 || !GetTextExtentPoint32W(dc,heading,
       (int)wcslen(heading),&heading_size) ||
     !GetTextExtentPoint32W(dc,path,path_len,&path_size))goto restore_font;
  max_w=toast_min((int)w-24,(int)((w*55u)/100u));
  if(max_w<=2*pad+30)goto restore_font;
  box_w=toast_min(max_w,toast_max(heading_size.cx,path_size.cx)+2*pad);
  max_path_w=box_w-2*pad;
  lines=toast_wrap_path(dc,path,max_path_w,starts,lengths);
  if(lines<1)goto restore_font;
  box_h=(lines+1)*step+2*pad+3;
  x=toast_max(12,(int)w/75);
  y=(int)h-toast_max(16,(int)h/50)-box_h;
  if(y<0)goto restore_font;
  panel.left=x;panel.top=y;panel.right=x+box_w;panel.bottom=y+box_h;
  /* Native ESC palette, transparent blue-violet fill and subtle lavender
   * outline. Paint only on the offscreen composed frame. */
  if(!toast_draw_translucent_panel(dc,&panel))goto restore_font;
  brush=(HBRUSH)GetStockObject(HOLLOW_BRUSH);
  border=CreatePen(PS_SOLID,1,RGB(169,148,251));
  if(!brush || !border) {
    if(border)DeleteObject(border);
    goto restore_font;
  }
  old_brush=SelectObject(dc,brush);
  old_pen=(HPEN)SelectObject(dc,border);
  if(old_brush && old_brush!=HGDI_ERROR && old_pen &&
     old_pen!=HGDI_ERROR) {
    HBRUSH rim=CreateSolidBrush(RGB(231,220,255));
    RoundRect(dc,panel.left,panel.top,panel.right,panel.bottom,8,8);
    if(rim) {
      RECT top={panel.left+3,panel.top+3,panel.right-3,panel.top+4};
      RECT bottom={panel.left+3,panel.bottom-4,panel.right-3,panel.bottom-3};
      FillRect(dc,&top,rim);
      FillRect(dc,&bottom,rim);
      DeleteObject(rim);
    }
    old_mode=SetBkMode(dc,TRANSPARENT);
    old_color=GetTextColor(dc);
    SetTextColor(dc,success?RGB(240,47,69):RGB(255,120,120));
    TextOutW(dc,x+pad,y+pad,heading,
             (int)wcslen(heading));
    SetTextColor(dc,RGB(245,240,255));
    for(i=0;i<lines;i++)
      TextOutW(dc,x+pad,y+pad+(i+1)*step,
               path+starts[i],lengths[i]);
    SetTextColor(dc,old_color);
    SetBkMode(dc,old_mode);
    ok=TRUE;
  }
  if(old_pen && old_pen!=HGDI_ERROR)SelectObject(dc,old_pen);
  if(old_brush && old_brush!=HGDI_ERROR)SelectObject(dc,old_brush);
  DeleteObject(border);
restore_font:
  SelectObject(dc,old_font);
font_done:
  DeleteObject(font);
  return ok;
}

/* Compose BEFORE presentation onto a temporary DirectDraw offscreen
 * surface. The original backbuffer stays byte-for-byte untouched, including
 * for a second F9 capture while the notification is still visible.
 * A single PRIMARY Blt transfers the scene and notice together; no more GDI
 * writes to the visible PRIMARY racing the next frame. */
static IDirectDrawSurface7 *toast_stage=NULL;
static IDirectDrawSurface7 *toast_stage_back=NULL; /* weak identity only */
static DWORD toast_stage_width=0,toast_stage_height=0;
static BOOL toast_prefer_system_memory=FALSE;

static void toast_discard_stage(void) {
  if(toast_stage) IDirectDrawSurface7_Release(toast_stage);
  toast_stage=NULL;
  toast_stage_back=NULL;
  toast_stage_width=toast_stage_height=0;
}

void SpiderF9Toast_ResetPresentation(void) {
  toast_discard_stage();
  toast_prefer_system_memory=FALSE;
}

static BOOL toast_create_stage(IDirectDrawSurface7 *back,DWORD width,
                               DWORD height,BOOL system_memory) {
  IDirectDraw7 *dd=NULL;
  DDSURFACEDESC2 desc;
  HRESULT hr;
  if(FAILED(IDirectDrawSurface7_GetDDInterface(back,(void**)&dd)) || !dd)
    return FALSE;
  memset(&desc,0,sizeof(desc));
  desc.dwSize=sizeof(desc);
  desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
  desc.dwWidth=width;
  desc.dwHeight=height;
  desc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|
                      (system_memory?DDSCAPS_SYSTEMMEMORY:DDSCAPS_VIDEOMEMORY);
  hr=IDirectDraw7_CreateSurface(dd,&desc,&toast_stage,NULL);
  IDirectDraw7_Release(dd);
  if(FAILED(hr) || !toast_stage) {
    toast_stage=NULL;
    return FALSE;
  }
  toast_stage_back=back;
  toast_stage_width=width;
  toast_stage_height=height;
  return TRUE;
}

/* The game's comic page can remain visible for many seconds without a
 * single DirectDraw Flip. Rather than forcing Flip (which would disrupt the
 * game), provide a self-contained, input-transparent layered window only if
 * the normal compositor has NOT presented the notification after 160 ms.
 * This window has its own short-lived message pump and does not subclass or
 * post keyboard messages to the game window.
 */
static LRESULT CALLBACK toast_popup_wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_NCHITTEST)return HTTRANSPARENT;
  if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
  return DefWindowProcW(hwnd,msg,wp,lp);
}

static DWORD WINAPI toast_popup_thread(LPVOID arg) {
  const DWORD gen=(DWORD)(ULONG_PTR)arg;
  const WCHAR classname[]=L"SpiderMan2k1Fix-F9-StaticNotice";
  const DWORD delay_ms=160u;
  HWND game=NULL,popup=NULL;
  HINSTANCE instance=GetModuleHandleW(NULL);
  HDC screen=NULL,mem=NULL;
  HBITMAP bitmap=NULL,old_bitmap=NULL;
  BITMAPINFO info;
  WNDCLASSW wc;
  BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
  RECT game_rect;
  POINT pos,src={0,0};
  SIZE size;
  WCHAR path[MAX_PATH];
  BOOL success=FALSE,ready=FALSE,shown=FALSE;
  DWORD *pixels=NULL;
  DWORD i,n,until=0;
  DWORD bg=0;
  Sleep(delay_ms);
  AcquireSRWLockShared(&toast_mutex);
  if(toast_generation==gen && toast_displayed_generation!=gen &&
     toast_path[0] && (DWORD)(GetTickCount()-toast_published_at) <
                                SPIDER_F9_TOAST_GRACE_MS) {
    memcpy(path,toast_path,sizeof(path));
    success=toast_success;
    ready=TRUE;
  }
  ReleaseSRWLockShared(&toast_mutex);
  game=toast_game_window;
  if(!ready || !game || !IsWindow(game) ||
     GetForegroundWindow()!=game || !GetWindowRect(game,&game_rect))return 0;
  size.cx=game_rect.right-game_rect.left;
  size.cy=game_rect.bottom-game_rect.top;
  if(size.cx<320 || size.cy<240 || size.cx>8192 || size.cy>8192)return 0;
  screen=GetDC(NULL); /* Never obtain or write a DirectDraw PRIMARY DC. */
  if(!screen)goto end;
  mem=CreateCompatibleDC(screen);
  if(!mem)goto end;
  memset(&info,0,sizeof(info));
  info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth=size.cx;
  info.bmiHeader.biHeight=-size.cy;
  info.bmiHeader.biPlanes=1;
  info.bmiHeader.biBitCount=32;
  info.bmiHeader.biCompression=BI_RGB;
  bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);
  if(!bitmap || !pixels)goto end;
  old_bitmap=(HBITMAP)SelectObject(mem,bitmap);
  if(!old_bitmap || old_bitmap==(HBITMAP)HGDI_ERROR) {
    old_bitmap=NULL;goto end;
  }
  n=(DWORD)size.cx*(DWORD)size.cy;
  memset(pixels,0,(size_t)n*sizeof(DWORD));
  if(!toast_draw_dc(mem,(DWORD)size.cx,(DWORD)size.cy,path,success))goto end;
  /* The existing compositor alpha-blends violet against an initially black
   * DIB, producing premultiplied (32,32,104) for its 65%-opacity fill.
   * Preserve that premultiplied fill; make heading/text/rim fully opaque
   * and all unused space fully transparent for UpdateLayeredWindow. */
  /* DWORD layout of a top-down 32-bit DIB is 0x00RRGGBB,
   * even though byte addresses are BGRA little-endian. */
  bg=(DWORD)((160u*SPIDER_F9_PANEL_ALPHA+127u)/255u) |
     ((DWORD)((50u*SPIDER_F9_PANEL_ALPHA+127u)/255u)<<8) |
     ((DWORD)((50u*SPIDER_F9_PANEL_ALPHA+127u)/255u)<<16);
  for(i=0;i<n;i++) {
    DWORD rgb=pixels[i]&0x00ffffffu;
    if(!rgb) pixels[i]=0u;
    else if(rgb==bg)pixels[i]=rgb|((DWORD)SPIDER_F9_PANEL_ALPHA<<24);
    else pixels[i]=rgb|0xff000000u;
  }

  memset(&wc,0,sizeof(wc));
  wc.lpfnWndProc=toast_popup_wndproc;
  wc.hInstance=instance;
  wc.lpszClassName=classname;
  if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)goto end;
  /* The game thread may stop pumping all messages while a BD page is
   * static. Do NOT make the popup owned by the game HWND: cross-thread
   * owner/window notifications can delay window destruction. */
  popup=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|
        WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,
        classname,L"",WS_POPUP,game_rect.left,game_rect.top,size.cx,size.cy,
        NULL,NULL,instance,NULL); /* never own game HWND across threads */
  if(!popup)goto end;
  pos.x=game_rect.left;pos.y=game_rect.top;
  if(!UpdateLayeredWindow(popup,screen,&pos,&size,mem,&src,0,&blend,ULW_ALPHA))
    goto end;
  AcquireSRWLockExclusive(&toast_mutex);
  if(toast_generation==gen && toast_displayed_generation!=gen &&
     GetForegroundWindow()==game) {
    toast_displayed_generation=gen;
    toast_expires_at=GetTickCount()+SPIDER_F9_TOAST_MS;
    InterlockedExchange(&toast_popup_generation,(LONG)gen);
    shown=TRUE;
  }
  ReleaseSRWLockExclusive(&toast_mutex);
  if(!shown)goto end;
  ShowWindow(popup,SW_SHOWNOACTIVATE);
  SetWindowPos(popup,HWND_TOPMOST,pos.x,pos.y,size.cx,size.cy,
               SWP_NOACTIVATE|SWP_SHOWWINDOW);
  if(toast_logger) {
    char msg[150];
    snprintf(msg,sizeof(msg),
      "F9-TOAST-STATIC: generation=%lu displayed without Flip (layered no-focus HWND)",
      (unsigned long)gen);
    toast_logger(msg);
  }
  until=GetTickCount()+SPIDER_F9_TOAST_MS;
  while((LONG)(until-GetTickCount())>0) {
    MSG msg;
    if(GetForegroundWindow()!=game || !IsWindow(game))break;
    AcquireSRWLockShared(&toast_mutex);
    ready=(toast_generation==gen);
    ReleaseSRWLockShared(&toast_mutex);
    if(!ready)break;
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) {
      if(msg.message==WM_QUIT)goto end;
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    MsgWaitForMultipleObjects(0,NULL,FALSE,40,QS_ALLINPUT);
  }
end:
  if(popup) {
    /* Force Windows to remove the layered popup even if the game has not
     * produced a single DirectDraw Flip. This cleanup runs on its owner
     * thread, without calling any game callback. */
    ShowWindow(popup,SW_HIDE);
    SetWindowPos(popup,NULL,0,0,0,0,
                 SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_HIDEWINDOW);
    DestroyWindow(popup);
    if(shown && toast_logger) {
      char msg[144];
      snprintf(msg,sizeof(msg),
        "F9-TOAST-STATIC: generation=%lu hidden and destroyed after 2s",
        (unsigned long)gen);
      toast_logger(msg);
    }
  }
  if(shown)InterlockedCompareExchange(&toast_popup_generation,0,(LONG)gen);
  if(old_bitmap)SelectObject(mem,old_bitmap);
  if(bitmap)DeleteObject(bitmap);
  if(mem)DeleteDC(mem);
  if(screen)ReleaseDC(NULL,screen);
  return 0;
}

IDirectDrawSurface7 *SpiderF9Toast_CompositeFrame(
    IDirectDrawSurface7 *back,HWND window,DWORD width,DWORD height) {
  WCHAR path[MAX_PATH];
  DWORD expires,published,generation;
  BOOL success,comic;
  int attempt;
  AcquireSRWLockShared(&toast_mutex);
  expires=toast_expires_at;
  published=toast_published_at;
  generation=toast_generation;
  success=toast_success;
  comic=toast_current_is_comic;
  if(toast_path[0] &&
     ((expires && (LONG)(expires-GetTickCount())>0) ||
      (!expires && (DWORD)(GetTickCount()-published) < SPIDER_F9_TOAST_GRACE_MS)))
    memcpy(path,toast_path,sizeof(path));
  else path[0]=L'\\0';
  ReleaseSRWLockShared(&toast_mutex);
  /* Never paint a comic notification into a frozen DirectDraw framebuffer:
   * after the last Flip it could remain visible indefinitely. The layered
   * window has its own expiration and needs no further game presentation. */
  if(comic) {
    toast_discard_stage();
    return NULL;
  }
  if((DWORD)InterlockedCompareExchange(&toast_popup_generation,0,0)==generation)
    return NULL;
  if(path[0]==L'\\0') {
    toast_discard_stage(); /* No per-frame memory or blit cost while inactive. */
    return NULL;
  }
  if(!back || !window || !IsWindow(window) ||
     GetForegroundWindow()!=window || width<320 || height<240 ||
     width>8192 || height>8192)return NULL;
  if(toast_stage && (toast_stage_back!=back ||
     toast_stage_width!=width || toast_stage_height!=height)) {
    SpiderF9Toast_ResetPresentation();
  }
  for(attempt=0;attempt<2;attempt++) {
    HDC dc=NULL;
    HRESULT hr;
    BOOL drawn=FALSE;
    if(!toast_stage &&
       !toast_create_stage(back,width,height,toast_prefer_system_memory)) {
      toast_prefer_system_memory=TRUE;
      continue;
    }
    /* The 3D source is copied untouched into a distinct present surface. */
    hr=IDirectDrawSurface7_Blt(toast_stage,NULL,back,NULL,DDBLT_WAIT,NULL);
    if(SUCCEEDED(hr))hr=IDirectDrawSurface7_GetDC(toast_stage,&dc);
    if(SUCCEEDED(hr) && dc) {
      drawn=toast_draw_dc(dc,width,height,path,success);
      IDirectDrawSurface7_ReleaseDC(toast_stage,dc);
      if(drawn) {
        AcquireSRWLockExclusive(&toast_mutex);
        /* A newer worker publication may have arrived mid-composition.
         * Never start its countdown with the older notification's frame. */
        if(toast_generation==generation) {
          if(!toast_expires_at)toast_expires_at=GetTickCount()+SPIDER_F9_TOAST_MS;
          toast_displayed_generation=generation;
        }
        ReleaseSRWLockExclusive(&toast_mutex);
        return toast_stage;
      }
    }
    toast_discard_stage();
    toast_prefer_system_memory=TRUE; /* Retry with a GDI-compatible surface. */
  }
  return NULL; /* Present the untouched game frame if composing fails. */
}
