/* Keyboard-first SDL3 raw joystick calibration; NO game-owned DirectInput
 * calls and NO native game input injection. Runs nested on the same original
 * game/SDL3 thread while Joystick Configuration's modal is open.
 * Esc cancels; Enter starts/restarts; S skips an optional control.
 * SDL mapping file is saved only after all required inputs were captured.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdl3_gamepad_probe.h"
#include "sdl3_raw_mapping.h"
#include "sdl3_raw_wizard.h"

enum { RAW_PAD=2001,RAW_BEGIN=2002,RAW_SKIP=2003,
       RAW_CLOSE=2004,RAW_STATUS=2005,RAW_TIMER=2006 };
typedef struct {
  HWND window,owner,pads,status;
  uint32_t ids[16],active_id;
  unsigned raw_count,step,armed,done;
  char guid[33],name[128];
  SpiderSDL3RawMapping mapping;
  SpiderSDL3RawSample neutral;
  unsigned still;
  void (*log)(const char *);
} SpiderRawWizard;
static const char *const prompts[SPIDER_RAW_MAP_COUNT]={
  "A / Cross / bottom face button",
  "B / Circle / right face button",
  "X / Square / left face button",
  "Y / Triangle / upper face button",
  "Back / Share", "Start / Options",
  "Left shoulder L1 / LB", "Right shoulder R1 / RB",
  "Press left stick (L3)", "Press right stick (R3)",
  "D-pad UP", "D-pad DOWN", "D-pad LEFT", "D-pad RIGHT",
  "Move LEFT stick RIGHT", "Move LEFT stick DOWN",
  "Move RIGHT stick RIGHT", "Move RIGHT stick DOWN",
  "Left trigger L2 / LT", "Right trigger R2 / RT"
};
static int axis_step(unsigned step) {
  return (step>=SPIDER_RAW_MAP_LEFTX && step<=SPIDER_RAW_MAP_RIGHTY);
}
static int required_step(unsigned step) {
  return step==SPIDER_RAW_MAP_A || step==SPIDER_RAW_MAP_B ||
         step==SPIDER_RAW_MAP_LEFTX || step==SPIDER_RAW_MAP_LEFTY;
}
static void wizard_text(SpiderRawWizard *w,const char *text) {
  if(w && w->status)SetWindowTextA(w->status,text);
}
static void wizard_prompt(SpiderRawWizard *w) {
  char line[380];
  if(!w)return;
  if(w->done)return;
  if(w->step>=SPIDER_RAW_MAP_COUNT)return;
  snprintf(line,sizeof(line),
    "Step %u / %u: %s\r\n"
    "Release every control, then activate ONLY this one.\r\n"
    "S: skip optional step   Esc: cancel   Enter: restart",
    w->step+1,SPIDER_RAW_MAP_COUNT,prompts[w->step]);
  wizard_text(w,line);
}
static int wizard_neutral(const SpiderSDL3RawSample *s) {
  unsigned i;
  if(!s || s->buttons)return 0;
  for(i=0;i<s->hat_count;i++)if(s->hats[i])return 0;
  return 1;
}
static void wizard_finish(SpiderRawWizard *w);
static void wizard_next(SpiderRawWizard *w,SpiderSDL3RawInput input) {
  if(!w || w->step>=SPIDER_RAW_MAP_COUNT)return;
  w->mapping.input[w->step]=input;
  w->step++;
  w->armed=0;w->still=0;
  wizard_prompt(w);
  if(w->step==SPIDER_RAW_MAP_COUNT)wizard_finish(w);
}
/* Captures a single deliberate input. At most one qualifying candidate is
 * accepted; ambiguous simultaneous inputs force a fresh neutral cycle.
 * Raw axes use the physically sampled baseline, so idle triggers at -32768
 * are not mistaken for movements. */
static void wizard_tick(SpiderRawWizard *w) {
  SpiderSDL3RawSample s;
  SpiderSDL3RawInput found={0,0,0};
  unsigned i,n=0;
  int axes_only;
  if(!w || w->done || !w->active_id)return;
  if(GetForegroundWindow()!=w->window) {w->armed=0;w->still=0;return;}
  if(!SpiderSDL3Probe_SetupPump() || !SpiderSDL3Probe_RawRead(&s)) {
    wizard_text(w,"Controller disconnected or unavailable. Press Enter to retry.");
    SpiderSDL3Probe_RawEnd();w->active_id=0;w->armed=0;
    return;
  }
  if(w->step>=SPIDER_RAW_MAP_COUNT)return;
  if(!w->armed) {
    if(!wizard_neutral(&s)) {w->still=0;return;}
    if(++w->still>=2u) {
      w->neutral=s;
      w->armed=1;w->still=0;
    }
    return;
  }
  axes_only=axis_step(w->step);
  if(!axes_only) {
    for(i=0;i<s.button_count;i++)
      if(s.buttons&(1u<<i)) {
        found.kind=SPIDER_RAW_BUTTON;
        found.index=i;found.hat_value=0;
        n++;
      }
    /* Only D-pad steps allow a raw hat; other steps require buttons.
     * Trigger steps may also capture an axis below. */
    if(w->step>=SPIDER_RAW_MAP_DPUP && w->step<=SPIDER_RAW_MAP_DPRIGHT) {
      for(i=0;i<s.hat_count;i++) {
        unsigned hat=s.hats[i];
        if(hat==1u || hat==2u || hat==4u || hat==8u) {
          found.kind=SPIDER_RAW_HAT;
          found.index=i;found.hat_value=hat;
          n++;
        }
      }
    }
  }
  if(axes_only || w->step>=SPIDER_RAW_MAP_LEFTTRIGGER) {
    for(i=0;i<s.axis_count;i++) {
      int delta=(int)s.axes[i]-(int)w->neutral.axes[i];
      if(delta>17000 || delta< -17000) {
        found.kind=SPIDER_RAW_AXIS;
        found.index=i;found.hat_value=0;
        n++;
      }
    }
  }
  if(n>1u) {w->armed=0;w->still=0;return;}
  if(n==1u) {
    wizard_next(w,found);

  }
}
static void wizard_finish(SpiderRawWizard *w) {
  char mapping[1024],line[410];
  if(!w || w->step!=SPIDER_RAW_MAP_COUNT)return;
  if(!spider_sdl3_raw_build(w->guid,w->name,&w->mapping,
               mapping,sizeof(mapping))) {
    wizard_text(w,"Mapping incomplete or unsupported. Press Enter to restart.");
    w->active_id=0;
    SpiderSDL3Probe_RawEnd();
    return;
  }
  w->done=1;
  if(SpiderSDL3Probe_RawRegisterAndSave(mapping,w->guid)) {
    snprintf(line,sizeof(line),
    "SDL3 CALIBRATION SAVED for %s.\r\n"
    "Close this window, then reopen SDL3 Setup: the device should appear as a mapped gamepad.\r\n"
    "IMPORTANT: this does NOT yet guarantee Spider-Man 3D gameplay.",
    w->name);
    wizard_text(w,line);
    if(w->log)w->log("SDL3-RAW-WIZARD: completed and persisted SDL3 normalized mapping; game 3D bridge unchanged");
  } else {
    wizard_text(w,
    "SDL3 mapping application or save FAILED. No persistent calibration confirmed.\r\n"
    "Check the log for the precise error. Enter restarts; Esc closes.");
    w->done=0;w->active_id=0;
  }
}
static void wizard_start(SpiderRawWizard *w) {
  LRESULT selected;
  unsigned index;
  char text[320];
  if(!w)return;
  SpiderSDL3Probe_RawEnd();
  memset(&w->mapping,0,sizeof(w->mapping));
  w->step=0;w->done=0;w->armed=0;w->still=0;w->active_id=0;
  selected=SendMessageA(w->pads,CB_GETCURSEL,0,0);
  if(selected==CB_ERR || (unsigned)selected>=w->raw_count) {
    wizard_text(w,"No unmapped raw joystick. SDL3 may already recognize your controller.");
    return;
  }
  index=(unsigned)selected;
  if(!SpiderSDL3Probe_RawBegin(w->ids[index],w->guid,w->name,sizeof(w->name))) {
    wizard_text(w,"SDL3 could not open this raw joystick. Check connection and try again.");
    return;
  }
  w->active_id=w->ids[index];
  snprintf(text,sizeof(text),
    "SDL3-RAW-WIZARD: opened id=%lu GUID=%s name=%.100s (keyboard-only)",
    (unsigned long)w->active_id,w->guid,w->name);
  if(w->log)w->log(text);
  wizard_prompt(w);
}
static void wizard_refresh(SpiderRawWizard *w) {
  unsigned i;
  char line[140];
  if(!w || !w->pads)return;
  w->raw_count=SpiderSDL3Probe_RawList(w->ids,16u);
  if(w->raw_count>16u)w->raw_count=16u;
  SendMessageA(w->pads,CB_RESETCONTENT,0,0);
  for(i=0;i<w->raw_count;i++) {
    snprintf(line,sizeof(line),"Unmapped joystick %u (SDL id=%lu)",
             i+1,(unsigned long)w->ids[i]);
    SendMessageA(w->pads,CB_ADDSTRING,0,(LPARAM)line);
  }
  if(w->raw_count)SendMessageA(w->pads,CB_SETCURSEL,0,0);
  else wizard_text(w,"No unmapped raw joystick found. SDL3 may already recognize your controller.");
}
static LRESULT CALLBACK wizard_wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  SpiderRawWizard *w=(SpiderRawWizard *)(uintptr_t)
       GetWindowLongPtrA(hwnd,GWLP_USERDATA);
  switch(msg) {
    case WM_CREATE: {
      CREATESTRUCTA *cs=(CREATESTRUCTA *)lp;
      HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
      HWND label,button;
      w=(SpiderRawWizard *)cs->lpCreateParams;
      if(!w)return -1;
      w->window=hwnd;
      SetWindowLongPtrA(hwnd,GWLP_USERDATA,(LONG_PTR)(uintptr_t)w);
      label=CreateWindowExA(0,"STATIC",
        "Raw joystick recognition - SDL3 only (gameplay unchanged)",
        WS_CHILD|WS_VISIBLE,20,16,620,22,hwnd,NULL,NULL,NULL);
      SendMessageA(label,WM_SETFONT,(WPARAM)font,TRUE);
      w->pads=CreateWindowExA(0,"COMBOBOX","",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
        20,49,610,220,hwnd,(HMENU)(uintptr_t)RAW_PAD,NULL,NULL);
      w->status=CreateWindowExA(0,"STATIC",
        "Select an unmapped joystick, then press Enter or Start.",
        WS_CHILD|WS_VISIBLE,20,105,610,100,hwnd,
        (HMENU)(uintptr_t)RAW_STATUS,NULL,NULL);
      for(unsigned i=RAW_BEGIN;i<=RAW_CLOSE;i++) {
        const char *caption=i==RAW_BEGIN?"Start / restart (Enter)":
          i==RAW_SKIP?"Skip optional input (S)":"Cancel / close (Esc)";
        button=CreateWindowExA(0,"BUTTON",caption,
          WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
          20+(int)(i-RAW_BEGIN)*210,220,200,36,hwnd,
          (HMENU)(uintptr_t)i,NULL,NULL);
        SendMessageA(button,WM_SETFONT,(WPARAM)font,TRUE);
      }
      label=CreateWindowExA(0,"STATIC",
        "The game keyboard stays available. This is an SDL hardware mapping test,\r\n"
        "NOT a claim of universal Spider-Man 3D compatibility.",
        WS_CHILD|WS_VISIBLE,20,270,610,50,hwnd,NULL,NULL,NULL);
      SendMessageA(label,WM_SETFONT,(WPARAM)font,TRUE);
      SendMessageA(w->pads,WM_SETFONT,(WPARAM)font,TRUE);
      SendMessageA(w->status,WM_SETFONT,(WPARAM)font,TRUE);
      SetTimer(hwnd,RAW_TIMER,35,NULL);
      return 0;
    }
    case WM_COMMAND:
      if(!w)return 0;
      if(HIWORD(wp)==BN_CLICKED) {
        if(LOWORD(wp)==RAW_BEGIN)wizard_start(w);
        else if(LOWORD(wp)==RAW_SKIP && w->active_id &&
                w->step<SPIDER_RAW_MAP_COUNT) {
          if(required_step(w->step))
            wizard_text(w,"This control is required and cannot be skipped.");
          else {
            SpiderSDL3RawInput skipped={0,0,0};
            wizard_next(w,skipped);
          }
        } else if(LOWORD(wp)==RAW_CLOSE)PostMessageA(hwnd,WM_CLOSE,0,0);
      }
      return 0;
    case WM_TIMER:
      if(w && wp==RAW_TIMER)wizard_tick(w);
      return 0;
    case WM_CLOSE:
      KillTimer(hwnd,RAW_TIMER);
      SpiderSDL3Probe_RawEnd();
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      KillTimer(hwnd,RAW_TIMER);
      SetWindowLongPtrA(hwnd,GWLP_USERDATA,0);
      return 0;
  }
  return DefWindowProcA(hwnd,msg,wp,lp);
}
void SpiderSDL3RawWizard_Open(HWND owner,void (*logger)(const char *)) {
  static int registered=0,busy=0;
  static const char *const cls="SpiderMan2k1FixSDL3RawWizard";
  WNDCLASSA wc;
  SpiderRawWizard w;
  HWND hwnd;
  MSG msg;
  int was_enabled;
  if(busy || !owner || !IsWindow(owner) ||
     GetForegroundWindow()!=owner || !SpiderSDL3Probe_SetupPump())
    return;
  busy=1;
  if(!registered) {
    memset(&wc,0,sizeof(wc));
    wc.lpfnWndProc=wizard_wndproc;
    wc.hInstance=GetModuleHandleA(NULL);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName=cls;
    if(!RegisterClassA(&wc)) {busy=0;return;}
    registered=1;
  }
  memset(&w,0,sizeof(w));w.owner=owner;w.log=logger;
  hwnd=CreateWindowExA(WS_EX_DLGMODALFRAME,cls,
    "SpiderMan2k1Fix - Recognize Raw SDL3 Joystick",
    WS_CAPTION|WS_SYSMENU|WS_POPUP,
    CW_USEDEFAULT,CW_USEDEFAULT,665,370,
    owner,NULL,GetModuleHandleA(NULL),&w);
  if(!hwnd){busy=0;return;}
  was_enabled=IsWindowEnabled(owner);
  if(was_enabled)EnableWindow(owner,FALSE);
  ShowWindow(hwnd,SW_SHOW);
  UpdateWindow(hwnd);
  SpiderSDL3Probe_SetupPump();
  wizard_refresh(&w);
  if(w.pads)SetFocus(w.pads);
  if(logger)logger("SDL3-RAW-WIZARD: opened by keyboard; raw joystick mapping is isolated from original DirectInput game");
  while(IsWindow(hwnd)) {
    BOOL ok=GetMessageA(&msg,NULL,0,0);
    if(ok<=0) {
      if(ok==0)PostQuitMessage((int)msg.wParam);
      break;
    }
    if(msg.message==WM_KEYDOWN &&
       (msg.hwnd==hwnd || IsChild(hwnd,msg.hwnd)) &&
       !(msg.lParam&(1L<<30))) {
      if(msg.wParam==VK_ESCAPE) {
        PostMessageA(hwnd,WM_CLOSE,0,0);continue;
      }
      if(msg.wParam==VK_RETURN) {
        wizard_start(&w);continue;
      }
      if(msg.wParam=='S' && w.active_id &&
         w.step<SPIDER_RAW_MAP_COUNT) {
        if(required_step(w.step))
          wizard_text(&w,"Required input: cannot skip this step.");
        else {
          SpiderSDL3RawInput none={0,0,0};
          wizard_next(&w,none);
        }
        continue;
      }
    }
    if(IsDialogMessageA(hwnd,&msg))continue;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  SpiderSDL3Probe_RawEnd();
  if(IsWindow(hwnd))DestroyWindow(hwnd);
  if(was_enabled && IsWindow(owner))EnableWindow(owner,TRUE);
  if(IsWindow(owner)) {
    SetForegroundWindow(owner);
    SetActiveWindow(owner);
  }
  busy=0;
}
