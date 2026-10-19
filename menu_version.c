#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <string.h>
#include "menu_version.h"
#include "version_info.h"

/* Draw on the menu backbuffer before presenting. The driver may reject
 * GetDC on a video-memory surface; the caller can then draw to the window.
 */
static HFONT version_font=NULL;
static int version_font_height=0;
static int larger(int a,int b) {return a>b?a:b;}

static BOOL draw_label(HDC dc,const RECT *content) {
  const char *label=SPIDERMAN2K1FIX_VERSION_LABEL;
  const int len=(int)strlen(label);
  const int height=content?content->bottom-content->top:0;
  const int width=content?content->right-content->left:0;
  HFONT old_font;
  int font_pixels,pad,x,y;
  SIZE measure;
  COLORREF old_color;
  int old_background;
  BOOL result;
  if(!dc || height<240 || width<320) return FALSE;
  font_pixels=larger(12,(height*11+240)/480);
  pad=larger(8,(height*10+240)/480);
  if(!version_font || version_font_height!=font_pixels) {
    HFONT replacement=CreateFontA(-font_pixels,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
      DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
      ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,"Arial");
    if(!replacement) return FALSE;
    if(version_font) DeleteObject(version_font);
    version_font=replacement;
    version_font_height=font_pixels;
  }
  old_font=(HFONT)SelectObject(dc,version_font);
  if(!old_font || old_font==HGDI_ERROR) return FALSE;
  old_background=SetBkMode(dc,TRANSPARENT);
  old_color=GetTextColor(dc);
  memset(&measure,0,sizeof(measure));
  if(!GetTextExtentPoint32A(dc,label,len,&measure)) {
    SetTextColor(dc,old_color);
    SetBkMode(dc,old_background);
    SelectObject(dc,old_font);
    return FALSE;
  }
  x=content->right-pad-measure.cx;
  y=content->bottom-pad-measure.cy;
  if(x<content->left+pad || y<content->top) {
    SetTextColor(dc,old_color);
    SetBkMode(dc,old_background);
    SelectObject(dc,old_font);
    return FALSE;
  }
  SetTextColor(dc,RGB(0,0,0));
  TextOutA(dc,x+1,y+1,label,len);
  TextOutA(dc,x-1,y+1,label,len);
  SetTextColor(dc,RGB(235,235,235));
  result=TextOutA(dc,x,y,label,len);
  SetTextColor(dc,old_color);
  SetBkMode(dc,old_background);
  SelectObject(dc,old_font);
  return result;
}

BOOL SpiderMenuVersion_DrawToSurface(IDirectDrawSurface7 *surface,const RECT *content) {
  HDC dc=NULL;
  HRESULT hr;
  BOOL drawn;
  if(!surface || !content) return FALSE;
  hr=IDirectDrawSurface7_GetDC(surface,&dc);
  if(FAILED(hr) || !dc) return FALSE;
  drawn=draw_label(dc,content);
  IDirectDrawSurface7_ReleaseDC(surface,dc);
  return drawn;
}

BOOL SpiderMenuVersion_DrawToWindow(HWND window,const RECT *content) {
  HDC dc;
  BOOL drawn;
  if(!window || !IsWindow(window) || !content) return FALSE;
  dc=GetDC(window);
  if(!dc) return FALSE;
  drawn=draw_label(dc,content);
  ReleaseDC(window,dc);
  return drawn;
}
