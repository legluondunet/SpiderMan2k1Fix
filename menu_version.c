#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <string.h>
#include "menu_version.h"
#include "version_info.h"

/* Draw on the menu backbuffer before presenting. The driver may reject
 * GetDC on a video-memory surface; the caller can then draw to the window.
 * Match the game's bold italic comic menu lettering, but remain a subdued
 * footer and never alter the version string or presentation coordinates.
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
  int font_pixels,pad,outline_pixels,x,y,dx,dy;
  SIZE measure;
  COLORREF old_color;
  int old_background;
  BOOL result;
  if(!dc || height<240 || width<320) return FALSE;
  /* Smaller than menu entries; scaled to the actual 4:3 content height. */
  font_pixels=larger(12,(height*10+240)/480);
  pad=larger(8,(height*10+240)/480);
  outline_pixels=larger(1,(height+479)/800);
  if(!version_font || version_font_height!=font_pixels) {
    HFONT replacement=CreateFontA(-font_pixels,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,
      DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
      ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,"Arial Black");
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
  /* Account for the outline and italic overhang in bottom/right padding. */
  x=content->right-pad-measure.cx-outline_pixels;
  y=content->bottom-pad-measure.cy-outline_pixels;
  if(x<content->left+pad+outline_pixels || y<content->top+outline_pixels) {
    SetTextColor(dc,old_color);
    SetBkMode(dc,old_background);
    SelectObject(dc,old_font);
    return FALSE;
  }
  /* Dark violet outline echoes the menu's inked comic typography. */
  SetTextColor(dc,RGB(19,13,38));
  for(dy=-1;dy<=1;dy++) for(dx=-1;dx<=1;dx++) {
    if(dx==0 && dy==0) continue;
    TextOutA(dc,x+dx*outline_pixels,y+dy*outline_pixels,label,len);
  }
  /* Muted lavender instead of an unrelated pure-white system font. */
  SetTextColor(dc,RGB(204,191,231));
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
