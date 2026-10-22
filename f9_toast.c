/* Short-lived F9 screenshot confirmation, composited into a cached
 * DirectDraw OFFSCREEN presentation surface before a single PRIMARY blit.
 * No overlay HWND, keyboard hook, GDI writes on PRIMARY, or drawing into
 * the 3D backbuffer; the screenshot itself never contains its confirmation.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <string.h>
#include <wchar.h>
#include "f9_toast.h"

#define SPIDER_F9_TOAST_MS 3500u
#define SPIDER_F9_TOAST_MAX_LINES 32
static SRWLOCK toast_mutex=SRWLOCK_INIT;
static WCHAR toast_path[MAX_PATH];
static BOOL toast_success=TRUE;
static DWORD toast_expires_at=0;

void SpiderF9Toast_PublishSavedPath(const WCHAR *full_path) {
  DWORD attrs;
  size_t len;
  if(!full_path) return;
  len=wcslen(full_path);
  if(!len || len>=MAX_PATH) return;
  attrs=GetFileAttributesW(full_path);
  if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&FILE_ATTRIBUTE_DIRECTORY))return;
  AcquireSRWLockExclusive(&toast_mutex);
  memcpy(toast_path,full_path,(len+1u)*sizeof(WCHAR));
  toast_success=TRUE;
  toast_expires_at=GetTickCount()+SPIDER_F9_TOAST_MS;
  ReleaseSRWLockExclusive(&toast_mutex);
}

void SpiderF9Toast_PublishError(const WCHAR *reason) {
  size_t len;
  if(!reason)return;
  len=wcslen(reason);
  if(!len || len>=MAX_PATH)return;
  AcquireSRWLockExclusive(&toast_mutex);
  memcpy(toast_path,reason,(len+1u)*sizeof(WCHAR));
  toast_success=FALSE;
  toast_expires_at=GetTickCount()+SPIDER_F9_TOAST_MS;
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

IDirectDrawSurface7 *SpiderF9Toast_CompositeFrame(
    IDirectDrawSurface7 *back,HWND window,DWORD width,DWORD height) {
  WCHAR path[MAX_PATH];
  DWORD expires;
  BOOL success;
  int attempt;
  AcquireSRWLockShared(&toast_mutex);
  expires=toast_expires_at;
  success=toast_success;
  if(expires && (LONG)(expires-GetTickCount())>0)
    memcpy(path,toast_path,sizeof(path));
  else path[0]=L'\\0';
  ReleaseSRWLockShared(&toast_mutex);
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
      if(drawn)return toast_stage;
    }
    toast_discard_stage();
    toast_prefer_system_memory=TRUE; /* Retry with a GDI-compatible surface. */
  }
  return NULL; /* Present the untouched game frame if composing fails. */
}
