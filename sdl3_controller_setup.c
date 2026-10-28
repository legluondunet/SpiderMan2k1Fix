/* Experimental controller setup PREVIEW, launched from the real game's
 * original Joystick Configuration screen (a SHA-verified native callsite).
 *
 * The legacy game thread is stopped on the menu's stack while this modal
 * owner window has a normal Win32 message loop. The already-open SDL3
 * subsystem may be pumped on the SAME thread by WM_TIMER, not by a worker.
 *
 * No game-owned DirectInput action/keyboard table writes, no synthesized
 * inputs, no file persistence and no native menu assignment unlocking.
 * Keyboard access is mandatory, including cancellation and no-device case.
 * This preview must remain OFF by default until real hardware testing. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_controller_setup.h"
#include "sdl3_gamepad_probe.h"
#include "sdl3_controller_capture.h"

enum {
  SPIDER_UI_PAD=1001, SPIDER_UI_ACTION=1002,
  SPIDER_UI_CAPTURE=1003, SPIDER_UI_RESET=1004,
  SPIDER_UI_CLOSE=1005, SPIDER_UI_STATUS=1006,
  SPIDER_UI_TIMER=1007
};
typedef struct {
  HWND hwnd,owner,pads,actions,status;
  SpiderSDL3Capture capture;
  uint32_t instance;
  uint16_t vid,pid;
  unsigned slot;
  unsigned count;
  unsigned action;
} SpiderSDL3SetupUI;
static int setup_enabled=0,setup_open=0;
static void (*setup_report)(const char *)=NULL;
static const char *const button_names[SPIDER_SDL3_BUTTON_COUNT]={
 "South (A/Cross)","East (B/Circle)","West (X/Square)",
 "North (Y/Triangle)","Back/Share","Guide","Start",
 "Left Stick","Right Stick","LB/L1","RB/R1",
 "DPad Up","DPad Down","DPad Left","DPad Right",
 "Misc1","Right Paddle 1","Left Paddle 1",
 "Right Paddle 2","Left Paddle 2","Touchpad"
};
void SpiderSDL3ControllerSetup_Init(const char *ini,
                                    void (*logger)(const char *)) {
  setup_report=logger;
  setup_enabled=ini &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3ControllerSetup",0,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1;
  if(setup_enabled && setup_report)
    setup_report("SDL3-SETUP: experimental native Joystick menu entry ENABLED (model-only preview, no saved/gameplay changes)");
}
int SpiderSDL3ControllerSetup_Enabled(void) {return setup_enabled;}
static void status(SpiderSDL3SetupUI *ui,const char *message) {
  if(ui && ui->status)SetWindowTextA(ui->status,message);
}
static void binding_name(SpiderSDL3Binding b,char *out,size_t n) {
  if(!out || !n)return;
  if(b.kind==SPIDER_SDL3_BIND_NONE)snprintf(out,n,"(none)");
  else if(b.kind==SPIDER_SDL3_BIND_TRIGGER)
    snprintf(out,n,b.index==SPIDER_SDL3_AXIS_LT?"LT / L2":"RT / R2");
  else if(b.kind==SPIDER_SDL3_BIND_BUTTON &&
          b.index<SPIDER_SDL3_BUTTON_COUNT)
    snprintf(out,n,"%s",button_names[b.index]);
  else snprintf(out,n,"(unsupported)");
}
static SpiderSDL3ControllerProfile *current_profile(SpiderSDL3SetupUI *ui) {
  return ui && ui->instance?
    SpiderSDL3Probe_SetupProfile(ui->vid,ui->pid):NULL;
}
static void refresh_actions(SpiderSDL3SetupUI *ui) {
  SpiderSDL3ControllerProfile *p=current_profile(ui);
  unsigned i;
  if(!ui || !ui->actions)return;
  SendMessageA(ui->actions,LB_RESETCONTENT,0,0);
  if(!p)return;
  for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++) {
    char label[154],binding[85];
    binding_name(p->actions[i],binding,sizeof(binding));
    snprintf(label,sizeof(label),"%u. %-10s  :  %s",
      i+1,spider_sdl3_action_names[i],binding);
    SendMessageA(ui->actions,LB_ADDSTRING,0,(LPARAM)label);
  }
  if(ui->action>=SPIDER_SDL3_ACTION_COUNT)ui->action=0;
  SendMessageA(ui->actions,LB_SETCURSEL,ui->action,0);
}
static void select_slot(SpiderSDL3SetupUI *ui,unsigned slot) {
  char name[120],line[230];
  SpiderSDL3ControllerSample s;
  uint32_t id=0;
  uint16_t vid=0,pid=0;
  if(!ui)return;
  spider_sdl3_capture_reset(&ui->capture);
  ui->slot=slot;ui->instance=0;ui->vid=0;ui->pid=0;
  if(!SpiderSDL3Probe_SetupRead(slot,&id,&vid,&pid,name,sizeof(name),&s)) {
    refresh_actions(ui);
    status(ui,"No mapped controller selected. Connect a pad or use keyboard to Close.");
    return;
  }
  ui->instance=id;ui->vid=vid;ui->pid=pid;
  refresh_actions(ui);
  snprintf(line,sizeof(line),
    "%s (%04X:%04X) — model-only preview. Select an action then Capture.",
    name,(unsigned)vid,(unsigned)pid);
  status(ui,line);
}
static void refresh_pads(SpiderSDL3SetupUI *ui) {
  unsigned n,i,desired=0;
  uint32_t old_id;
  if(!ui || !ui->pads)return;
  old_id=ui->instance;
  n=SpiderSDL3Probe_SetupCount();
  SendMessageA(ui->pads,CB_RESETCONTENT,0,0);
  for(i=0;i<n;i++) {
    char name[120],line[180];
    SpiderSDL3ControllerSample sample;
    uint32_t id=0;
    uint16_t vid=0,pid=0;
    if(!SpiderSDL3Probe_SetupRead(i,&id,&vid,&pid,
                                 name,sizeof(name),&sample))
      snprintf(line,sizeof(line),"Unavailable controller slot %u",i+1);
    else {
      snprintf(line,sizeof(line),"%s [%04X:%04X] (id=%lu)",
        name,(unsigned)vid,(unsigned)pid,(unsigned long)id);
      if(old_id && id==old_id)desired=i;
    }
    SendMessageA(ui->pads,CB_ADDSTRING,0,(LPARAM)line);
  }
  ui->count=n;
  if(!n) {
    spider_sdl3_capture_reset(&ui->capture);
    ui->instance=0;ui->vid=0;ui->pid=0;
    refresh_actions(ui);
    status(ui,"No SDL3 mapped gamepad. Connect a pad; keyboard Close still works.");
    return;
  }
  SendMessageA(ui->pads,CB_SETCURSEL,desired,0);
  if(old_id && desired!=ui->slot)
    spider_sdl3_capture_reset(&ui->capture);
  select_slot(ui,desired);
}
static void sample_device(SpiderSDL3SetupUI *ui) {
  SpiderSDL3ControllerSample s;
  SpiderSDL3Binding candidate;
  uint32_t id=0;
  uint16_t vid=0,pid=0;
  int rc;
  char name[110],line[220],bind[85];
  SpiderSDL3ControllerProfile *p;
  if(!ui || !IsWindow(ui->hwnd))return;
  if(GetForegroundWindow()!=ui->hwnd) {
    spider_sdl3_capture_reset(&ui->capture);
    return;
  }
  if(!SpiderSDL3Probe_SetupPump()) {
    if(!ui->instance)
      status(ui,"SDL3 not initialized. Return to the game and try again.");
    return;
  }
  if(SpiderSDL3Probe_SetupCount()!=ui->count)refresh_pads(ui);
  if(!ui->instance)return;
  if(!SpiderSDL3Probe_SetupRead(ui->slot,&id,&vid,&pid,
                               name,sizeof(name),&s) ||
      id!=ui->instance || vid!=ui->vid || pid!=ui->pid) {
    refresh_pads(ui);
    return;
  }
  if(!ui->capture.active)return;
  rc=spider_sdl3_capture_feed(&ui->capture,&s,&candidate);
  if(rc!=1)return;
  p=current_profile(ui);
  if(!p || !spider_sdl3_profile_assign(p,ui->action,candidate)) {
    status(ui,"Binding rejected (reserved input or conflicting profile).");
    return;
  }
  binding_name(candidate,bind,sizeof(bind));
  snprintf(line,sizeof(line),
    "%s = %s (preview in memory ONLY; original game actions unchanged)",
    spider_sdl3_action_names[ui->action],bind);
  refresh_actions(ui);
  status(ui,line);
  if(setup_report)setup_report("SDL3-SETUP: semantic binding changed in memory (NOT game input, NOT saved)");
}
static LRESULT CALLBACK setup_wndproc(HWND hwnd,UINT msg,
                                      WPARAM wp,LPARAM lp) {
  SpiderSDL3SetupUI *ui=(SpiderSDL3SetupUI *)(uintptr_t)
    GetWindowLongPtrA(hwnd,GWLP_USERDATA);
  switch(msg) {
    case WM_CREATE: {
      CREATESTRUCTA *cs=(CREATESTRUCTA *)lp;
      HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
      HWND label;
      ui=(SpiderSDL3SetupUI *)cs->lpCreateParams;
      if(!ui)return -1;
      ui->hwnd=hwnd;
      SetWindowLongPtrA(hwnd,GWLP_USERDATA,(LONG_PTR)(uintptr_t)ui);
      label=CreateWindowExA(0,"STATIC",
        "SDL3 CONTROLLER SETUP — PREVIEW ONLY (gameplay controls unchanged)",
        WS_CHILD|WS_VISIBLE,18,16,690,28,hwnd,NULL,NULL,NULL);
      SendMessageA(label,WM_SETFONT,(WPARAM)font,TRUE);
      ui->pads=CreateWindowExA(0,"COMBOBOX","",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
        18,55,680,250,hwnd,(HMENU)(uintptr_t)SPIDER_UI_PAD,NULL,NULL);
      ui->actions=CreateWindowExA(WS_EX_CLIENTEDGE,"LISTBOX","",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|LBS_NOTIFY|WS_VSCROLL,
        18,99,430,275,hwnd,(HMENU)(uintptr_t)SPIDER_UI_ACTION,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Capture selected action",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP,
        465,113,230,36,hwnd,(HMENU)(uintptr_t)SPIDER_UI_CAPTURE,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Restore defaults (this model)",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP,
        465,160,230,36,hwnd,(HMENU)(uintptr_t)SPIDER_UI_RESET,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Close (no gameplay changes)",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP,
        465,207,230,36,hwnd,(HMENU)(uintptr_t)SPIDER_UI_CLOSE,NULL,NULL);
      ui->status=CreateWindowExA(0,"STATIC","",
        WS_CHILD|WS_VISIBLE,18,391,684,45,hwnd,
        (HMENU)(uintptr_t)SPIDER_UI_STATUS,NULL,NULL);
      SendMessageA(ui->pads,WM_SETFONT,(WPARAM)font,TRUE);
      SendMessageA(ui->actions,WM_SETFONT,(WPARAM)font,TRUE);
      SendMessageA(ui->status,WM_SETFONT,(WPARAM)font,TRUE);
      for(unsigned i=SPIDER_UI_CAPTURE;i<=SPIDER_UI_CLOSE;i++) {
        HWND button=GetDlgItem(hwnd,(int)i);
        if(button)SendMessageA(button,WM_SETFONT,(WPARAM)font,TRUE);
      }
      SetTimer(hwnd,SPIDER_UI_TIMER,35,NULL);
      return 0;
    }
    case WM_COMMAND:
      if(!ui)return 0;
      if(LOWORD(wp)==SPIDER_UI_PAD && HIWORD(wp)==CBN_SELCHANGE) {
        LRESULT idx=SendMessageA(ui->pads,CB_GETCURSEL,0,0);
        if(idx!=CB_ERR)select_slot(ui,(unsigned)idx);
      } else if(LOWORD(wp)==SPIDER_UI_ACTION &&
                HIWORD(wp)==LBN_SELCHANGE) {
        LRESULT idx=SendMessageA(ui->actions,LB_GETCURSEL,0,0);
        if(idx!=LB_ERR && idx<SPIDER_SDL3_ACTION_COUNT) {
          ui->action=(unsigned)idx;
          spider_sdl3_capture_reset(&ui->capture);
          status(ui,"Action selected. Choose Capture then release all controls.");
        }
      } else if(LOWORD(wp)==SPIDER_UI_CAPTURE && HIWORD(wp)==BN_CLICKED) {
        if(current_profile(ui)) {
          spider_sdl3_capture_start(&ui->capture);
          status(ui,"Release ALL buttons/triggers; then press ONE new control. Esc cancels.");
        }
      } else if(LOWORD(wp)==SPIDER_UI_RESET && HIWORD(wp)==BN_CLICKED) {
        SpiderSDL3ControllerProfile *p=current_profile(ui);
        spider_sdl3_capture_reset(&ui->capture);
        if(p && spider_sdl3_profile_default(p,ui->vid,ui->pid)) {
          refresh_actions(ui);
          status(ui,"This model restored in memory only. No game controls changed.");
          if(setup_report)setup_report("SDL3-SETUP: in-memory model restored to defaults (NOT saved)");
        }
      } else if(LOWORD(wp)==SPIDER_UI_CLOSE && HIWORD(wp)==BN_CLICKED)
        DestroyWindow(hwnd);
      return 0;
    case WM_TIMER:
      if(ui && wp==SPIDER_UI_TIMER)sample_device(ui);
      return 0;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      KillTimer(hwnd,SPIDER_UI_TIMER);
      return 0;
  }
  return DefWindowProcA(hwnd,msg,wp,lp);
}
void SpiderSDL3ControllerSetup_Open(void) {
  static const char *const cls="SpiderMan2k1FixSDL3ControllerSetup";
  static int registered=0;
  WNDCLASSA wc;
  SpiderSDL3SetupUI ui;
  HWND owner,window;
  DWORD pid=0;
  MSG msg;
  int parent_enabled,quit_code=0,has_quit=0;
  if(!setup_enabled || setup_open)return;
  owner=GetForegroundWindow();
  if(!owner || !IsWindow(owner) || !IsWindowVisible(owner) ||
     GetWindowThreadProcessId(owner,&pid)==0 ||
     pid!=GetCurrentProcessId())return;
  if(!SpiderSDL3Probe_SetupPump()) {
    if(setup_report)setup_report("SDL3-SETUP: preview refused, SDL3 not ready on original game thread");
    MessageBoxA(owner,
      "SDL3 is not ready. Please return to the game, then open the\n"
      "Joystick Configuration menu again. Your original controls remain unchanged.",
      "SpiderMan2k1Fix SDL3 controller setup",MB_OK|MB_ICONINFORMATION);
    return;
  }
  if(!registered) {
    memset(&wc,0,sizeof(wc));
    wc.lpfnWndProc=setup_wndproc;
    wc.hInstance=GetModuleHandleA(NULL);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName=cls;
    if(!RegisterClassA(&wc))return;
    registered=1;
  }
  memset(&ui,0,sizeof(ui));
  ui.owner=owner;
  setup_open=1;
  window=CreateWindowExA(WS_EX_DLGMODALFRAME,cls,
    "SpiderMan2k1Fix — SDL3 Controller Test (preview)",
    WS_CAPTION|WS_SYSMENU|WS_POPUP,
    CW_USEDEFAULT,CW_USEDEFAULT,730,493,
    owner,NULL,GetModuleHandleA(NULL),&ui);
  if(!window) {
    setup_open=0;return;
  }
  parent_enabled=IsWindowEnabled(owner);
  if(parent_enabled)EnableWindow(owner,FALSE);
  ShowWindow(window,SW_SHOW);
  UpdateWindow(window);
  SpiderSDL3Probe_SetupPump();
  refresh_pads(&ui);
  if(setup_report)setup_report("SDL3-SETUP: preview dialog opened from original Joystick menu; gameplay input paused on stack");
  while(IsWindow(window)) {
    BOOL ok=GetMessageA(&msg,NULL,0,0);
    if(ok<=0) {
      if(ok==0){has_quit=1;quit_code=(int)msg.wParam;}
      break;
    }
    if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE &&
       (msg.hwnd==window || IsChild(window,msg.hwnd))) {
      if(ui.capture.active) {
        spider_sdl3_capture_reset(&ui.capture);
        status(&ui,"Capture cancelled. Original game controls unchanged.");
      } else SendMessageA(window,WM_CLOSE,0,0);
      continue;
    }
    if(IsDialogMessageA(window,&msg))continue;
    TranslateMessage(&msg);DispatchMessageA(&msg);
  }
  if(IsWindow(window))DestroyWindow(window);
  if(parent_enabled && IsWindow(owner))EnableWindow(owner,TRUE);
  if(IsWindow(owner))SetForegroundWindow(owner);
  setup_open=0;
  if(has_quit)PostQuitMessage(quit_code);
  if(setup_report)setup_report("SDL3-SETUP: preview closed; original game menu/gameplay mappings untouched");
}
