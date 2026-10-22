/* Short-lived native screenshot confirmation, rendered only on the
 * windowed DirectDraw PRIMARY after presenting the completed game frame.
 * No overlay HWND, no keyboard hook and no drawing into the 3D backbuffer.
 * This guarantees the screenshot itself cannot contain its confirmation.
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
  /* Compact variant of the native ESC pause ribbon: blue-violet fill,
   * luminous lavender edges, pale text and red outlined heading. */
  brush=CreateSolidBrush(RGB(50,50,160));
  border=CreatePen(PS_SOLID,2,RGB(169,148,251));
  if(!brush || !border) {
    if(brush)DeleteObject(brush);
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
  DeleteObject(brush);
restore_font:
  SelectObject(dc,old_font);
font_done:
  DeleteObject(font);
  return ok;
}

BOOL SpiderF9Toast_DrawAfterPresent(IDirectDrawSurface7 *primary,
                                     HWND window,DWORD width,DWORD height) {
  WCHAR path[MAX_PATH];
  DWORD expires;
  BOOL success;
  HDC dc=NULL;
  HRESULT hr=DDERR_INVALIDOBJECT;
  BOOL result;
  AcquireSRWLockShared(&toast_mutex);
  expires=toast_expires_at;
  success=toast_success;
  if(expires && (LONG)(expires-GetTickCount())>0)
    memcpy(path,toast_path,sizeof(path));
  else path[0]=L'\0';
  ReleaseSRWLockShared(&toast_mutex);
  if(path[0]==L'\0' || !window || !IsWindow(window) ||
     GetForegroundWindow()!=window)return FALSE;
  /* Never touch the 3D backbuffer: the PRIMARY receives the final frame
   * first, then this short-lived overlay. Prefer DirectDraw's matching
   * primary HDC; use the visible game window HDC if unavailable. */
  if(primary)hr=IDirectDrawSurface7_GetDC(primary,&dc);
  if(SUCCEEDED(hr) && dc) {
    result=toast_draw_dc(dc,width,height,path,success);
    IDirectDrawSurface7_ReleaseDC(primary,dc);
    return result;
  }
  dc=GetDC(window);
  if(!dc)return FALSE;
  result=toast_draw_dc(dc,width,height,path,success);
  ReleaseDC(window,dc);
  return result;
}
