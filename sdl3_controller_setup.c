/* Read-only controller guide for the fixed, PS1-inspired SDL3 controls.
 * The original retail Options -> Joystick menu redirects here through
 * the SHA-verified PCShell caller; Keyboard remains native and editable.
 * This is a drawn reference diagram, NOT a controller configuration modal:
 * no remapping, profiles, captures, calibration, file writes or SDL claims.
 * All gameplay ownership, neutral gating and focus protection remain in
 * the existing SDL3 probe/universal adapter on the original Flip thread. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "sdl3_controller_setup.h"
#include "sdl3_gamepad_probe.h"
#include "sdl3_universal_gameplay.h"
#include "sdl3_setup_focus_policy.h"
#include "gamepad_ui_profile.h"

enum { GUIDE_WIDTH=780, GUIDE_HEIGHT=535, GUIDE_CLOSE=1001 };
typedef struct {
  HWND owner,window,closing_foreground;
  unsigned closing_captured;
} SpiderSDL3Guide;
static int guide_enabled=0,guide_open=0;
static void (*guide_report)(const char *)=NULL;

void SpiderSDL3ControllerSetup_Init(const char *ini,
                                    void (*logger)(const char *)) {
  (void)ini;
  guide_report=logger;
  guide_enabled=1;
  if(guide_report)
    guide_report("SDL3-GUIDE: fixed PS1-inspired controls; read-only Joystick reference, no remapping or profile writes");
}
int SpiderSDL3ControllerSetup_Enabled(void) {return guide_enabled;}

static void guide_text(HDC dc,int x,int y,const char *str) {
  if(str)TextOutA(dc,x,y,str,(int)strlen(str));
}
static void guide_ring(HDC dc,int x,int y,int size,const char *glyph) {
  Ellipse(dc,x,y,x+size,y+size);
  guide_text(dc,x+size/2-8,y+size/2-7,glyph);
}
static void guide_paint(HDC dc,HWND hwnd) {
  RECT area,caption;
  HBRUSH bg=CreateSolidBrush(RGB(249,249,250));
  HBRUSH pad=CreateSolidBrush(RGB(205,207,215));
  HBRUSH button=CreateSolidBrush(RGB(232,233,237));
  HPEN outline=CreatePen(PS_SOLID,2,RGB(77,81,92));
  HGDIOBJ old_brush,old_pen,old_font;
  HFONT gui=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
  GetClientRect(hwnd,&area);
  FillRect(dc,&area,bg);
  SetBkMode(dc,TRANSPARENT);
  SetTextColor(dc,RGB(34,39,48));
  old_font=SelectObject(dc,gui);
  guide_text(dc,24,17,"SPIDER-MAN 2001  |  CONTROLLER REFERENCE");
  SetTextColor(dc,RGB(80,84,93));
  guide_text(dc,24,40,"Fixed PlayStation-style controls - same actions on DS4 and Xbox 360");

  old_pen=SelectObject(dc,outline);
  old_brush=SelectObject(dc,pad);
  /* Vector illustration: both grips, central shell and shoulder buttons.
   * Drawn with Win32 GDI, so no third-party image, asset or font is needed. */
  Ellipse(dc,183,146,365,338);
  Ellipse(dc,414,146,596,338);
  RoundRect(dc,244,135,537,301,47,41);
  RoundRect(dc,224,125,350,151,16,12);
  RoundRect(dc,429,125,555,151,16,12);
  SelectObject(dc,button);
  RoundRect(dc,329,167,376,190,14,11);
  RoundRect(dc,404,167,451,190,14,11);
  /* Directional cross on the left and two thumbsticks. */
  RoundRect(dc,276,198,302,272,7,7);
  RoundRect(dc,251,223,327,246,7,7);
  Ellipse(dc,314,265,355,306);
  Ellipse(dc,422,265,463,306);
  /* Sony face cluster (glyph names are also printed in the legend). */
  guide_ring(dc,474,179,27,"T");
  guide_ring(dc,503,206,27,"O");
  guide_ring(dc,474,234,27,"X");
  guide_ring(dc,446,206,27,"S");

  SelectObject(dc,old_brush);
  SelectObject(dc,old_pen);
  SetTextColor(dc,RGB(34,39,48));
  guide_text(dc,46,112,"L1 / LB  -  Aim");
  guide_text(dc,541,112,"R1 / RB  -  Zipline");
  guide_text(dc,46,166,"Left stick / D-pad");
  guide_text(dc,46,187,"Move / navigate");
  guide_text(dc,554,164,"Right trigger");
  guide_text(dc,554,184,"R2 / RT  -  Swing");
  guide_text(dc,300,319,"Share / Back  -  Pause");
  guide_text(dc,300,338,"Start  -  Select pad*");

  caption.left=25;caption.top=370;
  caption.right=750;caption.bottom=458;
  FillRect(dc,&caption,(HBRUSH)(COLOR_3DFACE+1));
  SetTextColor(dc,RGB(33,39,48));
  guide_text(dc,44,378,"CROSS  / A       Jump");
  guide_text(dc,44,399,"CIRCLE / B       Kick");
  guide_text(dc,44,420,"SQUARE / X      Punch");
  guide_text(dc,44,441,"TRIANGLE / Y    Web");
  guide_text(dc,401,378,"L1 / LB       Aim");
  guide_text(dc,401,399,"R1 / RB       Zipline");
  guide_text(dc,401,420,"R2 / RT       Swing");
  guide_text(dc,401,441,"Share / Back   Pause");
  SetTextColor(dc,RGB(88,91,100));
  guide_text(dc,25,465,
    "*Only needed when several controllers are connected. With one pad: automatic activation after neutral.");
  guide_text(dc,25,483,
    "Menu: Cross/A confirms, Circle/B goes back. Keyboard bindings remain editable separately.");
  SelectObject(dc,old_font);
  DeleteObject(outline);
  DeleteObject(button);
  DeleteObject(pad);
  DeleteObject(bg);
}
static LRESULT CALLBACK guide_wndproc(HWND hwnd,UINT msg,
                                       WPARAM wp,LPARAM lp) {
  SpiderSDL3Guide *ui=(SpiderSDL3Guide *)(uintptr_t)
    GetWindowLongPtrA(hwnd,GWLP_USERDATA);
  switch(msg) {
    case WM_CREATE: {
      CREATESTRUCTA *cs=(CREATESTRUCTA *)lp;
      HWND close;
      ui=(SpiderSDL3Guide *)cs->lpCreateParams;
      if(!ui)return -1;
      ui->window=hwnd;
      SetWindowLongPtrA(hwnd,GWLP_USERDATA,(LONG_PTR)(uintptr_t)ui);
      close=CreateWindowExA(0,"BUTTON","Close / Back to game",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
        565,497,185,28,hwnd,(HMENU)(uintptr_t)GUIDE_CLOSE,
        GetModuleHandleA(NULL),NULL);
      if(close) {
        SendMessageA(close,WM_SETFONT,
          (WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
        SetFocus(close);
      }
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc=BeginPaint(hwnd,&ps);
      guide_paint(dc,hwnd);
      EndPaint(hwnd,&ps);
      return 0;
    }
    case WM_COMMAND:
      if(LOWORD(wp)==GUIDE_CLOSE && HIWORD(wp)==BN_CLICKED) {
        PostMessageA(hwnd,WM_CLOSE,0,0);
        return 0;
      }
      break;
    case WM_KEYDOWN:
      if(wp==VK_ESCAPE || wp==VK_RETURN) {
        PostMessageA(hwnd,WM_CLOSE,0,0);
        return 0;
      }
      break;
    case WM_CLOSE:
      if(ui) {
        ui->closing_foreground=GetForegroundWindow();
        ui->closing_captured=1;
      }
      DestroyWindow(hwnd);
      return 0;
    case WM_NCDESTROY:
      SetWindowLongPtrA(hwnd,GWLP_USERDATA,0);
      return 0;
  }
  return DefWindowProcA(hwnd,msg,wp,lp);
}
void SpiderSDL3ControllerSetup_Open(void) {
  static const char *const cls="SpiderMan2k1FixSDL3ControllerGuide";
  static int registered=0;
  WNDCLASSA wc;
  SpiderSDL3Guide ui;
  HWND owner,window,previous,final_foreground;
  RECT owner_rect;
  DWORD pid=0;
  MSG msg;
  int parent_enabled,has_quit=0,quit_code=0;
  int restore_allowed,preclose_owned,previous_owned;
  if(!guide_enabled || guide_open)return;
  owner=GetForegroundWindow();
  if(!owner || !IsWindow(owner) || !IsWindowVisible(owner) ||
     !GetWindowThreadProcessId(owner,&pid) ||
     pid!=GetCurrentProcessId())return;
  /* Same safe modal handoff as the old SDL3 config window: release
   * synthetic keys and return the native DI pointer before blocking
   * the original game thread. No SDL3 input is pumped in this guide. */
  SpiderSDL3Universal_BeforeModal(owner);
  if(!registered) {
    memset(&wc,0,sizeof(wc));
    wc.lpfnWndProc=guide_wndproc;
    wc.hInstance=GetModuleHandleA(NULL);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName=cls;
    if(!RegisterClassA(&wc))return;
    registered=1;
  }
  memset(&ui,0,sizeof(ui));
  ui.owner=owner;
  GetWindowRect(owner,&owner_rect);
  guide_open=1;
  window=CreateWindowExA(WS_EX_DLGMODALFRAME,cls,
    "SpiderMan2k1Fix - Gamepad Controls",
    WS_CAPTION|WS_SYSMENU|WS_POPUP,
    owner_rect.left+((owner_rect.right-owner_rect.left)-GUIDE_WIDTH)/2,
    owner_rect.top+((owner_rect.bottom-owner_rect.top)-GUIDE_HEIGHT)/2,
    GUIDE_WIDTH,GUIDE_HEIGHT,
    owner,NULL,GetModuleHandleA(NULL),&ui);
  if(!window) {guide_open=0;return;}
  parent_enabled=IsWindowEnabled(owner)!=0;
  if(parent_enabled)EnableWindow(owner,FALSE);
  ShowWindow(window,SW_SHOW);
  UpdateWindow(window);
  if(guide_report)
    guide_report("SDL3-GUIDE: read-only PS1-style diagram opened from Options > Joystick; no controller selection or binding write");
  while(IsWindow(window)) {
    BOOL ok=GetMessageA(&msg,NULL,0,0);
    if(ok<=0) {
      if(ok==0) {has_quit=1;quit_code=(int)msg.wParam;}
      break;
    }
    if(IsDialogMessageA(window,&msg))continue;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  if(IsWindow(window))DestroyWindow(window);
  previous=GetForegroundWindow();
  preclose_owned=ui.closing_foreground==owner ||
                 ui.closing_foreground==window;
  previous_owned=previous==owner || previous==window;
  restore_allowed=spider_sdl3_setup_restore_focus(
    has_quit,ui.closing_captured,preclose_owned,previous_owned,
    ui.closing_foreground==NULL,previous==NULL);
  if(parent_enabled && IsWindow(owner))EnableWindow(owner,TRUE);
  if(restore_allowed && IsWindow(owner) && IsWindowVisible(owner)) {
    SetActiveWindow(owner);
    if(SetForegroundWindow(owner) && GetForegroundWindow()==owner)
      SetFocus(owner);
  }
  final_foreground=GetForegroundWindow();
  guide_open=0;
  /* Disarm any physical Start held during the modal; with one mapped
   * pad, the automatic selector still waits for a neutral game Flip.
   * This read-only guide can NEVER claim another controller. */
  SpiderSDL3Probe_SetupModalReturn(0,0,0);
  if(has_quit)PostQuitMessage(quit_code);
  else if(IsWindow(owner))SpiderGamepadUI_AfterControllerSetup(owner);
  if(guide_report) {
    char note[172];
    snprintf(note,sizeof(note),
      "SDL3-GUIDE: closed; focus restored=%d foreground-owned=%d; fixed mappings retained",
      restore_allowed,final_foreground==owner);
    guide_report(note);
  }
}
