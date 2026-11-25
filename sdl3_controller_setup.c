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
/* Original artwork: a continuous, sculpted dual-analog shell, no game or
 * manufacturer bitmap, no external asset or font. All points are 640x480
 * logical coordinates. Native labels are 512x240 (see paired layout in
 * spider_guide_native_labels); their visual baseline is about 8px above
 * 2*y. Draw callouts after the shell so each terminates on its own control. */
static void gline(HDC dc,int x,int y,int x2,int y2) {
  MoveToEx(dc,x,y,NULL);LineTo(dc,x2,y2);
}
static void ink(HDC dc,COLORREF pen,COLORREF brush) {
  SetDCPenColor(dc,pen);SetDCBrushColor(dc,brush);
}
static void disc(HDC dc,int x,int y,int r,COLORREF edge,COLORREF fill) {
  ink(dc,edge,fill);Ellipse(dc,x-r,y-r,x+r+1,y+r+1);
}
static void rounded(HDC dc,int x,int y,int x2,int y2,int r,
                    COLORREF edge,COLORREF fill) {
  ink(dc,edge,fill);RoundRect(dc,x,y,x2,y2,r,r);
}
/* A single Bezier silhouette; the inner face is another continuous path,
 * not a rectangle pasted over two circular grips. */
static void shell_path(HDC dc,int dy,COLORREF edge,COLORREF fill) {
  static const POINT points[]={
    {219,152},{245,150},{268,156}, {291,163},{349,163},{372,156},
    {395,150},{421,152},{433,174}, {449,199},{457,259},{452,289},
    {447,317},{431,335},{414,333}, {394,331},{380,307},{368,289},
    {355,273},{347,278},{320,278}, {293,278},{285,273},{272,289},
    {260,307},{246,331},{226,333}, {209,335},{193,317},{188,289},
    {183,259},{191,199},{207,174}, {210,164},{214,158},{219,152}
  };
  POINT p[sizeof(points)/sizeof(points[0])];
  unsigned i;
  for(i=0;i<sizeof(points)/sizeof(points[0]);i++) {
    p[i]=points[i];p[i].y+=dy;
  }
  ink(dc,edge,fill);
  if(BeginPath(dc)) {
    MoveToEx(dc,219,152+dy,NULL);
    /* Twelve cubic segments close the shell. */
    PolyBezierTo(dc,p,(DWORD)(sizeof(p)/sizeof(p[0])));
    CloseFigure(dc);EndPath(dc);StrokeAndFillPath(dc);
  }
}
static void face_path(HDC dc) {
  const POINT p[]={
    {245,159},{265,166},{280,170},
    {302,177},{338,177},{360,170},
    {384,161},{415,158},{427,183},
    {442,210},{442,258},{432,280},
    {423,300},{409,303},{396,282},
    {383,260},{372,249},{352,257},
    {338,263},{302,263},{288,257},
    {268,249},{257,260},{244,282},
    {231,303},{217,300},{208,280},
    {198,258},{198,210},{213,183},
    {218,170},{233,159},{245,159}
  };
  ink(dc,RGB(109,117,139),RGB(202,209,224));
  if(BeginPath(dc)) {
    MoveToEx(dc,245,159,NULL);
    PolyBezierTo(dc,p,(DWORD)(sizeof(p)/sizeof(p[0])));
    CloseFigure(dc);EndPath(dc);StrokeAndFillPath(dc);
  }
}
static void stick(HDC dc,int x,int y) {
  disc(dc,x,y,19,RGB(53,60,82),RGB(115,124,144));
  disc(dc,x,y,15,RGB(32,38,52),RGB(47,53,70));
  disc(dc,x,y,11,RGB(105,114,134),RGB(78,87,107));
  SetDCPenColor(dc,RGB(139,150,170));
  Arc(dc,x-9,y-9,x+10,y+10,x+8,y-6,x-7,y-8);
}
static void face_button(HDC dc,int x,int y,int glyph,COLORREF color) {
  HGDIOBJ old;
  HPEN pen;
  disc(dc,x,y,13,RGB(61,69,91),RGB(128,137,156));
  disc(dc,x,y,11,RGB(31,38,54),RGB(45,52,70));
  pen=CreatePen(PS_SOLID,2,color);
  if(!pen)return;
  old=SelectObject(dc,pen);
  if(glyph==0) {
    MoveToEx(dc,x,y-6,NULL);LineTo(dc,x-6,y+5);
    LineTo(dc,x+6,y+5);LineTo(dc,x,y-6);
  } else if(glyph==1) {
    HGDIOBJ b=SelectObject(dc,GetStockObject(NULL_BRUSH));
    Ellipse(dc,x-6,y-6,x+7,y+7);SelectObject(dc,b);
  } else if(glyph==2) {
    gline(dc,x-5,y-5,x+6,y+6);gline(dc,x+5,y-5,x-6,y+6);
  } else {
    HGDIOBJ b=SelectObject(dc,GetStockObject(NULL_BRUSH));
    Rectangle(dc,x-5,y-5,x+6,y+6);SelectObject(dc,b);
  }
  SelectObject(dc,old);DeleteObject(pen);
}
static void draw_gamepad(HDC dc) {
  const COLORREF dark=RGB(29,35,52),metal=RGB(162,174,196);
  HGDIOBJ p=SelectObject(dc,GetStockObject(DC_PEN));
  HGDIOBJ b=SelectObject(dc,GetStockObject(DC_BRUSH));
  /* Trigger row is physically distinct from the bumper row. */
  rounded(dc,218,133,280,157,12,dark,RGB(83,94,118));
  rounded(dc,360,126,422,157,12,dark,RGB(83,94,118));
  rounded(dc,222,137,276,149,8,RGB(112,125,149),RGB(125,137,161));
  rounded(dc,365,130,417,146,8,RGB(112,125,149),RGB(125,137,161));
  shell_path(dc,4,dark,RGB(37,43,64));
  shell_path(dc,0,dark,metal);
  face_path(dc);
  rounded(dc,218,151,280,174,12,dark,RGB(115,126,151));
  rounded(dc,360,151,422,174,12,dark,RGB(115,126,151));
  rounded(dc,223,154,275,163,7,RGB(160,174,196),RGB(156,169,192));
  rounded(dc,365,154,417,163,7,RGB(160,174,196),RGB(156,169,192));
  /* D-pad: four discrete beveled direction keys on a circular recess. */
  disc(dc,238,215,31,RGB(115,126,148),RGB(160,170,190));
  rounded(dc,229,187,247,209,5,dark,RGB(51,59,76));
  rounded(dc,229,221,247,243,5,dark,RGB(51,59,76));
  rounded(dc,210,206,232,224,5,dark,RGB(51,59,76));
  rounded(dc,244,206,266,224,5,dark,RGB(51,59,76));
  disc(dc,238,215,5,dark,RGB(72,82,101));
  SetDCPenColor(dc,RGB(156,169,192));
  gline(dc,233,195,238,190);gline(dc,238,190,243,195);
  gline(dc,233,235,238,240);gline(dc,238,240,243,235);
  gline(dc,218,210,213,215);gline(dc,213,215,218,220);
  gline(dc,258,210,263,215);gline(dc,263,215,258,220);
  /* Central pad and Share/Back key, right-hand menu key not mapped. */
  rounded(dc,290,178,350,217,10,RGB(90,101,126),RGB(123,137,161));
  rounded(dc,282,178,291,194,5,dark,RGB(52,62,82));
  rounded(dc,350,178,359,194,5,dark,RGB(52,62,82));
  stick(dc,282,260);stick(dc,348,260);
  disc(dc,390,225,43,RGB(115,126,148),RGB(160,170,190));
  face_button(dc,390,198,0,RGB(106,232,181)); /* Triangle / Y */
  face_button(dc,413,225,1,RGB(250,125,140)); /* Circle / B */
  face_button(dc,390,252,2,RGB(139,190,255)); /* Cross / A */
  face_button(dc,367,225,3,RGB(243,166,226)); /* Square / X */
  SelectObject(dc,b);SelectObject(dc,p);
}
/* Two-pass high-contrast leader lines. Endpoints are on their actual
 * semantic controls; route around neighboring controls, not through them. */
static void leader(HDC dc,const POINT *pts,int n) {
  HPEN shadow=CreatePen(PS_SOLID,3,RGB(29,30,66));
  HPEN light=CreatePen(PS_SOLID,1,RGB(218,216,252));
  HGDIOBJ old;
  if(!shadow||!light) {
    if(shadow)DeleteObject(shadow);
    if(light)DeleteObject(light);
    return;
  }
  old=SelectObject(dc,shadow);Polyline(dc,pts,n);
  SelectObject(dc,light);Polyline(dc,pts,n);
  SelectObject(dc,old);DeleteObject(shadow);DeleteObject(light);
  {
    HGDIOBJ p=SelectObject(dc,GetStockObject(DC_PEN));
    HGDIOBJ b=SelectObject(dc,GetStockObject(DC_BRUSH));
    disc(dc,pts[n-1].x,pts[n-1].y,3,RGB(38,41,75),RGB(239,237,255));
    SelectObject(dc,b);SelectObject(dc,p);
  }
}
static void draw_leaders(HDC dc) {
  static const POINT aim[]={{150,152},{198,152},{218,162},{249,162}};
  static const POINT swing[]={{480,120},{435,120},{403,136}};
  static const POINT zip[]={{480,168},{442,168},{410,162}};
  static const POINT move[]={{150,216},{201,216},{238,215}};
  static const POINT move_stick[]={{174,216},{174,260},{282,260}};
  static const POINT pause[]={{320,115},{320,137},{286,171},{286,186}};
  static const POINT web[]={{480,210},{451,210},{433,198},{390,198}};
  static const POINT kick[]={{480,244},{456,244},{437,225},{413,225}};
  static const POINT jump[]={{480,284},{441,284},{410,252},{390,252}};
  static const POINT punch[]={{333,368},{333,309},{326,291},{326,238},{352,225},{367,225}};
#define LEAD(a) leader(dc,a,(int)(sizeof(a)/sizeof(a[0])))
  LEAD(aim);LEAD(swing);LEAD(zip);LEAD(move);LEAD(move_stick);
  LEAD(pause);LEAD(web);LEAD(kick);LEAD(jump);LEAD(punch);
#undef LEAD
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
  draw_leaders(dc);
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
    guide_report("SDL3-GUIDE: drawn INSIDE original Joystick menu backbuffer; 4:3 sculpted pad and semantic leaders, no popup");
  return drawn?1:0;
}
