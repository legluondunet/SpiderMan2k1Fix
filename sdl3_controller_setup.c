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
#include "sdl3_controller_setup_nav.h"
#include "sdl3_gamepad_selection.h"
#include "sdl3_controller_setup_focus.h"
#include "gamepad_ui_profile.h"
#include "sdl3_setup_focus_policy.h"
#include "sdl3_raw_wizard.h"
#include "sdl3_physical_escape.h"

enum {
  SPIDER_UI_PAD=1001, SPIDER_UI_ACTION=1002,
  SPIDER_UI_CAPTURE=1003, SPIDER_UI_RESET=1004,
  SPIDER_UI_CLOSE=1005, SPIDER_UI_STATUS=1006,
  SPIDER_UI_TIMER=1007,
  /* These controls belong only to the SDL3 dialog. No global F keys. */
  SPIDER_UI_RAW=1008, SPIDER_UI_TEST=1009,
  SPIDER_UI_USE=1010
};
typedef struct {
  HWND hwnd,owner,pads,actions,status;
  SpiderSDL3Capture capture;
  SpiderSDL3SetupNav nav;
  SpiderSDL3SetupFocus focus;
  /* The native gameplay Start-selection loop is suspended by this modal.
   * Track separate, neutral-armed Start edges for ALL mapped gamepads.
   * This state changes only the dialog-selected profile, never the
   * original game-owned DirectInput controller while the modal is open. */
  SpiderSDL3Selection modal_start;
  uint32_t instance;
  uint16_t vid,pid;
  unsigned slot;
  unsigned count;
  unsigned action;
  unsigned user_chose_controller; /* never equate first preview row with explicit pad choice */
  unsigned test_mode,test_reports,test_blocked_commands,test_suppressed_keys;
  int test_escape_raw; /* physical keyboard Escape; never synthetic pad Esc */
  uint32_t test_buttons_previous;
  int test_axes_previous[6];
  /* Capped controller focus/Start telemetry for real Windows regression
   * reports; no per-frame spam and no change to input routing. */
  unsigned start_trace_count;
  unsigned start_unarmed_reported[SPIDER_SDL3_MAX_GAMEPADS];
  /* Close from a physical SDL3 B must wait for its release. Never destroy
   * our owner window synchronously inside WM_TIMER or a child callback. */
  unsigned close_pending;
  unsigned closing;
  /* A fresh Start in the modal OR a physical Circle/B (or Cross on
   * focused Close) from its displayed controller requests a gameplay
   * owner transfer AFTER modal exit. Merely browsing L1/R1 does not. */
  uint32_t start_owner_id;
  uint16_t start_owner_vid,start_owner_pid;
  unsigned start_owner_requested;
  DWORD close_requested_ms;
  /* Foreground snapshot at WM_CLOSE, before destroying this HWND. */
  HWND closing_foreground;
  unsigned closing_foreground_captured;
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
    GetPrivateProfileIntA("Input","ExperimentalSDL3ControllerSetup",1,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",1,ini)==1;
  if(setup_enabled && setup_report)
    setup_report("SDL3-SETUP: experimental native Joystick menu ENABLED (persistent model profiles; original gameplay unchanged)");
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
/* Real native Win32 focus gives the selected control a visible focus
 * rectangle, not a fake in-game cursor or synthetic keyboard event.
 * The game-owned window is never touched while this modal owns focus. */
static int setup_focus_button_id(unsigned selected) {
  if(selected==SPIDER_SETUP_BUTTON_CAPTURE)return SPIDER_UI_CAPTURE;
  if(selected==SPIDER_SETUP_BUTTON_RESET)return SPIDER_UI_RESET;
  if(selected==SPIDER_SETUP_BUTTON_CLOSE)return SPIDER_UI_CLOSE;
  if(selected==SPIDER_SETUP_BUTTON_RAW)return SPIDER_UI_RAW;
  if(selected==SPIDER_SETUP_BUTTON_TEST)return SPIDER_UI_TEST;
  return SPIDER_UI_USE;
}
/* Only ONE visible selection cursor: hide the list highlight when a
 * button has native focus, but retain ui->action for future editing. */
static void setup_sync_visual_selection(SpiderSDL3SetupUI *ui) {
  if(!ui || !IsWindow(ui->actions))return;
  SendMessageA(ui->actions,LB_SETCURSEL,
    ui->focus.column==SPIDER_SETUP_FOCUS_ACTIONS?
       (WPARAM)ui->action:(WPARAM)-1,0);
}
static void setup_apply_focus(SpiderSDL3SetupUI *ui) {
  HWND target;
  static const char *const names[SPIDER_SETUP_BUTTON_COUNT]={
    "Capture selected action","Restore defaults","Close",
    "Calibrate SDL3 joystick","Test SDL3 inputs",
    "Use selected SDL3 controller"
  };
  char line[180];
  if(!ui || !IsWindow(ui->hwnd))return;
  target=ui->focus.column==SPIDER_SETUP_FOCUS_BUTTONS?
    GetDlgItem(ui->hwnd,setup_focus_button_id(ui->focus.button)):
    ui->actions;
  if(!target || !IsWindow(target))return;
  /* The native Win32 focus cue is normally keyboard-only. Force only the
   * focus indicator visible; do NOT alter selection or inject game keys. */
  SendMessageA(ui->hwnd,WM_CHANGEUISTATE,
    MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
  SetFocus(target);
  setup_sync_visual_selection(ui);
  if(ui->focus.column==SPIDER_SETUP_FOCUS_BUTTONS) {
    snprintf(line,sizeof(line),
      "FOCUS: %s. Cross/A activates; stick left returns to action list.",
      names[ui->focus.button]);
    status(ui,line);
  } else
    status(ui,"FOCUS: action list. Stick up/down selects; right reaches buttons.");
}
/* Deferred gamepad close waits for release of the activating button.
 * Never destroy the owner HWND synchronously in WM_TIMER. */
static void setup_close_from_pad(SpiderSDL3SetupUI *ui,const char *source) {
  if(!ui || ui->close_pending)return;
  /* The physical controller operating this panel should also control
   * the original Joystick menu after closing it. Without this, browsing
   * DS4 via R1 then pressing Circle closes the modal successfully, but
   * DS4 Circle stops working in the underlying game menu because the
   * old Xbox is still its owner. Never rebind original DirectInput on
   * the modal stack; queue a verified handoff for next foreground Flip.
   * Escape, mouse X and keyboard/Win32 Close are NOT pad-close events. */
  if(ui->instance && ui->vid && ui->pid && !ui->capture.active) {
    char line[190];
    ui->start_owner_id=ui->instance;
    ui->start_owner_vid=ui->vid;
    ui->start_owner_pid=ui->pid;
    ui->start_owner_requested=1u;
    if(setup_report) {
      snprintf(line,sizeof(line),
        "SDL3-SETUP: PAD-CLOSE GAME-OWNER requested selected-id=%lu VID:PID=%04X:%04X; deferred until safe foreground Flip",
        (unsigned long)ui->instance,(unsigned)ui->vid,(unsigned)ui->pid);
      setup_report(line);
    }
  }
  ui->close_pending=1;
  ui->close_requested_ms=GetTickCount();
  spider_sdl3_setup_nav_reset(&ui->nav);
  status(ui,"Release controller buttons to return to Spider-Man.");
  if(setup_report)setup_report(source);
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
  for(i=0;i<SPIDER_SDL3_VISIBLE_ACTION_COUNT;i++) {
    char label[154],binding[85];
    const char *key;
    key=spider_sdl3_action_names[i];
    binding_name(p->actions[i],binding,sizeof(binding));
    snprintf(label,sizeof(label),"%02u. %-4s %-16s : %s",
      i+1,"3D",key,binding);
    SendMessageA(ui->actions,LB_ADDSTRING,0,(LPARAM)label);
  }
  if(ui->action>=SPIDER_SDL3_VISIBLE_ACTION_COUNT)ui->action=0;
  setup_sync_visual_selection(ui);
}
/* The new profile is serialized/committed BEFORE updating the live bank.
 * A failed filesystem operation cannot silently discard a good mapping. */
static int commit_profile(SpiderSDL3SetupUI *ui,
                          const SpiderSDL3ControllerProfile *trial,
                          const char *label) {
  char line[230];
  if(!ui || !trial || !SpiderSDL3Probe_SetupCommit(trial)) {
    status(ui,"SAVE FAILED: mapping unchanged. Check folder permissions/log.");
    if(setup_report)setup_report("SDL3-SETUP: SAVE FAILED; in-memory mapping retained");
    return 0;
  }
  refresh_actions(ui);
  snprintf(line,sizeof(line),"%s SAVED: %04X-%04X.ini (gameplay controls unchanged)",
    label?label:"Profile",(unsigned)trial->vendor,(unsigned)trial->product);
  status(ui,line);
  if(setup_report)setup_report("SDL3-SETUP: model profile committed to disk (no native gameplay change)");
  return 1;
}
static void select_slot(SpiderSDL3SetupUI *ui,unsigned slot) {
  char name[120],line[230];
  SpiderSDL3ControllerSample s;
  uint32_t id=0;
  uint16_t vid=0,pid=0;
  if(!ui)return;
  spider_sdl3_capture_reset(&ui->capture);
  spider_sdl3_setup_nav_reset(&ui->nav);
  ui->start_owner_requested=0u; /* new profile invalidates prior claim */
  ui->slot=slot;ui->instance=0;ui->vid=0;ui->pid=0;
  if(!SpiderSDL3Probe_SetupRead(slot,&id,&vid,&pid,name,sizeof(name),&s)) {
    ui->modal_start.active=0;
    ui->user_chose_controller=0;
    refresh_actions(ui);
    status(ui,"No mapped controller selected. Connect a pad or use keyboard to Close.");
    return;
  }
  ui->instance=id;ui->vid=vid;ui->pid=pid;
  ui->modal_start.active=id; /* local chooser follows manual LB/RB too */
  refresh_actions(ui);
  snprintf(line,sizeof(line),
    "%s (%04X:%04X) - SDL3 profile editable/saved; universal 3D game bridge NOT active.",
    name,(unsigned)vid,(unsigned)pid);
  status(ui,line);
}
static void refresh_pads(SpiderSDL3SetupUI *ui) {
  unsigned n,i,desired=0;
  uint32_t old_id,gameplay_id=0;
  uint32_t opened_ids[SPIDER_SDL3_MAX_GAMEPADS]={0};
  unsigned start_released_at_scan[SPIDER_SDL3_MAX_GAMEPADS]={0};
  uint16_t gameplay_vid=0,gameplay_pid=0;
  int matched_owner=0;
  char selection_line[220];
  if(!ui || !ui->pads)return;
  old_id=ui->instance;
  /* Initial modal open must follow the *game-owned selected SDL3 pad*,
   * not the first enumerated pad (typically a connected DS4). A user
   * choosing another model inside this dialog retains that choice on
   * subsequent refreshes. The identity API already rejects duplicate
   * VID:PID and stale/non-native selections. */
  if(!old_id)
    SpiderSDL3Probe_SelectedIdentity(&gameplay_vid,&gameplay_pid,&gameplay_id);
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
      if(i<SPIDER_SDL3_MAX_GAMEPADS) {
        opened_ids[i]=id;
        start_released_at_scan[i]=
          (sample.buttons&(1u<<SPIDER_SDL3_BUTTON_START))==0u;
      }
      if(old_id && id==old_id)desired=i;
      else if(!old_id && gameplay_id && id==gameplay_id &&
              vid==gameplay_vid && pid==gameplay_pid) {
        desired=i;
        matched_owner=1;
      }
    }
    SendMessageA(ui->pads,CB_ADDSTRING,0,(LPARAM)line);
  }
  ui->count=n;
  spider_sdl3_selection_sync(&ui->modal_start,opened_ids,n);
  /* SetupRead already sampled all physical pads while the modal owns
   * foreground. Prime new/reconnected instances immediately if Start
   * was actually seen RELEASED; do not accept a held Start, and do not
   * bypass the focus-loss neutral gate. This avoids missing the first
   * short deliberate press before the next WM_TIMER (35 ms). */
  for(i=0;i<n && i<SPIDER_SDL3_MAX_GAMEPADS;i++) {
    if(opened_ids[i] && start_released_at_scan[i])
      spider_sdl3_selection_start_edge(&ui->modal_start,opened_ids[i],0);
    ui->start_unarmed_reported[i]=0;
  }
  if(!n) {
    ui->user_chose_controller=0;
    spider_sdl3_capture_reset(&ui->capture);
    ui->instance=0;ui->vid=0;ui->pid=0;
    refresh_actions(ui);
    status(ui,"No SDL3 mapped gamepad. Connect a pad; keyboard Close still works.");
    return;
  }
  SendMessageA(ui->pads,CB_SETCURSEL,desired,0);
  if(!old_id && setup_report) {
    snprintf(selection_line,sizeof(selection_line),
      "SDL3-SETUP: AUTO-SELECT slot=%u source=%s gameplay-id=%lu VID:PID=%04X:%04X",
      desired,matched_owner?"verified-gameplay-owner":"fallback-first-mapped",
      (unsigned long)gameplay_id,(unsigned)gameplay_vid,(unsigned)gameplay_pid);
    setup_report(selection_line);
  }
  if(!old_id || (old_id && opened_ids[desired]!=old_id))
    ui->user_chose_controller=0; /* automatic/default row is NOT an explicit choice */
  if(old_id && desired!=ui->slot)
    spider_sdl3_capture_reset(&ui->capture);
  select_slot(ui,desired);
  if(n>1u && !ui->user_chose_controller) {
    status(ui,"Multiple SDL3 controllers: select intended pad in dropdown or L1/R1 BEFORE Use selected controller.");
    if(setup_report)setup_report("SDL3-SETUP: SELECT-UNCONFIRMED multiple pads; auto-preview cannot claim ownership");
  }
}
/* Explicit modal-only Start handoff: all devices are sampled on the same
 * existing WM_TIMER / SDL3 game thread, after one SDL3 pump. A new Start
 * press must be preceded by a released/neutral sample. No game native
 * DirectInput selection or gamepad RAM is changed here. */
static int modal_start_handoff(SpiderSDL3SetupUI *ui) {
  unsigned i,next=0,claims=0;
  uint32_t before;
  if(!ui || !ui->count)return 0;
  before=ui->modal_start.active;
  for(i=0;i<ui->count;i++) {
    SpiderSDL3ControllerSample sample;
    uint32_t id=0;
    uint16_t vid=0,pid=0;
    unsigned pressed,armed=0,previous=0,j;
    int edge;
    if(!SpiderSDL3Probe_SetupRead(i,&id,&vid,&pid,
                                  NULL,0,&sample))continue;
    pressed=(sample.buttons&(1u<<SPIDER_SDL3_BUTTON_START))!=0;
    for(j=0;j<ui->modal_start.count;j++)
      if(ui->modal_start.slots[j].id==id) {
        armed=ui->modal_start.slots[j].start_armed;
        previous=ui->modal_start.slots[j].previous_start;
        break;
      }
    if(!pressed && i<SPIDER_SDL3_MAX_GAMEPADS)
      ui->start_unarmed_reported[i]=0;
    if(pressed && i<SPIDER_SDL3_MAX_GAMEPADS &&
       (!previous || (!armed && !ui->start_unarmed_reported[i])) &&
       ui->start_trace_count<32u && setup_report) {
      char line[208];
      snprintf(line,sizeof(line),
        "SDL3-SETUP: START-SAMPLE id=%lu slot=%u armed=%u previous=%u capture=%u closing=%u foreground=1",
        (unsigned long)id,i,armed,previous,
        ui->capture.active,ui->close_pending);
      setup_report(line);
      ui->start_trace_count++;
      if(!armed)ui->start_unarmed_reported[i]=1;
    }
    edge=spider_sdl3_selection_start_edge(&ui->modal_start,id,pressed);
    if(edge) {
      next=i;
      ++claims;
    }
  }
  /* Simultaneous Start presses are ambiguous; refuse both. A capture or
   * pending close cannot change profiles halfway through its lifecycle. */
  if(claims!=1u || ui->capture.active || ui->close_pending) {
    if(claims==1u && ui->capture.active && ui->start_trace_count<32u && setup_report) {
      setup_report("SDL3-SETUP: START-IGNORED action capture active; Esc cancels capture, then use Select controller");
      ui->start_trace_count++;
    }
    if(claims>1u && ui->start_trace_count<32u && setup_report) {
      setup_report("SDL3-SETUP: START-REFUSED simultaneous claims; previous displayed controller retained");
      ui->start_trace_count++;
    }
    /* All Start edges have been sampled/consumed. Do NOT leave the
     * modal's active identity on the last sampled device when two pads
     * press Start together, or when capturing/closing. */
    ui->modal_start.active=before;
    return 0;
  }
  if(next!=ui->slot) {
    SendMessageA(ui->pads,CB_SETCURSEL,next,0);
    select_slot(ui,next);
  }
  ui->user_chose_controller=1u; /* verified physical Start selection */
  /* Start on the currently displayed profile (e.g. after L1/R1
   * browsing) is still an explicit request to own the original game
   * AFTER close. It need not change the combo-box row first. */
  /* Defer the actual game DirectInput handoff until AFTER this modal
   * is destroyed, when the game's native Flip loop resumes. */
  ui->start_owner_id=ui->instance;
  ui->start_owner_vid=ui->vid;
  ui->start_owner_pid=ui->pid;
  ui->start_owner_requested=1u;
  if(setup_report) {
    char line[195];
    snprintf(line,sizeof(line),
      "SDL3-SETUP: MODAL-START switched setup-only selected-id=%lu VID:PID=%04X:%04X (game owner handoff deferred until modal closes)",
      (unsigned long)ui->instance,(unsigned)ui->vid,(unsigned)ui->pid);
    setup_report(line);
  }
  return 1;
}
/* Choose a normalized controller explicitly without interpreting an action
 * being rebound as a native Start owner event. Next Flip checks identity
 * again; original 2001 DirectInput state is never modified here. */
static int setup_select_explicit_pad(SpiderSDL3SetupUI *ui) {
  SpiderSDL3ControllerSample sample;
  uint32_t id=0;
  uint16_t vid=0,pid=0;
  char line[220];
  if(!ui || ui->closing || ui->close_pending || ui->test_mode)return 0;
  /* Explicit Use on the auto-preview DS4 incorrectly selected the PS4
   * in a two-pad PS3+DS4 session. Require intentional dropdown/L1-R1
   * selection; only that actual user intent can claim the controller. */
  if(ui->count>1u && !ui->user_chose_controller) {
    status(ui,"Select your controller in the dropdown (or L1/R1) before pressing Use selected controller.");
    if(setup_report)setup_report("SDL3-SETUP: EXPLICIT-SELECT REFUSED auto-preview; multiple pads require deliberate choice");
    return 0;
  }
  if(ui->capture.active) {
    /* Proton may synthesize Return from the gamepad and start an action
     * rebind accidentally. First Use click CANCELS that capture only.
     * A second deliberate click/chord must approve controller ownership.
     * Never accept a single synthetic BN_CLICKED as a controller claim. */
    spider_sdl3_capture_reset(&ui->capture);
    spider_sdl3_setup_nav_reset(&ui->nav);
    SetFocus(ui->hwnd); /* inert HWND, not a focused default button */
    status(ui,"CAPTURE CANCELLED. Click Use selected SDL3 controller again to select the chosen pad.");
    if(setup_report)setup_report("SDL3-SETUP: EXPLICIT-SELECT CAPTURE CANCELLED; no owner queued; click Use again");
    return 0;
  }
  if(!ui->instance || !ui->vid || !ui->pid ||
     !SpiderSDL3Probe_SetupRead(ui->slot,&id,&vid,&pid,
                                NULL,0,&sample) ||
     id!=ui->instance || vid!=ui->vid || pid!=ui->pid) {
    status(ui,"Selection refused: SDL3 device disconnected/changed.");
    if(setup_report)setup_report("SDL3-SETUP: EXPLICIT-SELECT refused stale controller");
    return 0;
  }
  if(ui->start_owner_requested && ui->start_owner_id==id &&
     ui->start_owner_vid==vid && ui->start_owner_pid==pid) {
    if(setup_report)setup_report("SDL3-SETUP: EXPLICIT-SELECT already queued; duplicate activation ignored");
    return 1;
  }
  ui->start_owner_id=id;
  ui->start_owner_vid=vid;
  ui->start_owner_pid=pid;
  ui->start_owner_requested=1u;
  /* Selecting a controller is NOT a rebind. Remove the old focused
   * button cursor; restore only the list selection and require a fresh
   * neutral controller sample before it can activate another control. */
  ui->focus.column=SPIDER_SETUP_FOCUS_ACTIONS;
  spider_sdl3_setup_nav_reset(&ui->nav);
  if(IsWindow(ui->actions))SetFocus(ui->actions);
  setup_sync_visual_selection(ui);
  snprintf(line,sizeof(line),
    "SDL3-SETUP: EXPLICIT-SELECT QUEUED id=%lu VID:PID=%04X:%04X (revalidate next foreground Flip; native DI unchanged)",
    (unsigned long)id,(unsigned)vid,(unsigned)pid);
  if(setup_report)setup_report(line);
  status(ui,"Selected controller queued. Press keyboard Esc to return to game.");
  return 1;
}
static void setup_toggle_sdl3_test(SpiderSDL3SetupUI *ui);
static void setup_sdl3_test_lock(SpiderSDL3SetupUI *ui,int lock) {
  unsigned i;
  if(!ui || !ui->hwnd)return;
  /* No Win32 control may respond to controller-emulated keyboard events
   * during the SDL3 hardware test. Merely changing focus is insufficient
   * with Wine/Proton and the physical button that started the test. */
  SendMessageA(ui->pads,CB_SHOWDROPDOWN,FALSE,0);
  EnableWindow(ui->pads,lock?FALSE:TRUE);
  EnableWindow(ui->actions,lock?FALSE:TRUE);
  for(i=SPIDER_UI_CAPTURE;i<=SPIDER_UI_CLOSE;i++) {
    HWND button=GetDlgItem(ui->hwnd,(int)i);
    if(button)EnableWindow(button,lock?FALSE:TRUE);
  }
  for(i=SPIDER_UI_RAW;i<=SPIDER_UI_USE;i++) {
    HWND button=GetDlgItem(ui->hwnd,(int)i);
    if(button)EnableWindow(button,lock?FALSE:TRUE);
  }
  /* The parent has no default pushbutton semantics. It cannot turn
   * stray Space/Return into BN_CLICKED. An explicit Ctrl+Shift+T exits. */
  if(lock)SetFocus(ui->hwnd);
  else if(IsWindow(ui->actions))SetFocus(ui->actions);
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
    spider_sdl3_setup_nav_reset(&ui->nav);
    spider_sdl3_selection_focus_lost(&ui->modal_start);
    return;
  }
  if(!SpiderSDL3Probe_SetupPump()) {
    if(!ui->instance)
      status(ui,"SDL3 not initialized. Return to the game and try again.");
    return;
  }
  if(SpiderSDL3Probe_SetupCount()!=ui->count)refresh_pads(ui);
  /* Hardware diagnostic must not change the selected pad as a side effect
   * of its Start button; preserve the selected instance for telemetry. */
  if(!ui->test_mode && modal_start_handoff(ui))return;
  if(!ui->instance) {
    /* The selected pad may detach after B requested close. Never strand
     * the modal window waiting for a release on a vanished device. */
    if(ui->close_pending) {
      ui->close_pending=0;
      if(setup_report)
        setup_report("SDL3-SETUP: selected pad disconnected while CLOSE pending; posting WM_CLOSE");
      PostMessageA(ui->hwnd,WM_CLOSE,0,0);
    }
    return;
  }
  if(!SpiderSDL3Probe_SetupRead(ui->slot,&id,&vid,&pid,
                               name,sizeof(name),&s) ||
      id!=ui->instance || vid!=ui->vid || pid!=ui->pid) {
    refresh_pads(ui);
    return;
  }
  if(ui->close_pending) {
    /* The game's native menu must not inherit the closing B press.
     * Complete close asynchronously after all face buttons and triggers
     * are released. Escape always remains an immediate keyboard escape.
     * A bounded deadline avoids trapping a player with a stuck button. */
    if(spider_sdl3_profile_neutral(&s) ||
       (uint32_t)(GetTickCount()-ui->close_requested_ms)>=2000u) {
      ui->close_pending=0;
      if(setup_report)
        setup_report("SDL3-SETUP: pad close RELEASED; posting deferred WM_CLOSE outside WM_TIMER");
      PostMessageA(ui->hwnd,WM_CLOSE,0,0);
    }
    return;
  }
  if(ui->test_mode) {
    /* Every selected, mapped SDL3 gamepad can be tested; all other
     * Win32 actions are disabled and guarded against synthetic clicks.
     * Log BOTH sticks and triggers, not just face buttons/trigger edges. */
    unsigned axis,changed=s.buttons!=ui->test_buttons_previous;
    for(axis=0;axis<6u;axis++) {
      int delta=(int)s.axis[axis]-ui->test_axes_previous[axis];
      if(delta>1800 || delta< -1800)changed=1u;
    }
    if(changed) {
      ui->test_buttons_previous=s.buttons;
      for(axis=0;axis<6u;axis++)
        ui->test_axes_previous[axis]=s.axis[axis];
      snprintf(line,sizeof(line),
        "SDL3 TEST: id=%lu buttons=0x%06lX LX=%d LY=%d RX=%d RY=%d LT=%d RT=%d [Ctrl+Shift+T stop]",
        (unsigned long)id,(unsigned long)s.buttons,
        (int)s.axis[SPIDER_SDL3_AXIS_LEFTX],
        (int)s.axis[SPIDER_SDL3_AXIS_LEFTY],
        (int)s.axis[SPIDER_SDL3_AXIS_RIGHTX],
        (int)s.axis[SPIDER_SDL3_AXIS_RIGHTY],
        (int)s.axis[SPIDER_SDL3_AXIS_LT],
        (int)s.axis[SPIDER_SDL3_AXIS_RT]);
      status(ui,line);
      if(setup_report && ui->test_reports++<64u)setup_report(line);
    }
    return;
  }
  if(!ui->capture.active) {
    uint32_t edge=spider_sdl3_setup_nav_step(&ui->nav,&s);
    if(edge==(1u<<SPIDER_SETUP_NAV_LEFT) ||
       edge==(1u<<SPIDER_SETUP_NAV_RIGHT)) {
      if(spider_sdl3_setup_focus_step(&ui->focus,edge))
        setup_apply_focus(ui);
    } else if(edge==(1u<<SPIDER_SETUP_NAV_UP) ||
              edge==(1u<<SPIDER_SETUP_NAV_DOWN)) {
      if(ui->focus.column==SPIDER_SETUP_FOCUS_BUTTONS) {
        if(spider_sdl3_setup_focus_step(&ui->focus,edge))
          setup_apply_focus(ui);
      } else {
        if(edge==(1u<<SPIDER_SETUP_NAV_UP)) {
          if(ui->action)ui->action--;
          else ui->action=SPIDER_SDL3_VISIBLE_ACTION_COUNT-1u;
        } else
          ui->action=(ui->action+1u)%SPIDER_SDL3_VISIBLE_ACTION_COUNT;
        refresh_actions(ui);
        setup_apply_focus(ui);
      }
    } else if(edge==(1u<<SPIDER_SETUP_NAV_LB) ||
              edge==(1u<<SPIDER_SETUP_NAV_RB)) {
      if(ui->count>1u) {
        unsigned next=(ui->slot+ui->count+
          (edge==(1u<<SPIDER_SETUP_NAV_LB)?ui->count-1u:1u))%ui->count;
        SendMessageA(ui->pads,CB_SETCURSEL,next,0);
        select_slot(ui,next);
        ui->user_chose_controller=1u;
        if(setup_report && ui->start_trace_count<32u) {
          char line[180];
          snprintf(line,sizeof(line),
            "SDL3-SETUP: PROFILE-BROWSE L1/R1 display-id=%lu VID:PID=%04X:%04X (game owner unchanged)",
            (unsigned long)ui->instance,(unsigned)ui->vid,(unsigned)ui->pid);
          setup_report(line);
          ui->start_trace_count++;
        }
      }
    } else if(edge==(1u<<SPIDER_SETUP_NAV_SOUTH)) {
      if(ui->focus.column==SPIDER_SETUP_FOCUS_BUTTONS) {
        const int id=setup_focus_button_id(ui->focus.button);
        if(id==SPIDER_UI_CLOSE) {
          setup_close_from_pad(ui,
            "SDL3-SETUP: Cross/A activated focused CLOSE; waiting for release");
        } else {
          /* Send a real button command on the same thread, AFTER WM_TIMER.
           * Never SendMessage into a potentially destructive modal handler. */
          HWND button=GetDlgItem(ui->hwnd,id);
          if(button)PostMessageA(ui->hwnd,WM_COMMAND,
            MAKEWPARAM(id,BN_CLICKED),(LPARAM)button);
          if(setup_report)
            setup_report(id==SPIDER_UI_RESET?
              "SDL3-SETUP: Cross/A activated focused RESET":
              "SDL3-SETUP: Cross/A activated focused CAPTURE");
        }
      } else if(current_profile(ui)) {
        /* South on a list row is navigation/confirmation, not permission
         * to edit a saved gameplay binding. Only the clearly labeled
         * Capture selected action button or Ctrl+Shift+C can arm a remap. */
        status(ui,"To edit this action: choose Capture selected action (or Ctrl+Shift+C).");
        if(setup_report && ui->start_trace_count++<32u)
          setup_report("SDL3-SETUP: South on actions list ignored (explicit Capture button required)");
      }
    } else if(edge==(1u<<SPIDER_SETUP_NAV_EAST)) {
      setup_close_from_pad(ui,
        "SDL3-SETUP: pad B requested CLOSE; waiting for physical release");
    } else if(edge==(1u<<SPIDER_SETUP_NAV_NORTH)) {
      SpiderSDL3ControllerProfile trial,*selected=current_profile(ui);
      if(selected && spider_sdl3_profile_default(&trial,ui->vid,ui->pid) &&
         commit_profile(ui,&trial,"Defaults"))
        if(setup_report)setup_report("SDL3-SETUP: controller Y defaults SAVED");
    }
    return;
  }
  rc=spider_sdl3_capture_feed(&ui->capture,&s,&candidate);
  if(rc!=1)return;
  spider_sdl3_setup_nav_reset(&ui->nav);
  p=current_profile(ui);
  if(!p) {
    status(ui,"Binding refused: no selected mapped controller.");
    return;
  }
  {
    SpiderSDL3ControllerProfile trial=*p;
    if(!spider_sdl3_profile_assign_visible(&trial,ui->action,candidate)) {
      status(ui,"Binding rejected (reserved input or conflicting profile).");
      return;
    }
    binding_name(candidate,bind,sizeof(bind));
    snprintf(line,sizeof(line),"%s = %s",
             spider_sdl3_action_names[ui->action],bind);
    commit_profile(ui,&trial,line);
  }
}
/* One keyboard path independent of Win32 dialog button focus.
 * Tab/Space remain available via IsDialogMessage and ordinary button
 * handling, but a mouse is NEVER required to reach any operation. */
static int handle_setup_keyboard(SpiderSDL3SetupUI *ui,UINT vk) {
  unsigned next;
  HWND focused;
  SpiderSDL3ControllerProfile *p;
  if(!ui || !ui->hwnd)return 0;
  if(ui->test_mode)return 1; /* active test owns ALL ordinary keyboard input */
  if(vk==VK_ESCAPE) {
    if(ui->capture.active) {
      spider_sdl3_capture_reset(&ui->capture);
      spider_sdl3_setup_nav_reset(&ui->nav);
      status(ui,"Capture cancelled. Original gameplay mapping unchanged.");
    } else {
      if(setup_report)setup_report("SDL3-SETUP: Escape requested CLOSE (deferred message)");
      PostMessageA(ui->hwnd,WM_CLOSE,0,0);
    }
    return 1;
  }
  /* Explicit keyboard shortcuts never overlap native button focus.
   * Control remapping is armed only by Ctrl+Shift+C or the Capture button. */
  if(vk=='C' && (GetKeyState(VK_CONTROL)&0x8000) &&
     (GetKeyState(VK_SHIFT)&0x8000)) {
    if(!ui->capture.active && current_profile(ui)) {
      spider_sdl3_capture_start(&ui->capture);
      spider_sdl3_setup_nav_reset(&ui->nav);
      status(ui,"CAPTURE: release all controls, then press one button or LT/RT. Esc cancels.");
    }
    return 1;
  }
  /* Ctrl+Shift+U safely cancels a stuck capture; a second press claims.
   * It follows the same fail-closed two-step rule as the Use button. */
  if(vk=='U' && (GetKeyState(VK_CONTROL)&0x8000) &&
     (GetKeyState(VK_SHIFT)&0x8000)) {
    setup_select_explicit_pad(ui);
    return 1;
  }
  if(ui->capture.active)return 0; /* Keyboard cannot be bound via SDL3. */
  focused=GetFocus();
  if(vk==VK_UP || vk==VK_DOWN) {
    /* Native drop-down keyboard navigation must keep working. */
    if(focused==ui->pads)return 0;
    /* Retain keyboard focus on a native focused button; arrows are
     * handled normally there. Analog input navigates button selection. */
    /* Arrow keys always browse the action list, even if a button has
     * keyboard focus. This fixes the apparent frozen list after Use. */
    if(ui->focus.column!=SPIDER_SETUP_FOCUS_ACTIONS) {
      ui->focus.column=SPIDER_SETUP_FOCUS_ACTIONS;
      if(IsWindow(ui->actions))SetFocus(ui->actions);
    }
    if(!current_profile(ui))return 1;
    next=(ui->action+SPIDER_SDL3_VISIBLE_ACTION_COUNT+
      (vk==VK_UP?SPIDER_SDL3_VISIBLE_ACTION_COUNT-1u:1u))%
      SPIDER_SDL3_VISIBLE_ACTION_COUNT;
    ui->action=next;
    refresh_actions(ui);
    status(ui,"Action selected. Press Enter to capture; release all buttons first.");
    return 1;
  }
  if(vk==VK_PRIOR || vk==VK_NEXT) {
    if(ui->count>1u) {
      next=(ui->slot+ui->count+
        (vk==VK_PRIOR?ui->count-1u:1u))%ui->count;
      SendMessageA(ui->pads,CB_SETCURSEL,next,0);
      select_slot(ui,next);
      ui->user_chose_controller=1u;
    }
    return 1;
  }
  if(vk==VK_RETURN) {
    if(focused==ui->pads &&
       SendMessageA(ui->pads,CB_GETDROPPEDSTATE,0,0))return 0;
    /* Enter also activates a focused Reset/Close button, as users expect
     * from standard Win32 dialogs. Space and Tab continue working. */
    if(focused==GetDlgItem(ui->hwnd,SPIDER_UI_CAPTURE) ||
       focused==GetDlgItem(ui->hwnd,SPIDER_UI_RESET) ||
       focused==GetDlgItem(ui->hwnd,SPIDER_UI_CLOSE) ||
       focused==GetDlgItem(ui->hwnd,SPIDER_UI_RAW) ||
       focused==GetDlgItem(ui->hwnd,SPIDER_UI_TEST) ||
       focused==GetDlgItem(ui->hwnd,SPIDER_UI_USE)) {
      int id=GetDlgCtrlID(focused);
      SendMessageA(ui->hwnd,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),(LPARAM)focused);
      return 1;
    }
    /* A Proton-emulated South->Return may be delivered while the
     * action list has focus. Never interpret Return alone as remapping
     * approval; use Capture button or Ctrl+Shift+C exclusively. */
    if(focused==ui->actions)
      status(ui,"Select Capture selected action (or Ctrl+Shift+C) to edit this row.");
    return 1;
  }
  if(vk==VK_F5) {
    SpiderSDL3ControllerProfile trial;
    p=current_profile(ui);
    if(p && spider_sdl3_profile_default(&trial,ui->vid,ui->pid) &&
       commit_profile(ui,&trial,"Defaults")) {
      spider_sdl3_setup_nav_reset(&ui->nav);
      if(setup_report)setup_report("SDL3-SETUP: F5 defaults SAVED");
    }
    return 1;
  }
  if(vk==VK_F1) {
    status(ui,"L1/R1 browse pads. Select controller / Ctrl+Shift+U, then keyboard Esc. Start works outside capture.");
    return 1;
  }
  return 0; /* Tab and Space keep their native Win32 button behavior. */
}
/* Enter through the ordinary Tab+Enter button, but EXIT only through an
 * explicit keyboard chord. During testing all controls including this
 * button are disabled so physical controller keys cannot click again.
 * F9 screenshots and F10 location shortcuts remain completely untouched. */
static void setup_toggle_sdl3_test(SpiderSDL3SetupUI *ui) {
  unsigned axis;
  char line[240];
  if(!ui || !ui->hwnd)return;
  ui->test_mode=!ui->test_mode;
  if(ui->test_mode)
    ui->test_escape_raw=spider_sdl3_escape_begin(ui->hwnd);
  else {
    spider_sdl3_escape_end(ui->test_escape_raw);
    ui->test_escape_raw=0;
  }
  ui->test_buttons_previous=~0u;
  for(axis=0;axis<6u;axis++)ui->test_axes_previous[axis]=-100000;
  ui->test_reports=0u;
  spider_sdl3_capture_reset(&ui->capture);
  spider_sdl3_setup_nav_reset(&ui->nav);
  spider_sdl3_selection_focus_lost(&ui->modal_start);
  setup_sdl3_test_lock(ui,ui->test_mode);
  if(ui->test_mode) {
    snprintf(line,sizeof(line),
      "SDL3 TEST ON id=%lu VID:PID=%04X:%04X. Esc (keyboard: %s), Ctrl+Shift+T stop.",
      (unsigned long)ui->instance,(unsigned)ui->vid,(unsigned)ui->pid,
      ui->test_escape_raw?"armed":"unavailable");
    status(ui,line);
  } else
    status(ui,"SDL3 TEST OFF: normal controls restored. Keyboard Esc closes.");
  if(setup_report) {
    snprintf(line,sizeof(line),
      "SDL3-SETUP: normalized input test %s id=%lu VID:PID=%04X:%04X (all Win32 controls %s; Ctrl+Shift+T exit; F9/F10 unchanged)",
      ui->test_mode?"ON":"OFF",
      (unsigned long)ui->instance,(unsigned)ui->vid,(unsigned)ui->pid,
      ui->test_mode?"DISABLED":"ENABLED");
    setup_report(line);
    if(ui->test_mode)setup_report(ui->test_escape_raw?
      "SDL3-SETUP: physical keyboard Escape via WM_INPUT ARMED; synthetic pad Circle/B blocked":
      "SDL3-SETUP: physical keyboard Escape unavailable; Ctrl+Shift+T remains safe stop");
  }
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
        "SDL3 CONTROLLER SETUP - 3D only; Cross/A Confirm, Circle/B Back fixed",
        WS_CHILD|WS_VISIBLE,18,16,690,28,hwnd,NULL,NULL,NULL);
      SendMessageA(label,WM_SETFONT,(WPARAM)font,TRUE);
      ui->pads=CreateWindowExA(0,"COMBOBOX","",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
        18,55,680,250,hwnd,(HMENU)(uintptr_t)SPIDER_UI_PAD,NULL,NULL);
      ui->actions=CreateWindowExA(WS_EX_CLIENTEDGE,"LISTBOX","",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|LBS_NOTIFY|WS_VSCROLL,
        18,99,430,255,hwnd,(HMENU)(uintptr_t)SPIDER_UI_ACTION,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Capture selected action",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_NOTIFY,
        465,113,230,36,hwnd,(HMENU)(uintptr_t)SPIDER_UI_CAPTURE,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Restore defaults (this model)",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_NOTIFY,
        465,160,230,36,hwnd,(HMENU)(uintptr_t)SPIDER_UI_RESET,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Close / return to game",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_NOTIFY,
        465,207,230,36,hwnd,(HMENU)(uintptr_t)SPIDER_UI_CLOSE,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Calibrate SDL3 joystick",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
        465,254,230,36,hwnd,(HMENU)(uintptr_t)SPIDER_UI_RAW,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Test SDL3 inputs",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
        465,301,230,36,hwnd,(HMENU)(uintptr_t)SPIDER_UI_TEST,NULL,NULL);
      CreateWindowExA(0,"BUTTON","Use selected SDL3 controller",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
        465,349,230,34,hwnd,(HMENU)(uintptr_t)SPIDER_UI_USE,NULL,NULL);
      label=CreateWindowExA(0,"STATIC",
        "Stick Left/Right: navigate    L1/R1: browse controllers\r\n"
        "Choose the intended gamepad ABOVE first if multiple are connected\r\n"
        "Use selected / Ctrl+Shift+U: choose pad; Ctrl+Shift+C: remap\r\n"
        "Start only outside capture; Esc: close/stop test; F9/F10 unchanged",
        WS_CHILD|WS_VISIBLE,18,393,684,65,hwnd,NULL,NULL,NULL);
      SendMessageA(label,WM_SETFONT,(WPARAM)font,TRUE);
      ui->status=CreateWindowExA(0,"STATIC","",
        WS_CHILD|WS_VISIBLE,18,467,684,45,hwnd,
        (HMENU)(uintptr_t)SPIDER_UI_STATUS,NULL,NULL);
      SendMessageA(ui->pads,WM_SETFONT,(WPARAM)font,TRUE);
      SendMessageA(ui->actions,WM_SETFONT,(WPARAM)font,TRUE);
      SendMessageA(ui->status,WM_SETFONT,(WPARAM)font,TRUE);
      for(unsigned i=SPIDER_UI_CAPTURE;i<=SPIDER_UI_CLOSE;i++) {
        HWND button=GetDlgItem(hwnd,(int)i);
        if(button)SendMessageA(button,WM_SETFONT,(WPARAM)font,TRUE);
      }
      for(unsigned i=SPIDER_UI_RAW;i<=SPIDER_UI_USE;i++) {
        HWND button=GetDlgItem(hwnd,(int)i);
        if(button)SendMessageA(button,WM_SETFONT,(WPARAM)font,TRUE);
      }
      SetTimer(hwnd,SPIDER_UI_TIMER,35,NULL);
      return 0;
    }
    case WM_COMMAND:
      if(!ui)return 0;
      /* If Wine queues native BN_CLICKED or list notifications after SDL3
       * test starts, refuse them regardless of the disabled HWND state. */
      if(ui->test_mode) {
        if(setup_report && ui->test_blocked_commands++<10u) {
          char line[175];
          snprintf(line,sizeof(line),
            "SDL3-SETUP: TEST blocked Win32 control id=%u notification=%u",
            (unsigned)LOWORD(wp),(unsigned)HIWORD(wp));
          setup_report(line);
        }
        return 0;
      }
      if(LOWORD(wp)==SPIDER_UI_PAD && HIWORD(wp)==CBN_SELCHANGE) {
        LRESULT idx=SendMessageA(ui->pads,CB_GETCURSEL,0,0);
        if(idx!=CB_ERR) {
          select_slot(ui,(unsigned)idx);
          ui->user_chose_controller=1u;
          if(setup_report) {
            char line[175];
            snprintf(line,sizeof(line),"SDL3-SETUP: USER-CHOSEN slot=%u id=%lu VID:PID=%04X:%04X",
              (unsigned)idx,(unsigned long)ui->instance,(unsigned)ui->vid,(unsigned)ui->pid);
            setup_report(line);
          }
        }
      } else if(LOWORD(wp)==SPIDER_UI_ACTION &&
                HIWORD(wp)==LBN_SETFOCUS) {
        ui->focus.column=SPIDER_SETUP_FOCUS_ACTIONS;
        setup_sync_visual_selection(ui);
      } else if(((LOWORD(wp)>=SPIDER_UI_CAPTURE &&
                  LOWORD(wp)<=SPIDER_UI_CLOSE) ||
                 (LOWORD(wp)>=SPIDER_UI_RAW &&
                  LOWORD(wp)<=SPIDER_UI_USE)) &&
                HIWORD(wp)==BN_SETFOCUS) {
        ui->focus.column=SPIDER_SETUP_FOCUS_BUTTONS;
        ui->focus.button=LOWORD(wp)<=SPIDER_UI_CLOSE?
          (unsigned)(LOWORD(wp)-SPIDER_UI_CAPTURE):
          (unsigned)(SPIDER_SETUP_BUTTON_RAW+LOWORD(wp)-SPIDER_UI_RAW);
        setup_sync_visual_selection(ui);
      } else if(LOWORD(wp)==SPIDER_UI_ACTION &&
                HIWORD(wp)==LBN_SELCHANGE) {
        LRESULT idx=SendMessageA(ui->actions,LB_GETCURSEL,0,0);
        if(idx!=LB_ERR && idx<SPIDER_SDL3_VISIBLE_ACTION_COUNT) {
          ui->action=(unsigned)idx;
          ui->focus.column=SPIDER_SETUP_FOCUS_ACTIONS;
          setup_sync_visual_selection(ui);
          spider_sdl3_capture_reset(&ui->capture);
          status(ui,"Action selected. Choose Capture then release all controls.");
        }
      } else if(LOWORD(wp)==SPIDER_UI_CAPTURE && HIWORD(wp)==BN_CLICKED) {
        if(current_profile(ui)) {
          spider_sdl3_capture_start(&ui->capture);
          spider_sdl3_setup_nav_reset(&ui->nav);
          status(ui,"Release ALL buttons/triggers; then press ONE new control. Esc cancels.");
        }
      } else if(LOWORD(wp)==SPIDER_UI_RESET && HIWORD(wp)==BN_CLICKED) {
        SpiderSDL3ControllerProfile *p=current_profile(ui);
        if(setup_report)setup_report("SDL3-SETUP: Reset BEGIN (modal window and selected model valid)");
        spider_sdl3_capture_reset(&ui->capture);
        spider_sdl3_setup_nav_reset(&ui->nav);
        {
          SpiderSDL3ControllerProfile trial;
          if(p && spider_sdl3_profile_default(&trial,ui->vid,ui->pid) &&
             commit_profile(ui,&trial,"Defaults")) {
            if(setup_report)setup_report("SDL3-SETUP: model defaults restored AND SAVED");
          } else if(setup_report)
            setup_report("SDL3-SETUP: Reset REFUSED (no valid selected model or save failure)");
        }
      } else if(LOWORD(wp)==SPIDER_UI_RAW && HIWORD(wp)==BN_CLICKED) {
        SpiderSDL3RawWizard_Open(ui->hwnd,setup_report);
        spider_sdl3_setup_nav_reset(&ui->nav);
        spider_sdl3_capture_reset(&ui->capture);
        if(SpiderSDL3Probe_SetupPump())refresh_pads(ui);
      } else if(LOWORD(wp)==SPIDER_UI_TEST && HIWORD(wp)==BN_CLICKED) {
        setup_toggle_sdl3_test(ui);
      } else if(LOWORD(wp)==SPIDER_UI_USE && HIWORD(wp)==BN_CLICKED) {
        setup_select_explicit_pad(ui);
      } else if(LOWORD(wp)==SPIDER_UI_CLOSE && HIWORD(wp)==BN_CLICKED) {
        if(setup_report)setup_report("SDL3-SETUP: Close button requested (deferred message)");
        PostMessageA(hwnd,WM_CLOSE,0,0);
      }
      return 0;
    case WM_TIMER:
      if(ui && wp==SPIDER_UI_TIMER)sample_device(ui);
      return 0;
    case WM_CLOSE:
      if(ui && !ui->closing) {
        if(ui->test_mode)setup_toggle_sdl3_test(ui);
        /* Before DestroyWindow: otherwise Explorer can be activated by
         * Windows and misidentified as a deliberate Alt+Tab. */
        ui->closing_foreground=GetForegroundWindow();
        ui->closing_foreground_captured=1u;
        ui->closing=1;
        if(setup_report)setup_report("SDL3-SETUP: WM_CLOSE entered; disarming timer and capture");
        spider_sdl3_capture_reset(&ui->capture);
        spider_sdl3_setup_nav_reset(&ui->nav);
      }
      KillTimer(hwnd,SPIDER_UI_TIMER);
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      KillTimer(hwnd,SPIDER_UI_TIMER);
      SetWindowLongPtrA(hwnd,GWLP_USERDATA,0);
      if(setup_report)setup_report("SDL3-SETUP: WM_DESTROY done; no active timer or captured input");
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
  HWND prior_foreground=NULL,final_foreground=NULL;
  int restore_allowed=0,focus_attempted=0,focus_result=0;
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
    "SpiderMan2k1Fix - Controller Setup (3D actions only)",
    WS_CAPTION|WS_SYSMENU|WS_POPUP,
    CW_USEDEFAULT,CW_USEDEFAULT,730,558,
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
  if(ui.actions)SetFocus(ui.actions); /* Keyboard-first, no mouse dependency. */
  if(setup_report)setup_report("SDL3-SETUP: preview dialog opened from original Joystick menu; gameplay input paused on stack");
  while(IsWindow(window)) {
    BOOL ok=GetMessageA(&msg,NULL,0,0);
    if(ok<=0) {
      if(ok==0){has_quit=1;quit_code=(int)msg.wParam;}
      break;
    }
    /* Use only hardware keyboard WM_INPUT Escape. Controller-mapped
     * VK_ESCAPE remains suppressed to avoid Circle/B stopping the test. */
    if(ui.test_mode && ui.test_escape_raw &&
       msg.message==WM_INPUT && msg.hwnd==window) {
      if(spider_sdl3_escape_key((HRAWINPUT)msg.lParam)) {
        if(setup_report)setup_report("SDL3-SETUP: PHYSICAL-ESC keyboard stopped normalized input test");
        setup_toggle_sdl3_test(&ui);
      }
      DefWindowProcA(window,msg.message,msg.wParam,msg.lParam);
      continue;
    }
    /* Gamepad buttons in Wine/Proton can synthesize Win32 keyboard
     * messages. While TEST is active, consume all ordinary key messages
     * before IsDialogMessage/DispatchMessage so disabled child controls
     * never receive a synthetic key/command. Keyboard-only Ctrl+Shift+T
     * stops testing; Ctrl+Shift+Q leaves the dialog in one action. */
    if(ui.test_mode &&
       (msg.hwnd==window || IsChild(window,msg.hwnd))) {
      if((msg.message==WM_KEYDOWN || msg.message==WM_SYSKEYDOWN) &&
         (GetKeyState(VK_CONTROL)&0x8000) &&
         (GetKeyState(VK_SHIFT)&0x8000) &&
         !(msg.lParam&(1L<<30))) {
        if(msg.wParam=='T') {
          setup_toggle_sdl3_test(&ui);
          continue;
        }
        if(msg.wParam=='Q') {
          setup_toggle_sdl3_test(&ui);
          PostMessageA(window,WM_CLOSE,0,0);
          continue;
        }
      }
      if(msg.message==WM_KEYDOWN || msg.message==WM_KEYUP ||
         msg.message==WM_SYSKEYDOWN || msg.message==WM_SYSKEYUP ||
         msg.message==WM_CHAR || msg.message==WM_SYSCHAR ||
         msg.message==WM_DEADCHAR || msg.message==WM_SYSDEADCHAR) {
        if(setup_report && ui.test_suppressed_keys++<12u) {
          char line[150];
          snprintf(line,sizeof(line),
            "SDL3-SETUP: TEST suppressed Win32 keyboard msg=0x%04X vk=%lu",
            (unsigned)msg.message,(unsigned long)msg.wParam);
          setup_report(line);
        }
        continue;
      }
    }
    if(msg.message==WM_KEYDOWN &&
       (msg.hwnd==window || IsChild(window,msg.hwnd)) &&
       !(msg.lParam&(1L<<30)) &&
       handle_setup_keyboard(&ui,(UINT)msg.wParam))
      continue;
    if(IsDialogMessageA(window,&msg))continue;
    TranslateMessage(&msg);DispatchMessageA(&msg);
  }
  if(setup_report)setup_report("SDL3-SETUP: modal message loop EXIT; restoring original game window");
  spider_sdl3_escape_end(ui.test_escape_raw);
  ui.test_escape_raw=0;
  /* If the modal was removed without WM_CLOSE (WM_QUIT), never assume a
   * foreign active window belongs to us. */
  if(IsWindow(window))DestroyWindow(window);
  prior_foreground=GetForegroundWindow();
  /* An owned dialog can transfer the foreground to Explorer as a side
   * effect of DestroyWindow. Use its pre-destruction snapshot, rather than
   * requiring the destroyed HWND still to be foreground. An actual Alt+Tab
   * BEFORE WM_CLOSE will still fail closed. */
  restore_allowed=spider_sdl3_setup_restore_focus(
    has_quit,ui.closing_foreground_captured,
    ui.closing_foreground==owner || ui.closing_foreground==window,
    prior_foreground==owner || prior_foreground==window,
    ui.closing_foreground==NULL,prior_foreground==NULL);
  if(parent_enabled && IsWindow(owner))EnableWindow(owner,TRUE);
  if(restore_allowed && IsWindow(owner) && IsWindowVisible(owner)) {
    SetActiveWindow(owner); /* same original game thread; do not attach foreign input queues */
    focus_attempted=1;
    focus_result=SetForegroundWindow(owner)!=0;
    if(GetForegroundWindow()==owner)SetFocus(owner);
  }
  final_foreground=GetForegroundWindow();
  if(setup_report) {
    char line[250];
    snprintf(line,sizeof(line),
      "SDL3-SETUP: owner focus restore allowed=%d attempted=%d result=%d preclose=%p previous=%p final=%p game=%p [pre-close owned-snapshot; external focus respected]",
      restore_allowed,focus_attempted,focus_result,
      (void *)ui.closing_foreground,(void *)prior_foreground,
      (void *)final_foreground,(void *)owner);
    setup_report(line);
  }
  setup_open=0;
  /* Reject any Start held while the modal owned the game thread, rather
   * than treating it as a fresh game-ownership command on next Flip. */
  SpiderSDL3Probe_SetupModalReturn(
    ui.start_owner_requested && !has_quit && restore_allowed &&
      final_foreground==owner ? ui.start_owner_id:0u,
    ui.start_owner_vid,ui.start_owner_pid);
  if(has_quit)PostQuitMessage(quit_code);
  else if(IsWindow(owner))
    SpiderGamepadUI_AfterControllerSetup(owner); /* arm only, no DI8 method */
  if(setup_report)setup_report("SDL3-SETUP: preview closed; optional explicit Start handoff queued for next safe foreground Flip");
}
