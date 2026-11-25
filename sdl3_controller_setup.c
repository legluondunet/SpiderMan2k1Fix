/* Integrated, read-only PS1-inspired controller reference.
 * Paint onto the ORIGINAL 2001 joystick menu's completed DirectDraw
 * backbuffer, on the SAME game Flip thread, before windowed presentation.
 * No Win32 window, modal message loop, graphics DLL, SDL input pumping,
 * controller file writes, or independent focus/keyboard ownership.
 * The retail menu retains its own Back/Escape/cursor/transition behavior;
 * its joystick rebinding/capture is separately SHA-gated and disabled. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "sdl3_controller_setup.h"

static void (*guide_report)(const char *)=NULL;
static int guide_enabled=0;
static int guide_requested=0,guide_seen_native=0,guide_reported=0;
static LONG guide_draws=0;

/* The verified retail 2001 executable's gActionMapRelated owns the
 * joystick-vs-keyboard configuration screen. 0 means Keyboard/other.
 * Read-only inspection while the menu renders; never patch this value. */
#define SPIDER_NATIVE_JOYSTICK_CONTEXT ((const volatile DWORD *)(uintptr_t)0x00AC129Cu)

void SpiderSDL3ControllerSetup_Init(const char *ini,
                                     void (*logger)(const char *)) {
  (void)ini;
  guide_report=logger;
  guide_enabled=1;
  guide_requested=0;
  guide_seen_native=0;
  guide_reported=0;
  guide_draws=0;
  if(guide_report)
    guide_report("SDL3-GUIDE: native Joystick reference overlay armed (no external window, action remapping or profile write)");
}
int SpiderSDL3ControllerSetup_Enabled(void) {return guide_enabled;}
void SpiderSDL3ControllerSetup_OnNativeEnter(void) {
  if(!guide_enabled)return;
  guide_requested=1;
  guide_seen_native=0;
  guide_reported=0;
  guide_draws=0;
  if(guide_report)
    guide_report("SDL3-GUIDE: native Joystick screen entered; waiting for verified menu context before compositing");
}
/* Retained for ABI compatibility, never opens an external popup. */
void SpiderSDL3ControllerSetup_Open(void) {
  if(guide_report)
    guide_report("SDL3-GUIDE: external window retired; use original Options > Joystick native screen");
}
static void gtext(HDC dc,int x,int y,const char *s) {
  if(s)TextOutA(dc,x,y,s,(int)strlen(s));
}
static void gline(HDC dc,int x,int y,int x2,int y2) {
  MoveToEx(dc,x,y,NULL);LineTo(dc,x2,y2);
}
static void pad_disc(HDC dc,int x,int y,int r,COLORREF fill) {
  HBRUSH b=CreateSolidBrush(fill);
  HGDIOBJ old=SelectObject(dc,b);
  Ellipse(dc,x-r,y-r,x+r,y+r);
  SelectObject(dc,old);DeleteObject(b);
}
static void draw_gamepad(HDC dc) {
  HPEN stroke=CreatePen(PS_SOLID,3,RGB(37,33,68));
  HPEN detail=CreatePen(PS_SOLID,1,RGB(103,99,154));
  HBRUSH grip=CreateSolidBrush(RGB(150,148,181));
  HBRUSH shell=CreateSolidBrush(RGB(201,199,217));
  HBRUSH shade=CreateSolidBrush(RGB(109,105,150));
  HGDIOBJ oldp=SelectObject(dc,stroke),oldb=SelectObject(dc,grip);
  /* PlayStation-style vector illustration, not a separate modal or bitmap.
   * Dual grips + central shell + bumpers preserve the native comic style. */
  Ellipse(dc,231,170,318,281);
  Ellipse(dc,321,170,408,281);
  SelectObject(dc,shell);
  RoundRect(dc,255,161,385,258,31,27);
  SelectObject(dc,shade);
  RoundRect(dc,260,152,309,170,9,9); /* L1 */
  RoundRect(dc,331,152,380,170,9,9); /* R1 */
  SelectObject(dc,grip);
  RoundRect(dc,280,192,299,242,5,5); /* d-pad vertical */
  RoundRect(dc,264,208,315,226,5,5); /* d-pad horizontal */
  SelectObject(dc,detail);
  pad_disc(dc,297,244,12,RGB(92,89,136)); /* sticks */
  pad_disc(dc,343,244,12,RGB(92,89,136));
  pad_disc(dc,357,185,11,RGB(89,130,168)); /* triangle */
  pad_disc(dc,376,203,11,RGB(179,84,111)); /* circle */
  pad_disc(dc,357,223,11,RGB(88,143,134)); /* cross */
  pad_disc(dc,339,203,11,RGB(148,100,172)); /* square */
  SelectObject(dc,oldb);SelectObject(dc,oldp);
  DeleteObject(stroke);DeleteObject(detail);
  DeleteObject(grip);DeleteObject(shell);DeleteObject(shade);
  SetBkMode(dc,TRANSPARENT);
  SetTextColor(dc,RGB(244,242,252));
  gtext(dc,353,180,"T");
  gtext(dc,373,198,"O");
  gtext(dc,353,218,"X");
  gtext(dc,336,198,"S");
}
static BOOL draw_native_guide(HDC dc,const RECT *content) {
  int oldmode,oldBk;
  SIZE windowext,viewext;
  POINT org;
  HPEN edge=CreatePen(PS_SOLID,2,RGB(166,138,239));
  HPEN fine=CreatePen(PS_SOLID,1,RGB(113,94,192));
  HBRUSH panel=CreateSolidBrush(RGB(24,16,76));
  HBRUSH panel2=CreateSolidBrush(RGB(45,31,109));
  HGDIOBJ oldp,oldb,oldf;
  HFONT title=CreateFontA(21,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
                         DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
                         DEFAULT_PITCH|FF_SWISS,"Arial");
  HFONT legend=CreateFontA(15,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
                          DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
                          DEFAULT_PITCH|FF_SWISS,"Arial");
  HFONT small=CreateFontA(12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                         DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
                         DEFAULT_PITCH|FF_SWISS,"Arial");
  if(!dc || !content || content->right<=content->left ||
     content->bottom<=content->top)return FALSE;
  /* Preserve and restore the DirectDraw surface DC mapping. All gamepad
   * art is in native 640x480 menu coordinates, never desktop geometry. */
  oldmode=GetMapMode(dc);
  GetWindowExtEx(dc,&windowext);
  GetViewportExtEx(dc,&viewext);
  GetViewportOrgEx(dc,&org);
  oldBk=SetBkMode(dc,TRANSPARENT);
  if(!SetMapMode(dc,MM_ANISOTROPIC))return FALSE;
  SetWindowExtEx(dc,640,480,NULL);
  SetViewportExtEx(dc,content->right-content->left,
                        content->bottom-content->top,NULL);
  SetViewportOrgEx(dc,content->left,content->top,NULL);
  oldp=SelectObject(dc,edge);
  oldb=SelectObject(dc,panel);
  RoundRect(dc,54,104,586,437,16,16);
  SelectObject(dc,panel2);
  RoundRect(dc,62,112,578,429,13,13);
  SelectObject(dc,fine);
  gline(dc,77,147,563,147);
  gline(dc,77,296,563,296);
  oldf=SelectObject(dc,title?title:GetStockObject(DEFAULT_GUI_FONT));
  SetTextColor(dc,RGB(244,224,255));
  gtext(dc,82,120,"JOYSTICK  /  COMMANDES");
  SelectObject(dc,small?small:GetStockObject(DEFAULT_GUI_FONT));
  SetTextColor(dc,RGB(201,189,249));
  gtext(dc,82,152,"Une manette active a la fois  |  commandes fixes");
  gtext(dc,90,189,"L1 / LB");
  gtext(dc,90,207,"Viser");
  gtext(dc,462,189,"R1 / RB");
  gtext(dc,462,207,"Zipline");
  gtext(dc,90,260,"Stick / croix");
  gtext(dc,90,275,"Deplacement");
  gtext(dc,462,260,"R2 / RT");
  gtext(dc,462,275,"Balancement");
  draw_gamepad(dc);
  SelectObject(dc,legend?legend:GetStockObject(DEFAULT_GUI_FONT));
  SetTextColor(dc,RGB(249,236,255));
  gtext(dc,85,311,"CROIX / A");
  gtext(dc,328,311,"Sauter");
  gtext(dc,85,336,"ROND / B");
  gtext(dc,328,336,"Coup de pied");
  gtext(dc,85,361,"CARRE / X");
  gtext(dc,328,361,"Coup de poing");
  gtext(dc,85,386,"TRIANGLE / Y");
  gtext(dc,328,386,"Toile");
  SelectObject(dc,small?small:GetStockObject(DEFAULT_GUI_FONT));
  SetTextColor(dc,RGB(200,187,246));
  gtext(dc,85,409,"Share / Back : Pause     |     Manette active : derniere utilisee");
  SelectObject(dc,oldf);
  SelectObject(dc,oldb);SelectObject(dc,oldp);
  if(title)DeleteObject(title);
  if(legend)DeleteObject(legend);
  if(small)DeleteObject(small);
  DeleteObject(panel);DeleteObject(panel2);
  DeleteObject(edge);DeleteObject(fine);
  SetViewportOrgEx(dc,org.x,org.y,NULL);
  SetViewportExtEx(dc,viewext.cx,viewext.cy,NULL);
  SetWindowExtEx(dc,windowext.cx,windowext.cy,NULL);
  SetMapMode(dc,oldmode);
  SetBkMode(dc,oldBk);
  return TRUE;
}
int SpiderSDL3ControllerSetup_DrawToSurface(
    IDirectDrawSurface7 *surface,const RECT *content) {
  DWORD native_context;
  HDC dc=NULL;
  HRESULT hr;
  BOOL drawn;
  if(!guide_enabled || !guide_requested || !surface || !content)return 0;
  native_context=*SPIDER_NATIVE_JOYSTICK_CONTEXT;
  if(!native_context) {
    if(guide_seen_native) {
      guide_requested=0;
      if(guide_report)
        guide_report("SDL3-GUIDE: original Joystick screen exited; native overlay deactivated");
    }
    return 0;
  }
  guide_seen_native=1;
  hr=IDirectDrawSurface7_GetDC(surface,&dc);
  if(FAILED(hr)||!dc) {
    if(!guide_reported && guide_report)
      guide_report("SDL3-GUIDE: native surface GetDC unavailable; original menu preserved");
    guide_reported=1;
    return 0;
  }
  drawn=draw_native_guide(dc,content);
  IDirectDrawSurface7_ReleaseDC(surface,dc);
  if(drawn && ++guide_draws==1 && guide_report)
    guide_report("SDL3-GUIDE: drawn INSIDE original Joystick menu backbuffer; 4:3 coordinates, no popup");
  return drawn?1:0;
}
