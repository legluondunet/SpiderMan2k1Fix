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
#include <stdlib.h>
#include "sdl3_controller_setup.h"
#include "sdl3_gamepad_probe.h"

static void (*guide_report)(const char *)=NULL;
static int guide_enabled=0;
static int guide_requested=0,guide_frame=0,guide_reported=0;
static LONG guide_draws=0;
/* SDL_GamepadButtonLabel ABI (SDL 3.2+): A B X Y Cross Circle Square
 * Triangle = 1..8. Zero deliberately uses a positional prompt. */
static unsigned guide_labels[4]={0},guide_family=0;
static uint32_t guide_instance=0;
unsigned SpiderSDL3ControllerSetup_GuideFamily(void) {return guide_family;}
const char *SpiderSDL3ControllerSetup_GuideButton(unsigned position) {
  static const char *const names[]={"","A","B","X","Y","CROSS","CIRCLE","SQUARE","TRIANGLE"};
  static const char *const positions[]={"SOUTH","EAST","WEST","NORTH"};
  if(position>=4)return "";
  return guide_labels[position] && guide_labels[position]<=8?
    names[guide_labels[position]]:positions[position];
}
void SpiderSDL3ControllerSetup_RefreshGuide(void) {
  static const unsigned ps[4]={5,6,7,8},xb[4]={1,2,3,4};
  unsigned labels[4]={0},family=0,i;
  uint16_t vid=0;
  uint32_t instance=0;
  if(SpiderSDL3Probe_SelectedGuideLabels(labels,&vid,&instance)) {
    if(labels[0]==5 && labels[1]==6 && labels[2]==7 && labels[3]==8)family=1;
    else if(labels[0]==1 && labels[1]==2 && labels[2]==3 && labels[3]==4)family=2;
    else if(labels[0]==2 && labels[1]==1 && labels[2]==4 && labels[3]==3)family=3;
    else if(!labels[0] && !labels[1] && !labels[2] && !labels[3]) {
      if(vid==0x054Cu) {family=1;memcpy(labels,ps,sizeof(labels));}
      else if(vid==0x045Eu) {family=2;memcpy(labels,xb,sizeof(labels));}
    }
  }
  if(guide_instance!=instance || guide_family!=family ||
     memcmp(guide_labels,labels,sizeof(labels))) {
    char line[160];
    snprintf(line,sizeof(line),"SDL3-GUIDE: display owner=%lu family=%u face=%u,%u,%u,%u",
      (unsigned long)instance,family,labels[0],labels[1],labels[2],labels[3]);
    if(guide_report)guide_report(line);
  }
  guide_instance=instance;guide_family=family;
  for(i=0;i<4;i++)guide_labels[i]=labels[i];
}

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
  guide_instance=0;guide_family=0;memset(guide_labels,0,sizeof(guide_labels));
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
/* Original vector letter/symbol strokes, independent of retail font. */
static void face_button(HDC dc,int x,int y,unsigned pos) {
  static const COLORREF colors[]={RGB(196,203,216),RGB(111,225,139),
    RGB(250,113,127),RGB(125,179,255),RGB(255,214,106),
    RGB(139,190,255),RGB(250,125,140),RGB(243,166,226),RGB(106,232,181)};
  unsigned glyph=guide_labels[pos];
  HGDIOBJ old;
  HPEN pen;
  disc(dc,x,y,12,RGB(52,60,80),RGB(135,146,165));
  disc(dc,x,y,10,RGB(29,35,52),RGB(44,51,69));
  pen=CreatePen(PS_SOLID,2,colors[glyph<=8?glyph:0]);
  if(!pen)return;
  old=SelectObject(dc,pen);
  switch(glyph) {
    case 1: /* A */
      gline(dc,x-5,y+6,x,y-6);gline(dc,x,y-6,x+5,y+6);
      gline(dc,x-3,y+2,x+4,y+2);break;
    case 2: /* B */
      gline(dc,x-4,y-6,x-4,y+6);
      gline(dc,x-4,y-6,x+3,y-6);gline(dc,x+3,y-6,x+5,y-3);
      gline(dc,x+5,y-3,x+1,y);gline(dc,x-4,y,x+1,y);
      gline(dc,x+1,y,x+5,y+3);gline(dc,x+5,y+3,x+3,y+6);
      gline(dc,x+3,y+6,x-4,y+6);break;
    case 3: case 5: /* X / Cross */
      gline(dc,x-5,y-5,x+6,y+6);gline(dc,x+5,y-5,x-6,y+6);break;
    case 4: /* Y */
      gline(dc,x-5,y-6,x,y);gline(dc,x+5,y-6,x,y);
      gline(dc,x,y,x,y+6);break;
    case 6: case 7: {
      HGDIOBJ b=SelectObject(dc,GetStockObject(NULL_BRUSH));
      if(glyph==6)Ellipse(dc,x-6,y-6,x+7,y+7);
      else Rectangle(dc,x-5,y-5,x+6,y+6);
      SelectObject(dc,b);break;
    }
    case 8:
      MoveToEx(dc,x,y-6,NULL);LineTo(dc,x-6,y+5);
      LineTo(dc,x+6,y+5);LineTo(dc,x,y-6);break;
    default: /* Unknown: indicate physical position, never invent A/B. */
      if(pos==0)gline(dc,x-4,y+5,x+5,y+5);
      else if(pos==1)gline(dc,x+5,y-4,x+5,y+5);
      else if(pos==2)gline(dc,x-5,y-4,x-5,y+5);
      else gline(dc,x-4,y-5,x+5,y-5);
      break;
  }
  SelectObject(dc,old);DeleteObject(pen);
}
static void dpad(HDC dc,int x,int y,int r) {
  const COLORREF dark=RGB(29,35,52),fill=RGB(53,61,78);
  int t=r*3/10,h=r*9/10;
  disc(dc,x,y,r,RGB(115,126,148),RGB(160,170,190));
  rounded(dc,x-t,y-h,x+t,y+h,5,dark,fill);
  rounded(dc,x-h,y-t,x+h,y+t,5,dark,fill);
  /* Seamless raised cross, plus restrained bevel highlights. */
  ink(dc,fill,fill);Rectangle(dc,x-t+1,y-t+1,x+t,y+t);
  SetDCPenColor(dc,RGB(144,157,181));
  gline(dc,x-t+3,y-h+7,x,y-h+3);gline(dc,x,y-h+3,x+t-3,y-h+7);
  gline(dc,x-t+3,y+h-7,x,y+h-3);gline(dc,x,y+h-3,x+t-3,y+h-7);
  gline(dc,x-h+7,y-t+3,x-h+3,y);gline(dc,x-h+3,y,x-h+7,y+t-3);
  gline(dc,x+h-7,y-t+3,x+h-3,y);gline(dc,x+h-3,y,x+h-7,y+t-3);
}
static void draw_gamepad(HDC dc) {
  const COLORREF dark=RGB(29,35,52),metal=RGB(162,174,196);
  HGDIOBJ p=SelectObject(dc,GetStockObject(DC_PEN));
  HGDIOBJ b=SelectObject(dc,GetStockObject(DC_BRUSH));
  /* Trigger row is physically distinct from the bumper row. */
  rounded(dc,218,126,280,157,12,dark,RGB(83,94,118));
  rounded(dc,360,126,422,157,12,dark,RGB(83,94,118));
  rounded(dc,222,130,276,146,8,RGB(112,125,149),RGB(125,137,161));
  rounded(dc,365,130,417,146,8,RGB(112,125,149),RGB(125,137,161));
  shell_path(dc,4,dark,RGB(37,43,64));
  shell_path(dc,0,dark,metal);
  face_path(dc);
  rounded(dc,218,151,280,174,12,dark,RGB(115,126,151));
  rounded(dc,360,151,422,174,12,dark,RGB(115,126,151));
  rounded(dc,223,154,275,163,7,RGB(160,174,196),RGB(156,169,192));
  rounded(dc,365,154,417,163,7,RGB(160,174,196),RGB(156,169,192));
  /* The Xbox/Nintendo family uses the asymmetric stick/D-pad arrangement.
   * Neutral artwork is not a claim to depict an exact hardware model. */
  if(guide_family==2 || guide_family==3) {
    stick(dc,238,215);dpad(dc,279,247,22);
    rounded(dc,282,179,299,190,8,dark,RGB(58,67,86));
    rounded(dc,341,179,358,190,8,dark,RGB(58,67,86));
    disc(dc,320,183,13,dark,RGB(101,113,134));
    disc(dc,320,183,9,RGB(150,164,186),RGB(64,74,94));
  } else {
    dpad(dc,238,215,29);stick(dc,282,260);
    if(guide_family==1)
      rounded(dc,292,178,348,211,10,RGB(90,101,126),RGB(123,137,161));
    rounded(dc,282,178,291,194,5,dark,RGB(52,62,82));
    rounded(dc,350,178,359,194,5,dark,RGB(52,62,82));
  }
  stick(dc,348,260);
  disc(dc,390,225,36,RGB(115,126,148),RGB(160,170,190));
  face_button(dc,390,202,3); /* NORTH: Web */
  face_button(dc,413,225,1); /* EAST: Kick */
  face_button(dc,390,248,0); /* SOUTH: Jump */
  face_button(dc,367,225,2); /* WEST: Punch */
  SelectObject(dc,b);SelectObject(dc,p);
}
/* Two-pass high-contrast leader lines. Endpoints are on their actual
 * semantic controls; route around neighboring controls, not through them. */
static void leader(HDC dc,const POINT *pts,int n) {
  HPEN shadow=CreatePen(PS_SOLID,2,RGB(29,30,66));
  HPEN light=CreatePen(PS_SOLID,1,RGB(218,216,252));
  HGDIOBJ old;
  if(!shadow||!light) {
    if(shadow)DeleteObject(shadow);
    if(light)DeleteObject(light);
    return;
  }
  /* Rounded elbows via a short cubic at each bend. The last 1-2
   * pixels of the curve never cover a glyph. */
  old=SelectObject(dc,shadow);
  for(int pass=0;pass<2;pass++) {
    if(pass)SelectObject(dc,light);
    MoveToEx(dc,pts[0].x,pts[0].y,NULL);
    for(int i=1;i<n-1;i++) {
      POINT a=pts[i-1],b=pts[i],c=pts[i+1],q[3];
      int dx=b.x-a.x,dy=b.y-a.y,ex=c.x-b.x,ey=c.y-b.y;
      int m=abs(dx)>abs(dy)?abs(dx):abs(dy);
      int k=abs(ex)>abs(ey)?abs(ex):abs(ey);
      int r=m/2<k/2?m/2:k/2;if(r>5)r=5;
      if(!m||!k){LineTo(dc,b.x,b.y);continue;}
      LineTo(dc,b.x-dx*r/m,b.y-dy*r/m);
      q[0]=b;q[1]=b;q[2].x=b.x+ex*r/k;q[2].y=b.y+ey*r/k;
      PolyBezierTo(dc,q,3);
    }
    LineTo(dc,pts[n-1].x,pts[n-1].y);
  }
  SelectObject(dc,old);DeleteObject(shadow);DeleteObject(light);
  {
    HGDIOBJ p=SelectObject(dc,GetStockObject(DC_PEN));
    HGDIOBJ b=SelectObject(dc,GetStockObject(DC_BRUSH));
    disc(dc,pts[n-1].x,pts[n-1].y,2,RGB(38,41,75),RGB(239,237,255));
    SelectObject(dc,b);SelectObject(dc,p);
  }
}
static void draw_leaders(HDC dc) {
  /* Four action labels occupy the same right column, in route order.
   * Punch exits north of the diamond, avoiding all three other glyphs. */
  static const POINT aim[]={{150,152},{205,152},{225,162}};
  static const POINT swing[]={{480,119},{425,119},{408,136}};
  static const POINT zip[]={{480,151},{435,151},{418,162}};
  static const POINT move[]={{150,215},{210,215}};
  static const POINT move_stick[]={{150,260},{264,260}};
  static const POINT xbox_stick[]={{150,215},{220,215}};
  static const POINT xbox_dpad[]={{150,260},{245,260},{258,247}};
  static const POINT pause[]={{286,115},{286,178}};
  static const POINT xbox_pause[]={{290,115},{290,179}};
  static const POINT punch[]={{480,185},{383,185},{367,201},{367,213}};
  static const POINT web[]={{480,218},{420,218},{401,202}};
  static const POINT kick[]={{480,249},{448,249},{425,225}};
  static const POINT jump[]={{480,280},{428,280},{400,257}};
#define LEAD(a) leader(dc,a,(int)(sizeof(a)/sizeof(a[0])))
  LEAD(aim);LEAD(swing);LEAD(zip);
  if(guide_family==2 || guide_family==3) {
    LEAD(xbox_stick);LEAD(xbox_dpad);LEAD(xbox_pause);
  } else {LEAD(move);LEAD(move_stick);LEAD(pause);}
  LEAD(punch);LEAD(web);LEAD(kick);LEAD(jump);
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
    guide_report("SDL3-GUIDE: drawn INSIDE original Joystick menu backbuffer; 4:3 active-controller glyphs and rounded leaders, no popup");
  return drawn?1:0;
}
