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
static int guide_requested=0,guide_frame=0,guide_reported=0;
static LONG guide_draws=0;

/* The verified retail 2001 executable's gActionMapRelated owns the
 * joystick-vs-keyboard configuration mode, NOT its screen lifetime.
 * Read-only inspection while the menu renders; never patch this value. */
#define SPIDER_NATIVE_JOYSTICK_CONTEXT ((const volatile DWORD *)(uintptr_t)0x00AC129Cu)

void SpiderSDL3ControllerSetup_Init(const char *ini,
                                     void (*logger)(const char *)) {
  (void)ini;
  guide_report=logger;
  guide_enabled=1;
  guide_requested=0;
  guide_frame=0;
  guide_reported=0;
  guide_draws=0;
  if(guide_report)
    guide_report("SDL3-GUIDE: native Joystick reference overlay armed (no external window, action remapping or profile write)");
}
int SpiderSDL3ControllerSetup_Enabled(void) {return guide_enabled;}
void SpiderSDL3ControllerSetup_OnNativeEnter(void) {
  if(!guide_enabled)return;
  guide_requested=1;
  guide_frame=0;
  guide_reported=0;
  guide_draws=0;
  if(guide_report)
    guide_report("SDL3-GUIDE: native Joystick screen entered; waiting for verified menu context before compositing");
}
int SpiderSDL3ControllerSetup_IsNativeActive(void) {
  return guide_enabled && guide_requested;
}
void SpiderSDL3ControllerSetup_OnNativeFrame(void) {
  if(SpiderSDL3ControllerSetup_IsNativeActive())guide_frame=1;
}
void SpiderSDL3ControllerSetup_OnNativeExit(void) {
  guide_requested=0;
  guide_frame=0;
  if(guide_report)
    guide_report("SDL3-GUIDE: original Joystick screen exited; native overlay deactivated");
}
/* Retained for ABI compatibility, never opens an external popup. */
void SpiderSDL3ControllerSetup_Open(void) {
  if(guide_report)
    guide_report("SDL3-GUIDE: external window retired; use original Options > Joystick native screen");
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
  HBRUSH grip=CreateSolidBrush(RGB(146,150,169));
  HBRUSH shell=CreateSolidBrush(RGB(210,212,223));
  HBRUSH shade=CreateSolidBrush(RGB(97,101,122));
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
  {
    POINT dpad[]={{282,184},{298,184},{298,199},{313,199},
      {313,215},{298,215},{298,230},{282,230},{282,215},
      {267,215},{267,199},{282,199}};
    Polygon(dc,dpad,sizeof(dpad)/sizeof(dpad[0]));
  }
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
  /* Real PlayStation glyph shapes instead of placeholder letters. */
  {
    HPEN glyph=CreatePen(PS_SOLID,2,RGB(250,250,255));
    HGDIOBJ p=SelectObject(dc,glyph);
    HGDIOBJ b=SelectObject(dc,GetStockObject(NULL_BRUSH));
    POINT triangle[]={{357,178},{351,190},{363,190}};
    Polygon(dc,triangle,3);
    Ellipse(dc,370,197,382,209);
    gline(dc,352,218,362,228);gline(dc,362,218,352,228);
    Rectangle(dc,333,197,345,209);
    SelectObject(dc,b);SelectObject(dc,p);DeleteObject(glyph);
  }
}
/* Only controller artwork is drawn on the DirectDraw surface. Native
 * title, frame, background and all action legends belong to the game. */
static BOOL draw_native_guide(HDC dc,const RECT *content) {
  int saved;
  if(!dc || !content || content->right<=content->left ||
     content->bottom<=content->top)return FALSE;
  saved=SaveDC(dc);
  if(!saved)return FALSE;
  if(!SetMapMode(dc,MM_ANISOTROPIC)) {
    RestoreDC(dc,saved);
    return FALSE;
  }
  SetWindowExtEx(dc,640,480,NULL);
  SetViewportExtEx(dc,content->right-content->left,
                        content->bottom-content->top,NULL);
  SetViewportOrgEx(dc,content->left,content->top,NULL);
  draw_gamepad(dc);
  RestoreDC(dc,saved);
  return TRUE;
}
int SpiderSDL3ControllerSetup_DrawToSurface(
    IDirectDrawSurface7 *surface,const RECT *content) {
  HDC dc=NULL;
  HRESULT hr;
  BOOL drawn;
  if(!SpiderSDL3ControllerSetup_IsNativeActive() || !guide_frame)return 0;
  /* One fresh native display event authorizes exactly one presentation.
   * Never rely on gActionMapRelated, which remains set after menu exit. */
  guide_frame=0;
  if(!surface || !content || !*SPIDER_NATIVE_JOYSTICK_CONTEXT)return 0;
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
