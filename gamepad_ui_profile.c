/*
 * SpiderMan2k1Fix / experimental DirectInput UI profile:
 * - Auto-detect ORIGINAL Sony DualShock 4 VID=054C product IDs 05C4,09CC,
 *   and the 0BA0 original adapter, strictly via DirectInput8 product GUID.
 * - Never modify the game's original DirectInput COM vtables. All seven
 *   original gameplay mappings survive. An OFF-by-default experiment may
 *   unbind only the original Start DWORD after exact signature checks.
 * - In legacy UI bridge mode, map configurable CROSS/OPTIONS edges to
 *   Enter. In opt-in PS1 preview, ONLY CROSS generates synthetic Enter in
 *   2D/Bink/native pause. Circle backs out, SHARE owns native 3D pause.
 * - Legacy TRIANGLE cinematic skipping stays game-owned. In PS1 preview,
 *   Jump moves to Cross; do not inject conflicting keys during 3D scripts.
 * - Bink movies can run after leaving the renderer's 2D menu mode.
 * - In 3D, OPTIONS requests Escape/Pause ONLY when the saved seven
 *   native gameplay bindings no longer assign BUTTON9 to an action.
 *   Factory Zipline=BUTTON9 therefore blocks Pause, not gameplay.
 *
 * SendInput integration with the original game's mixed native keyboard
 * paths is NOT yet verified under Wine or native Windows. This is a
 * diagnostic candidate. A failed independent Acquire/GetDeviceState
 * disables synthesis safely; never bypasses an original movie time gate.
 */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define COBJMACROS
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "gamepad_ui_profile.h"
#include "ps1_native_start_mapping.h"
#include "ps1_mouse_mode.h"
#include "ps1_post_quit_guard.h"
#include "ps1_transition_edge_ownership.h"
#include "ps1_menu_confirm.h"
#include "ps1_focus_pause_wake.h"
#include "ps1_ds4_hotplug.h"
#include "native_joystick_reconnect_policy.h"
#include "xbox360_menu_policy.h"
#include "xbox360_gameplay_mapping.h"
#include "xbox360_trigger_swing_policy.h"
#include "virtual_spidey_cfg.h"
#include "native_joystick_reconnect.h"
#include "ps1_input_flight_recorder.h"
/* Native UI code lives in ps1_joystick_readonly_menu.c. */

typedef HRESULT (WINAPI *SpiderDICreateFn)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
enum {SPIDER_PROFILE_OFF=0,SPIDER_PROFILE_AUTO=1,SPIDER_PROFILE_DUALSHOCK4=2,SPIDER_PROFILE_XBOX360=3};
enum {SPIDER_CONFIRM_CROSS=1,SPIDER_CONFIRM_OPTIONS=2,SPIDER_CONFIRM_BOTH=3};
enum {SPIDER_BACK_OFF=0,SPIDER_BACK_CIRCLE=1,SPIDER_BACK_SHARE=2};
enum {SPIDER_PAUSE_OFF=0,SPIDER_PAUSE_OPTIONS=1};
static const char *const native_actions[7]={
  "Jump","Kick","Punch","Web","Aim","Zipline","Swing"};
static const unsigned native_factory_buttons[7]={3u,4u,0u,1u,6u,9u,7u};

typedef struct {
  unsigned found,ds4_count,xbox_count;
  GUID ds4_instance,xbox_instance;
  DWORD product_id,xbox_pid;
  char name[MAX_PATH],xbox_name[MAX_PATH];
  unsigned short last_vendor,last_product;
  char last_name[MAX_PATH];
} SpiderDIDiscovery;

static int configured=0,profile=SPIDER_PROFILE_AUTO,confirm=SPIDER_CONFIRM_BOTH;
static unsigned detected_kind=SPIDER_RECONNECT_NONE;
static uint32_t xbox_previous_mask=0;
static unsigned xbox_button_trace_count=0;
/* Manual passive trigger-axis diagnostic; no gameplay injection or hooks. */
static int xbox_trigger_probe_requested=0;
/* Separate manual RT-to-Swing keyboard experiment (default OFF). */
static int xbox_rt_swing_requested=0;
static unsigned xbox_rt_swing_down_scan=0;
static unsigned xbox_rt_swing_denied_count=0;
static unsigned xbox_trigger_probe_samples=0;
static DWORD xbox_trigger_probe_tick=0;
static DIJOYSTATE2 xbox_trigger_previous={0};
static int xbox_trigger_probe_has_previous=0;
/* Xbox Joystick UI must be read-only like PS1. The native patch is
 * permanent for a process: attempt only once after original bootstrap,
 * and only for an unambiguously verified Xbox device. */
static int xbox_readonly_armed=0,xbox_readonly_attempted=0,xbox_readonly_active=0;
static int xbox360_preview_requested=0,xbox360_preview_active=0;
static void xbox_try_readonly_menu(void);
static int back=SPIDER_BACK_CIRCLE,pause_button=SPIDER_PAUSE_OPTIONS;
static int ps1_preview_requested=0,ps1_preview_active=0,previous_share=0;
static int ps1_options_skip_requested=0;

/* MouseEnabled is global; no DS4-dependent mouse state lives here. */
static int ps1_quit_trace_requested=0;
static int ps1_post_quit_guard_requested=0;
static int ps1_transition_edge_ownership_requested=0;
static SpiderPS1TransitionEdgeOwnership ps1_transition_ownership={0};
static int ps1_input_flight_recorder_requested=0;
static SpiderPS1InputHistory ps1_input_history={0};
static SpiderPS1PostQuitGuard ps1_post_quit_guard={0};
/* Xbox also needs native request-7 -> new-menu input ownership.
 * Keep separate state so Sony legacy options remain unchanged. */
static SpiderPS1PostQuitGuard xbox_post_quit_guard={0};
static SpiderNativeMenuReturnGate xbox_transition_ownership={0};
static unsigned xbox_return_to_menu_reports=0;
static unsigned ps1_quit_trace_count=0;
static int ps1_native_start_unbound=0,previous_native_start=0;
static unsigned ps1_native_start_reports=0;
static unsigned ps1_cutscene_reports=0;
static int probing=0,detected=0,previous_buttons=0,previous_back=0,previous_pause=0;
static unsigned active_scan=0;
static LONG sent_enter=0,sent_escape=0;
static char profile_ini[MAX_PATH]={0};
static unsigned pause_conflict_reports=0,outside_mode_reports=0;
static unsigned postmenu_bink_reports=0;
static unsigned bad_reads=0;
static SpiderDS4Hotplug hotplug={0};
static int hotplug_missing_logged=0;
static int hotplug_late_preview_reported=0;
static SpiderPausePadWake pause_wake={0};
static int pause_wake_active=0;
static int pause_wake_retry_logged=0;
/* A backgrounded non-exclusive DirectInput handle normally loses Acquire.
 * Permit exactly ONE guarded Acquire when foreground GetDeviceState reports
 * DIERR_NOTACQUIRED. Never Acquire on INPUTLOST/UNPLUGGED (hardware stale). */
static unsigned pause_wake_notacquired_attempts=0;
static IDirectInput8A *di=NULL;
static IDirectInputDevice8A *device=NULL;
static HMODULE di_module=NULL;
static void (*log_message)(const char *)=NULL;

static void report(const char *msg) {if(log_message)log_message(msg);}
static int ds4_pid(unsigned vid,unsigned pid) {
  return spider_reconnect_sony_ds4(vid,pid);
}
static int parse_native_button(const char *value,unsigned *button) {
  const char *p=value;
  char *end=NULL;
  unsigned long n;
  if(!p || !button)return 0;
  if(_strnicmp(p,"BUTTON",6)==0)p+=6;
  if(!*p || *p=='-' || *p=='+')return 0;
  n=strtoul(p,&end,0);
  /* 0x4000 is the game's native unbound button sentinel (NONE), not a conflict.
   * All other indices above the DI button range remain invalid. */
  if(!end || end==p || *end || (n>127ul && n!=0x4000ul))return 0;
  *button=(unsigned)n;
  return 1;
}
/* Before mapping OPTIONS to Escape, verify that the original game does
 * not assign BUTTON9 to ANY native gameplay action. A factory install
 * maps Zipline there, so this guard refuses Pause until user rebinds
 * Zipline in the game's Joystick settings (e.g. BUTTON5/R1). */
static int options_free_of_native_actions(void) {
  unsigned i;
  char value[50],fallback[24],line[240];
  if(!profile_ini[0])return 0;
  for(i=0;i<7u;i++) {
    unsigned button;
    snprintf(fallback,sizeof(fallback),"BUTTON%u",native_factory_buttons[i]);
    GetPrivateProfileStringA("Joystick",native_actions[i],fallback,
                             value,sizeof(value),profile_ini);
    if(!parse_native_button(value,&button) || button==9u) {
      if(pause_conflict_reports++<6u) {
        snprintf(line,sizeof(line),
          "GAMEPAD-UI: PAUSE BLOCKED: native [Joystick] %s=%s conflicts with OPTIONS/BUTTON9 or is unknown; rebind in game's Joystick menu, e.g. Zipline to R1/BUTTON5",
          native_actions[i],value);
        report(line);
      }
      return 0;
    }
  }
  return 1;
}
/* Xbox Back/Start (candidate DirectInput indices 6/7) must NEVER
 * synthesize ESC while the same native input fires a gameplay action.
 * Re-read persisted [Joystick] assignments on each fresh press. The
 * read-only Xbox menu cannot be used to resolve collisions; a future
 * separately validated runtime preset must do that. NONE (0x4000)
 * is unbound and harmless, but unknown values must fail closed. */
static int xbox_pause_button_free_of_native_actions(unsigned requested_button) {
  unsigned i;
  char value[50],fallback[24],line[245];
  /* Original persisted Start/Back assignments can differ from live RAM.
   * When our authenticated Xbox overlay is active, compare against its
   * actual seven buttons, not the player's persisted [Joystick] values. */
  if(xbox360_preview_active)
    return spider_xbox360_button_free_for_pause(requested_button);
  if(!profile_ini[0])return 0;
  for(i=0;i<7u;i++) {
    unsigned button;
    snprintf(fallback,sizeof(fallback),"BUTTON%u",native_factory_buttons[i]);
    GetPrivateProfileStringA("Joystick",native_actions[i],fallback,
                             value,sizeof(value),profile_ini);
    if(!parse_native_button(value,&button) || button==requested_button) {
      if(pause_conflict_reports++<12u) {
        snprintf(line,sizeof(line),
          "GAMEPAD-XBOX360: PAUSE BLOCKED physical BUTTON%u: native [Joystick] %s=%s conflicts or is invalid; Joystick is read-only, native gameplay mapping must be fixed before enabling this pause button",
          requested_button,native_actions[i],value);
        report(line);
      }
      return 0;
    }
  }
  return 1;
}
static BOOL CALLBACK enumerate_gamepad(const DIDEVICEINSTANCEA *id,VOID *ctx) {
  SpiderDIDiscovery *d=(SpiderDIDiscovery*)ctx;
  unsigned vid=(unsigned)(id->guidProduct.Data1&0xffffu);
  unsigned pid=(unsigned)((id->guidProduct.Data1>>16)&0xffffu);
  if(d->found<32u)d->found++;
  d->last_vendor=(unsigned short)vid;
  d->last_product=(unsigned short)pid;
  strncpy(d->last_name,id->tszProductName,sizeof(d->last_name)-1);
  d->last_name[sizeof(d->last_name)-1]=0;
  if(ds4_pid(vid,pid)) {
    d->ds4_count++;
    if(d->ds4_count==1u) {
      d->ds4_instance=id->guidInstance;
      d->product_id=(DWORD)pid;
      strncpy(d->name,id->tszProductName,sizeof(d->name)-1);
      d->name[sizeof(d->name)-1]=0;
    }
  } else if(spider_reconnect_xbox360(vid,pid)) {
    d->xbox_count++;
    if(d->xbox_count==1u) {
      d->xbox_instance=id->guidInstance;
      d->xbox_pid=(DWORD)pid;
      strncpy(d->xbox_name,id->tszProductName,sizeof(d->xbox_name)-1);
      d->xbox_name[sizeof(d->xbox_name)-1]=0;
    }
  }
  return DIENUM_CONTINUE;
}
/* Balance precisely the scan code injected at the previous Flip,
 * including across any focus-loss pause. Never leave Escape held. */
static void release_active_key(void) {
  INPUT in;
  char line[160];
  if(!active_scan)return;
  memset(&in,0,sizeof(in));
  in.type=INPUT_KEYBOARD;
  in.ki.wScan=(WORD)active_scan;
  in.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;
  SpiderNativeReconnect_LogSyntheticMenuKey(active_scan,"before KEYUP");
  if(SendInput(1,&in,sizeof(in))!=1) {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: WARNING synthetic KEYUP failed scan=0x%02X Win32=%lu",
      active_scan,(unsigned long)GetLastError());
    report(line);
  }
  SpiderNativeReconnect_LogSyntheticMenuKey(active_scan,"after KEYUP");
  active_scan=0;
}
static void begin_scancode(unsigned scan,const char *action) {
  INPUT in;
  char line[185];
  LONG number;
  memset(&in,0,sizeof(in));
  in.type=INPUT_KEYBOARD;
  in.ki.wScan=(WORD)scan;
  in.ki.dwFlags=KEYEVENTF_SCANCODE;
  SpiderNativeReconnect_LogSyntheticMenuKey(scan,"before KEYDOWN");
  if(SendInput(1,&in,sizeof(in))!=1) {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: WARNING %s KEYDOWN SendInput failed Win32=%lu",
      action,(unsigned long)GetLastError());
    report(line);
    return;
  }
  active_scan=scan;
  SpiderNativeReconnect_LogSyntheticMenuKey(scan,"after KEYDOWN");
  number=scan==0x01u?++sent_escape:++sent_enter;
  if(number<=20) {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: SYNTHETIC %s (DIK 0x%02X) down #%ld; release next Flip; native menu/Bink/guarded 3D scope",
      action,scan,(long)number);
    report(line);
  }
}
/* Keep held RT-to-Swing key independent from momentary menu keys.
 * The injected DIK scan is dynamically read from the authenticated game
 * config and released on every focus/input/context exit, never in a loop. */
static void xbox_rt_swing_release(const char *why) {
  INPUT in;
  unsigned scan=xbox_rt_swing_down_scan;
  char line[210];
  if(!scan)return;
  memset(&in,0,sizeof(in));
  in.type=INPUT_KEYBOARD;
  in.ki.wScan=(WORD)scan;
  in.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;
  if(SendInput(1,&in,sizeof(in))!=1) {
    snprintf(line,sizeof(line),
      "GAMEPAD-XBOX360: RT-SWING KEYUP FAILED DIK 0x%02X reason=%s Win32=%lu; retry on next Flip",
      scan,why?why:"unknown",(unsigned long)GetLastError());
    report(line);
    return;
  }
  xbox_rt_swing_down_scan=0;
  snprintf(line,sizeof(line),
    "GAMEPAD-XBOX360: RT-SWING UP DIK 0x%02X reason=%s",
    scan,why?why:"released");
  report(line);
}
static void xbox_rt_swing_update(long axis,int gameplay_ok) {
  unsigned scan;
  INPUT in;
  char line[210];
  int should_hold;
  int native_canonical=SpiderNativeReconnect_CanonicalXboxMapping();
  SpiderXbox360_SetRTAlias(xbox_rt_swing_requested &&
    detected_kind==SPIDER_RECONNECT_XBOX360 &&
    (xbox360_preview_active || native_canonical));
  /* Startup Xbox profile OR a successfully reconnected original Xbox
   * whose live 14-action table matches our canonical seven actions.
   * A Sony-at-startup -> Xbox-at-runtime game MUST NOT depend on a
   * bootstrap-only xbox360_preview_active flag which can never change.
   * If the original game table is CUSTOM, the hot-swap path fails closed. */
  if(!xbox_rt_swing_requested ||
     (!xbox360_preview_active && !native_canonical) ||
     detected_kind!=SPIDER_RECONNECT_XBOX360 || !gameplay_ok) {
    xbox_rt_swing_release("inactive game/focus/pause/controller or unverified native Xbox map");
    return;
  }
  should_hold=spider_xbox360_rt_swing_held(
    xbox_rt_swing_down_scan!=0,axis);
  if(!should_hold) {
    xbox_rt_swing_release("RT released or axis neutral/positive");
    return;
  }
  if(xbox_rt_swing_down_scan)return;
  /* The native keyboard mapping is editable even when Joystick is not.
   * Use the game-owned live keyboard table after a native hot-swap;
   * stale virtual Spidey.cfg or the Xbox-only bootstrap flag is not
   * authoritative after a Sony boot. */
  scan=native_canonical?SpiderCfg_Xbox360SwingScanLive():
                        SpiderCfg_Xbox360SwingScan();
  if(!scan) {
    if(xbox_rt_swing_denied_count++<5u)
      report("GAMEPAD-XBOX360: RT-SWING REFUSED: live original Swing keyboard DIK unset/unsafe/duplicates another action or CRT unverified");
    return;
  }
  memset(&in,0,sizeof(in));
  in.type=INPUT_KEYBOARD;
  in.ki.wScan=(WORD)scan;
  in.ki.dwFlags=KEYEVENTF_SCANCODE;
  if(SendInput(1,&in,sizeof(in))!=1) {
    snprintf(line,sizeof(line),
      "GAMEPAD-XBOX360: RT-SWING KEYDOWN FAILED DIK 0x%02X Win32=%lu",
      scan,(unsigned long)GetLastError());
    report(line);
    return;
  }
  xbox_rt_swing_down_scan=scan;
  snprintf(line,sizeof(line),
    "GAMEPAD-XBOX360: RT-SWING DOWN lZ=%ld DIK=0x%02X (held until release/focus/context change; LT high axis ignored)",
    axis,scan);
  report(line);
}
static void disable_profile_with_policy(const char *reason,
                                        int already_inputlost) {
  release_active_key();
  xbox_rt_swing_release("secondary controller disabled");
  if(device) {
    IDirectInputDevice8A *old=device;
    device=NULL;
    if(already_inputlost)
      report("GAMEPAD-HOTPLUG: secondary stale device Release BEGIN (skip Unacquire after INPUTLOST)");
    else IDirectInputDevice8_Unacquire(old);
    IDirectInputDevice8_Release(old);
    if(already_inputlost)
      report("GAMEPAD-HOTPLUG: secondary stale device Release COMPLETE");
  }
  if(di) {IDirectInput8_Release(di);di=NULL;}
  if(di_module) {FreeLibrary(di_module);di_module=NULL;}
  detected=0;
  detected_kind=SPIDER_RECONNECT_NONE;
  xbox_previous_mask=0;
  memset(&xbox_post_quit_guard,0,sizeof(xbox_post_quit_guard));
  memset(&xbox_transition_ownership,0,sizeof(xbox_transition_ownership));
  xbox_trigger_probe_has_previous=0;
  xbox_trigger_probe_samples=0;
  if(reason)report(reason);
}
static void disable_profile(const char *reason) {
  disable_profile_with_policy(reason,0);
}
/* Called only after an independent DirectInput read failed while foreground.
 * A physical unplug can invalidate the old handle immediately: DO NOT
 * Acquire() that stale device in the same failing frame. Record release
 * checkpoints so that a fault in the old game's own input path remains
 * distinguishable from one in our secondary device shutdown. */
static void spider_hotplug_drop_device(HRESULT hr,const char *context) {
  char line[235];
  snprintf(line,sizeof(line),
    "GAMEPAD-HOTPLUG: %s hr=0x%08lX; secondary device release BEGIN (no Acquire on failed read)",
    context,(unsigned long)hr);
  report(line);
  disable_profile_with_policy(NULL,hr==DIERR_INPUTLOST);
  report("GAMEPAD-HOTPLUG: secondary device release COMPLETE; rescan deferred 2s");
  probing=1;
  hotplug.scan_started=1u;
  hotplug.last_scan=GetTickCount();
  hotplug_missing_logged=0;
}
void SpiderGamepadUI_Init(const char *ini,void (*logger)(const char *)) {
  char choice[40],buttons[40],back_choice[40],pause_choice[40],line[260];
  log_message=logger;
  configured=1;
  if(!ini||!ini[0]) {
    profile=SPIDER_PROFILE_OFF;
    report("GAMEPAD-UI: OFF no configuration path");
    return;
  }
  if(strlen(ini)>=sizeof(profile_ini)) {
    profile=SPIDER_PROFILE_OFF;
    report("GAMEPAD-UI: OFF INI path too long");
    return;
  }
  strcpy(profile_ini,ini);
  /* Experimental only: no automatic fixed-profile activation until native
   * menu lock and Start handling are validated in the real game. */
  ps1_preview_requested=GetPrivateProfileIntA("Input","ControllerPS1Preview",0,ini)==1;
  xbox360_preview_requested=GetPrivateProfileIntA("Input","ControllerXbox360Preview",0,ini)==1;
  xbox_trigger_probe_requested=GetPrivateProfileIntA("Debug","Xbox360TriggerProbe",0,ini)==1;
  xbox_rt_swing_requested=GetPrivateProfileIntA("Input","ControllerXbox360RTSwing",0,ini)==1;
  ps1_options_skip_requested=GetPrivateProfileIntA("Input","ControllerPS1OptionsSkip",0,ini)==1;
  /* ControllerPS1ReadOnlyJoystick is legacy. A supported detected
   * controller now always gets a read-only Joystick menu. */
  /* Global game-only mouse policy replaces the retired DS4-only option.
   * Absent MouseEnabled is intentionally OFF for existing/fresh INIs.
   * MouseEnabled=1 explicitly restores normal mouse menu interaction. */
  SpiderPS1Mouse_Configure(GetPrivateProfileIntA("Input","MouseEnabled",0,ini)==1,logger);
  if(GetPrivateProfileStringA("Input","ControllerAutoDisableMouse","",choice,sizeof(choice),ini))
    report("GAMEPAD-MOUSE: ControllerAutoDisableMouse is retired; MouseEnabled=0/1 now controls the game mouse for all players");
  ps1_quit_trace_requested=GetPrivateProfileIntA("Input","ControllerPS1QuitTrace",0,ini)==1;
  ps1_post_quit_guard_requested=GetPrivateProfileIntA("Input","ControllerPS1PostQuitGuard",0,ini)==1;
  ps1_transition_edge_ownership_requested=GetPrivateProfileIntA("Input","ControllerPS1TransitionEdgeOwnership",0,ini)==1;
  memset(&ps1_transition_ownership,0,sizeof(ps1_transition_ownership));
  ps1_input_flight_recorder_requested=GetPrivateProfileIntA("Input","ControllerPS1InputFlightRecorder",0,ini)==1;
  memset(&ps1_input_history,0,sizeof(ps1_input_history));
  memset(&ps1_post_quit_guard,0,sizeof(ps1_post_quit_guard));
  memset(&xbox_post_quit_guard,0,sizeof(xbox_post_quit_guard));
  memset(&xbox_transition_ownership,0,sizeof(xbox_transition_ownership));
  xbox_return_to_menu_reports=0;
  GetPrivateProfileStringA("Input","ControllerProfile","Auto",choice,sizeof(choice),ini);
  if(!_stricmp(choice,"Off"))profile=SPIDER_PROFILE_OFF;
  else if(!_stricmp(choice,"Auto"))profile=SPIDER_PROFILE_AUTO;
  else if(!_stricmp(choice,"DualShock4"))profile=SPIDER_PROFILE_DUALSHOCK4;
  else if(!_stricmp(choice,"Xbox360"))profile=SPIDER_PROFILE_XBOX360;
  else {profile=SPIDER_PROFILE_OFF;report("GAMEPAD-UI: OFF invalid ControllerProfile");}
  GetPrivateProfileStringA("Input","ControllerEnterButtons","Both",buttons,sizeof(buttons),ini);
  if(!_stricmp(buttons,"Cross"))confirm=SPIDER_CONFIRM_CROSS;
  else if(!_stricmp(buttons,"Options"))confirm=SPIDER_CONFIRM_OPTIONS;
  else if(!_stricmp(buttons,"Both"))confirm=SPIDER_CONFIRM_BOTH;
  else {confirm=SPIDER_CONFIRM_BOTH;report("GAMEPAD-UI: invalid ControllerEnterButtons; using Both");}
  GetPrivateProfileStringA("Input","ControllerBackButton","Circle",
                           back_choice,sizeof(back_choice),ini);
  if(!_stricmp(back_choice,"Off"))back=SPIDER_BACK_OFF;
  else if(!_stricmp(back_choice,"Circle"))back=SPIDER_BACK_CIRCLE;
  else if(!_stricmp(back_choice,"Share"))back=SPIDER_BACK_SHARE;
  else {back=SPIDER_BACK_CIRCLE;report("GAMEPAD-UI: invalid ControllerBackButton; using Circle");}
  GetPrivateProfileStringA("Input","ControllerPauseButton","Options",
                           pause_choice,sizeof(pause_choice),ini);
  if(!_stricmp(pause_choice,"Off"))pause_button=SPIDER_PAUSE_OFF;
  else if(!_stricmp(pause_choice,"Options"))pause_button=SPIDER_PAUSE_OPTIONS;
  else {pause_button=SPIDER_PAUSE_OFF;report("GAMEPAD-UI: invalid ControllerPauseButton; disabling pause bridge");}
  if(GetPrivateProfileIntA("Input","ExperimentalGamepadStart",-1,ini)!=-1 ||
     GetPrivateProfileStringA("Input","ExperimentalGamepadStart","",choice,sizeof(choice),ini)!=0)
    report("GAMEPAD-UI: old ExperimentalGamepadStart is RETIRED/ignored; remove BUTTON8 setting");
  snprintf(line,sizeof(line),
    "GAMEPAD-UI: config profile=%s legacy-confirm=%s legacy-back=%s legacy-pause=%s ps1preview-requested=%d options3dskip=%d Return=DIK_0x1C Escape=DIK_0x01",
    profile==SPIDER_PROFILE_OFF?"Off":profile==SPIDER_PROFILE_AUTO?"Auto":
      profile==SPIDER_PROFILE_XBOX360?"Xbox360":"DualShock4",
    confirm==SPIDER_CONFIRM_CROSS?"Cross":confirm==SPIDER_CONFIRM_OPTIONS?"Options":"Both",
    back==SPIDER_BACK_OFF?"Off":back==SPIDER_BACK_CIRCLE?"Circle":"Share",
    pause_button==SPIDER_PAUSE_OFF?"Off":"Options (conflict guarded)",
    ps1_preview_requested,ps1_options_skip_requested);
  report(line);
  report(xbox360_preview_requested?
    "GAMEPAD-XBOX360: PREVIEW REQUESTED (opt-in, unique Xbox + native readonly + CRT bootstrap required)":
    "GAMEPAD-XBOX360: PREVIEW OFF (default, original 3D bindings retained)");
  report(xbox_rt_swing_requested?
    "GAMEPAD-XBOX360: RT-SWING REQUESTED negative lZ -> held native keyboard Swing; requires original Xbox preview OR canonical live Xbox hot-swap mapping":
    "GAMEPAD-XBOX360: RT-SWING OFF (default)");
}
static void detect_device(HWND hwnd) {
  SpiderDIDiscovery d;
  SpiderDICreateFn create;
  HRESULT hr;
  int choose_ds4,choose_xbox;
  char line[300];
  HINSTANCE game=GetModuleHandleA(NULL);
  probing=1;
  hotplug.scan_started=1u;
  hotplug.last_scan=GetTickCount();
  if(profile==SPIDER_PROFILE_OFF) {
    report("GAMEPAD-UI: OFF; original gamepad controls untouched");
    return;
  }
  di_module=LoadLibraryA("dinput8.dll");
  create=di_module?(SpiderDICreateFn)GetProcAddress(di_module,"DirectInput8Create"):NULL;
  if(!create) {disable_profile("GAMEPAD-UI: unavailable dinput8.dll enumeration; original controls preserved");return;}
  hr=create(game,DIRECTINPUT_VERSION,&IID_IDirectInput8A,(void**)&di,NULL);
  if(FAILED(hr)||!di) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: DirectInput8Create failed hr=0x%08lX; original controls preserved",(unsigned long)hr);
    disable_profile(line);return;
  }
  memset(&d,0,sizeof(d));
  hr=IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL,enumerate_gamepad,
                               &d,DIEDFL_ATTACHEDONLY);
  if(FAILED(hr)) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: EnumDevices failed hr=0x%08lX; original controls preserved",(unsigned long)hr);
    disable_profile(line);return;
  }
  if(!hotplug_missing_logged || d.found==1u) {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: DISCOVERY gamepads=%u SonyDualShock4=%u Xbox360=%u last-product=%04X:%04X last-name=%.110s",
      d.found,d.ds4_count,d.xbox_count,(unsigned)d.last_vendor,(unsigned)d.last_product,d.last_name);
    report(line);
  }
  /* The original game picks its controller independently. Never select
   * any UI bridge when there are two attached controllers, a mismatched
   * forced profile, or an unknown product GUID. Xbox is never PS1. */
  choose_ds4=d.found==1u && d.ds4_count==1u &&
             (profile==SPIDER_PROFILE_AUTO || profile==SPIDER_PROFILE_DUALSHOCK4);
  choose_xbox=d.found==1u && d.xbox_count==1u &&
              (profile==SPIDER_PROFILE_AUTO || profile==SPIDER_PROFILE_XBOX360);
  if(!choose_ds4 && !choose_xbox) {
    if(!hotplug_missing_logged)
      disable_profile("GAMEPAD-UI: no unique allowed DS4/Xbox360; auto bridge inactive; game mappings unchanged; hotplug rescan every 2s while foreground");
    else disable_profile(NULL);
    hotplug_missing_logged=1;
    return;
  }
  hr=IDirectInput8_CreateDevice(di,choose_ds4?&d.ds4_instance:&d.xbox_instance,&device,NULL);
  if(FAILED(hr)||!device) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: CreateDevice failed hr=0x%08lX",(unsigned long)hr);
    disable_profile(line);return;
  }
  hr=IDirectInputDevice8_SetDataFormat(device,&c_dfDIJoystick2);
  if(SUCCEEDED(hr))
    hr=IDirectInputDevice8_SetCooperativeLevel(device,hwnd,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE);
  if(FAILED(hr)) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: device format/cooperative level failed hr=0x%08lX",(unsigned long)hr);
    disable_profile(line);return;
  }
  hr=IDirectInputDevice8_Acquire(device);
  if(FAILED(hr)) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: secondary nonexclusive Acquire failed hr=0x%08lX; original controller untouched",(unsigned long)hr);
    disable_profile(line);return;
  }
  detected=1;
  detected_kind=choose_ds4?SPIDER_RECONNECT_DS4:SPIDER_RECONNECT_XBOX360;
  xbox_previous_mask=0;
  xbox_button_trace_count=0;
  xbox_trigger_probe_has_previous=0;
  xbox_trigger_probe_samples=0;
  hotplug_missing_logged=0;
  spider_ds4_hotplug_attached(&hotplug);
  previous_buttons=0;previous_back=0;previous_pause=0;
  previous_share=0;previous_native_start=0;
  if(choose_ds4) {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: PROFILE DualShock4 ACTIVE (verified device) Sony VID=054C PID=%04lX name=%.110s; legacy 2D/Bink confirmation=ControllerEnterButtons; PS1 preview after CRT bootstrap: Cross=BUTTON1 only, Circle=BUTTON2 back, SHARE=BUTTON8 3D pause; OPTIONS=BUTTON9 is NOT synthetic Enter in PS1",
      (unsigned long)d.product_id,d.name);
  } else {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: PROFILE Xbox360 EXPERIMENTAL ACTIVE VID=045E PID=%04lX name=%.110s; A=BUTTON0 Enter B=BUTTON1 Escape (candidate; check BUTTON-EDGE logs); original 3D gameplay unchanged; DS4 PS1 preview excluded",
      (unsigned long)d.xbox_pid,d.xbox_name);
  }
  report(line);
}
/* This is the ORIGINAL, game-owned CutSceneSkipAllow state machine.
 * The same addresses and per-scene conditions were mapped in the existing
 * F10 diagnostics (quick_exit_f12.c). Never guess 3D from rendering.
 * Memory reads require a readable COMMITTED owner span, and the write
 * uses ONLY the original game skip-request byte after its own permission.
 * Manually opted in (ControllerPS1OptionsSkip=1), not the Auto default.
 * The user's Cross is already mapped to native Jump, which the game
 * uses for 3D cinematic skip; it does NOT need a synthetic key here. */
static int ps1_request_native_options_cutscene_skip(void) {
  const DWORD owner=*(const volatile DWORD *)(uintptr_t)0x006A9038u;
  const volatile BYTE *obj;
  volatile BYTE *native_request=(volatile BYTE *)(uintptr_t)0x0060CFC4u;
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t base,where;
  BYTE scene_context,skip_allowed,script_active;
  DWORD script_ip;
  if(owner<0x00010000u ||
     VirtualQuery((LPCVOID)(uintptr_t)owner,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT ||
     (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
     !((mbi.Protect&0xffu)==PAGE_READONLY ||
       (mbi.Protect&0xffu)==PAGE_READWRITE ||
       (mbi.Protect&0xffu)==PAGE_WRITECOPY ||
       (mbi.Protect&0xffu)==PAGE_EXECUTE_READ ||
       (mbi.Protect&0xffu)==PAGE_EXECUTE_READWRITE ||
       (mbi.Protect&0xffu)==PAGE_EXECUTE_WRITECOPY))return 0;
  base=(uintptr_t)mbi.BaseAddress;
  where=(uintptr_t)owner;
  if(where<base || where-base>mbi.RegionSize ||
     mbi.RegionSize-(where-base)<0x1bcu)return 0;
  obj=(const volatile BYTE *)where;
  scene_context=obj[0x1acu];
  skip_allowed=obj[0x1a4u];
  script_active=obj[0x1b4u];
  script_ip=*(const volatile DWORD *)(where+0x1b8u);
  if(scene_context!=1u || skip_allowed!=1u || script_active!=1u ||
     script_ip==0u || *native_request!=0u)return 0;
  /* The exact game-native 0x4C66A6 writer uses this one-byte request;
   * original 0x4BC3E0 consumes it and cleans up scene resources. */
  *native_request=1u;
  report("GAMEPAD-UI: PS1-3D-OPTIONS native CutSceneSkipAllow=1 request=1 (original 0x4BC3E0 cleanup; NOT forced)");
  return 1;
}
/* Early game HWND acquisition precedes the verified native CRT reload.
 * On failure retry the old unchanged UI discovery on the first Flip. */
int SpiderGamepadUI_PreparePS1(HWND hwnd) {
  if(!configured || !ps1_preview_requested || profile==SPIDER_PROFILE_OFF ||
     !hwnd || !IsWindow(hwnd) || GetForegroundWindow()!=hwnd)return 0;
  if(!probing)detect_device(hwnd);
  /* A verified Xbox pad is deliberately NOT a Sony PS1 preview.
   * Keep its acquired secondary DirectInput device and probing=1;
   * resetting probing here would recreate/leak the Xbox COM device
   * on the first Flip while the original game is using the pad. */
  if(detected && detected_kind==SPIDER_RECONNECT_XBOX360) {
    report("GAMEPAD-XBOX360: PS1-PREVIEW skipped (Sony-only); retained Xbox UI device for menu A/B");
    return 0;
  }
  if(!detected){
    probing=0;
    report("GAMEPAD-UI: PS1-PREVIEW deferred: early DS4 device acquisition failed; original game mappings untouched");
    return 0;
  }
  report("GAMEPAD-UI: PS1-PREVIEW Sony DS4 acquired before native config bootstrap");
  return 1;
}
/* Pre-bootstrap Xbox discovery: refuse RAM mapping unless the unique
 * physical Xbox and the exact native joystick readonly menu are ready.
 * Do not retrofit a game profile after its original startup config load. */
int SpiderGamepadUI_PrepareXbox360(HWND hwnd) {
  if(!configured || !xbox360_preview_requested || profile==SPIDER_PROFILE_OFF ||
     profile==SPIDER_PROFILE_DUALSHOCK4 || !hwnd || !IsWindow(hwnd) ||
     !IsWindowVisible(hwnd) || GetForegroundWindow()!=hwnd)return 0;
  if(!probing)detect_device(hwnd);
  if(!detected || !device || detected_kind!=SPIDER_RECONNECT_XBOX360) {
    report("GAMEPAD-XBOX360: PREVIEW REFUSED no uniquely verified early Xbox controller");
    return 0;
  }
  xbox_readonly_armed=1;
  xbox_try_readonly_menu();
  if(!xbox_readonly_active) {
    report("GAMEPAD-XBOX360: PREVIEW REFUSED native Joystick readonly patch unavailable");
    return 0;
  }
  report("GAMEPAD-XBOX360: PREVIEW EARLY PREFLIGHT OK unique Xbox + native readonly; waiting for authenticated CRT read");
  return 1;
}
/* The retired native BUTTON11 Start remap never reached its table scan:
 * its arbitrary 32 MiB PE SizeOfImage cap rejected the user's retail EXE.
 * Read-only, one-shot replacement: examine verified writable .data pages
 * and record exact 14-entry candidate counts. No mapping writes, no hooks.
 * A positive count is still NOT proof that modifying this mapping is safe. */
/* The normal probe reads at most 1 MiB; the read-only extended
 * scan is manually opted in because this 2001 retail PE maps a 42 MiB
 * writable data section. Query each committed region independently; never
 * cross a region boundary or touch game-owned input mapping bytes.
 *
 * A complete scan could still miss maps stored in another section or
 * synthesized at runtime. Do NOT use the result as permission to patch.
 */
static void ps1_probe_native_start_tables(void) {
  static const DWORD actions[14]={
    0x1u,0x2u,0x4u,0x8u,0x10u,0x20u,0x40u,0x80u,
    0x100u,0x200u,0x400u,0x1000u,0x2000u,0x8000u};
  BYTE *base=(BYTE *)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *pe;
  IMAGE_SECTION_HEADER *sec;
  unsigned j,sections=0,matches=0,truncated=0,regions=0,strict_matches=0;
  BYTE *strict_candidate=NULL;
  int deep=profile_ini[0] &&
    GetPrivateProfileIntA("Input","ControllerPS1DeepStartProbe",0,profile_ini)==1;
  int disable_native_start=profile_ini[0] &&
    GetPrivateProfileIntA("Input","ControllerPS1DisableNativeStart",0,profile_ini)==1;
  const size_t max_bytes=deep?(size_t)(64u*1024u*1024u):(size_t)(1024u*1024u);
  char line[270];
  if(!base || (uintptr_t)base!=0x00400000u)return;
  dos=(IMAGE_DOS_HEADER *)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE ||
     dos->e_lfanew<=0 || dos->e_lfanew>0x1000) {
    report("GAMEPAD-PS1: START-TABLE read-only probe refused invalid DOS header");
    return;
  }
  pe=(IMAGE_NT_HEADERS32 *)(base+dos->e_lfanew);
  if(pe->Signature!=IMAGE_NT_SIGNATURE ||
     pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     pe->OptionalHeader.ImageBase!=0x00400000u ||
     pe->OptionalHeader.SizeOfImage<0x00140000u ||
     pe->OptionalHeader.SizeOfImage>0x10000000u ||
     pe->FileHeader.NumberOfSections<1u ||
     pe->FileHeader.NumberOfSections>32u) {
    snprintf(line,sizeof(line),
      "GAMEPAD-PS1: START-TABLE read-only probe unsupported PE image=0x%08lX sections=%u base=0x%08lX (NO writes)",
      (unsigned long)pe->OptionalHeader.SizeOfImage,
      (unsigned)pe->FileHeader.NumberOfSections,
      (unsigned long)pe->OptionalHeader.ImageBase);
    report(line);
    return;
  }
  sec=IMAGE_FIRST_SECTION(pe);
  for(j=0;j<(unsigned)pe->FileHeader.NumberOfSections;j++) {
    DWORD span=sec[j].Misc.VirtualSize;
    size_t cursor=0,budget;
    BYTE *data;
    if(memcmp(sec[j].Name,".data",5)!=0 ||
       !(sec[j].Characteristics&IMAGE_SCN_MEM_WRITE) ||
       span<14u*8u ||
       sec[j].VirtualAddress>=pe->OptionalHeader.SizeOfImage ||
       span>pe->OptionalHeader.SizeOfImage-sec[j].VirtualAddress)
      continue;
    sections++;
    data=base+sec[j].VirtualAddress;
    budget=span;
    if(budget>max_bytes){budget=max_bytes;truncated++;}
    while(cursor+14u*8u<=budget) {
      MEMORY_BASIC_INFORMATION mbi;
      uintptr_t here=(uintptr_t)(data+cursor),region_start,region_end;
      size_t safe,offset;
      DWORD protect;
      unsigned k;
      if(VirtualQuery((LPCVOID)here,&mbi,sizeof(mbi))!=sizeof(mbi) ||
         !mbi.RegionSize) {
        truncated++;
        break;
      }
      region_start=(uintptr_t)mbi.BaseAddress;
      if(here<region_start || mbi.RegionSize>(size_t)(~(uintptr_t)0)-region_start) {
        truncated++;
        break;
      }
      region_end=region_start+mbi.RegionSize;
      if(region_end<=here){truncated++;break;}
      safe=region_end-here;
      if(safe>budget-cursor)safe=budget-cursor;
      protect=mbi.Protect&0xffu;
      regions++;
      if(mbi.State==MEM_COMMIT &&
         !(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
         (protect==PAGE_READWRITE || protect==PAGE_EXECUTE_READWRITE ||
          protect==PAGE_WRITECOPY || protect==PAGE_EXECUTE_WRITECOPY)) {
        /* All sampled DWORDs must fit within this single readable
         * allocation region. Native tables spanning a page split are
         * intentionally omitted, never accessed speculatively. */
        for(offset=0;offset+14u*8u<=safe;offset+=4u) {
          const DWORD *v=(const DWORD *)(const void *)(data+cursor+offset);
          if(v[0]!=actions[0])continue;
          for(k=1u;k<14u;k++)if(v[2u*k]!=actions[k])break;
          if(k==14u && v[2u*11u+1u]==11u) {
            matches++;
            if(spider_ps1_start_strict_factory((const uint8_t *)(const void *)v,
                                               safe-offset)) {
              strict_matches++;
              if(strict_matches==1u)strict_candidate=(BYTE *)(void *)v;
            }
            if(matches<=10u) {
              snprintf(line,sizeof(line),
                "GAMEPAD-PS1: START-TABLE candidate=%u RVA=0x%08lX Start=BUTTON11 [READ-ONLY; NO table edits]",
                matches,(unsigned long)(sec[j].VirtualAddress+cursor+offset));
              report(line);
            }
          }
        }
      } else {
        truncated++;
      }
      cursor+=safe;
    }
    if(budget>cursor && cursor+14u*8u>budget) {
      /* This last sub-112-byte tail is intentionally skipped. */
    }
  }
  snprintf(line,sizeof(line),
    "GAMEPAD-PS1: START-TABLE probe data-sections=%u candidates=%u strict=%u truncated-regions=%u PE-image=0x%08lX deep=%d queried-regions=%u cap-MiB=%u; diagnostic scan only",
    sections,matches,strict_matches,truncated,(unsigned long)pe->OptionalHeader.SizeOfImage,
    deep,regions,(unsigned)(max_bytes/(1024u*1024u)));
  report(line);
  if(disable_native_start) {
    /* A deliberate separate experiment. Fail closed if default 1 MiB
     * scan was used, any memory was skipped, multiple native patterns
     * were found, or the mapping doesn't match the entire original
     * factory structure. No COM/vtable hooks or PE code patching. */
    if(!deep || truncated || sections!=1u || matches!=1u ||
       strict_matches!=1u || !strict_candidate ||
       strict_candidate!=base+0x00168408u) {
      snprintf(line,sizeof(line),
        "GAMEPAD-PS1: NATIVE-START-UNBIND REFUSED deep=%d sections=%u matches=%u strict=%u truncated=%u (original BUTTON11 retained)",
        deep,sections,matches,strict_matches,truncated);
      report(line);
    } else {
      BYTE *slot=strict_candidate+SPIDER_PS1_START_BUTTON_OFFSET;
      MEMORY_BASIC_INFORMATION mbi;
      uintptr_t here=(uintptr_t)slot,region;
      DWORD prot;
      if((here&3u) ||
         VirtualQuery(slot,&mbi,sizeof(mbi))!=sizeof(mbi) ||
         mbi.State!=MEM_COMMIT ||
         (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))) {
        report("GAMEPAD-PS1: NATIVE-START-UNBIND REFUSED unsafe memory protection/alignment");
      } else {
        region=(uintptr_t)mbi.BaseAddress;
        prot=mbi.Protect&0xffu;
        if(here<region || here-region>=mbi.RegionSize ||
           mbi.RegionSize-(here-region)<sizeof(LONG) ||
           !(prot==PAGE_READWRITE || prot==PAGE_EXECUTE_READWRITE ||
             prot==PAGE_WRITECOPY || prot==PAGE_EXECUTE_WRITECOPY) ||
           !spider_ps1_start_strict_factory((const uint8_t *)strict_candidate,
                                          SPIDER_PS1_START_MAP_BYTES)) {
          report("GAMEPAD-PS1: NATIVE-START-UNBIND REFUSED mapping changed or slot not safely writable");
        } else if(InterlockedCompareExchange(
                    (volatile LONG *)(void *)slot,
                    (LONG)SPIDER_PS1_START_BUTTON_UNBOUND,
                    (LONG)SPIDER_PS1_START_BUTTON_FACTORY)==
                  (LONG)SPIDER_PS1_START_BUTTON_FACTORY) {
          snprintf(line,sizeof(line),
            "GAMEPAD-PS1: NATIVE-START-UNBIND EXPERIMENT ACTIVE RVA=0x%08lX Start BUTTON11 -> native unused 0x4000; one validated RAM dword; game restart restores; SHARE remains menu control",
            (unsigned long)((uintptr_t)slot-(uintptr_t)base));
          ps1_native_start_unbound=1;
          report(line);
        } else {
          report("GAMEPAD-PS1: NATIVE-START-UNBIND REFUSED Start slot modified concurrently");
        }
      }
    }
  }
}
static void xbox_try_readonly_menu(void) {
  if(!xbox_readonly_armed || xbox_readonly_attempted || !detected ||
     detected_kind!=SPIDER_RECONNECT_XBOX360 || profile==SPIDER_PROFILE_OFF)
    return;
  xbox_readonly_attempted=1;
  if(!SpiderXbox360_InstallReadOnlyJoystick(log_message))
    report("GAMEPAD-XBOX360: JOYSTICK-READONLY REFUSED: original machine-code signatures not matched or patch installation failed; menu may still be editable");
  else xbox_readonly_active=1;
}
/* Joystick Configuration is a viewer whenever a supported physical
 * controller is detected. This lock is NOT conditional on the optional
 * seven-action gameplay preview; the native game action list is shown
 * without permitting rebinding, while Keyboard remains editable. */
void SpiderGamepadUI_ArmPS1ReadOnly(void) {
  if(!configured || profile==SPIDER_PROFILE_OFF ||
     profile==SPIDER_PROFILE_XBOX360 || !detected || !device ||
     detected_kind!=SPIDER_RECONNECT_DS4)return;
  if(!SpiderPS1_InstallReadOnlyJoystick(log_message))
    report("GAMEPAD-PS1: JOYSTICK-READONLY inactive; exact native signatures missing or failed to install");
}
void SpiderGamepadUI_ArmXbox360ReadOnly(void) {
  if(!configured || profile==SPIDER_PROFILE_OFF ||
     profile==SPIDER_PROFILE_DUALSHOCK4)return;
  xbox_readonly_armed=1;
  xbox_try_readonly_menu();
}
void SpiderGamepadUI_ActivateXbox360(void) {
  if(!configured || !xbox360_preview_requested || !xbox_readonly_active ||
     !detected || !device || detected_kind!=SPIDER_RECONNECT_XBOX360)return;
  xbox360_preview_active=1;
  report("GAMEPAD-XBOX360: PREVIEW ACTIVE game CRT consumed RAM-only seven-action profile; A Jump; Back BUTTON6 provides Pause; Start BUTTON7 NOT mapped by bridge; INI preserved; physical non-A/B unverified");
}
void SpiderGamepadUI_ActivatePS1(void){
  if(!detected || detected_kind!=SPIDER_RECONNECT_DS4 || !ps1_preview_requested)return;
  ps1_preview_active=1;
  back=SPIDER_BACK_CIRCLE;
  report("GAMEPAD-UI: PS1-PREVIEW ACTIVE, fixed gameplay RAM only; CROSS-only 2D/Bink/pause confirmation (OPTIONS/Start not mapped to Enter); original Joystick read-only remains optional/strictly gated");
  ps1_probe_native_start_tables(); /* read-only scan; optional checked single-DWORD RAM experiment */
  /* The viewer lock is now armed at bootstrap for a detected DS4,
   * independently of whether the optional gameplay preview loaded. */
  /* Game mouse suppression is configured globally and verified on Flip,
   * independently of whether this optional PS1 preview is active. */
}
/* The original retail PShell_EndTrainingUpdate at 0x47BC40 uses
 * object 0x682968 + 0x14 to dispatch index 0 -> engine request 8,
 * 1 -> request 10, 2 -> request 7. In other 3D stages a different
 * game-owned owner (0x440EF0) can also request 7. These samples
 * deliberately do not assume which owner produced a transition.
 *
 * This diagnostic is PASSIVE: no mouse, menu, joystick, keyboard,
 * object, engine-state or code writes. A per-Flip snapshot may miss
 * transient objects; absence of a sample is not proof of absence.
 */
static int ps1_read_native_selection(DWORD owner,int *selection) {
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t va=(uintptr_t)owner,base;
  DWORD protect;
  if(!owner || owner<0x00010000u || !selection ||
     VirtualQuery((LPCVOID)va,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))
    return 0;
  base=(uintptr_t)mbi.BaseAddress;
  protect=mbi.Protect&0xffu;
  if(va<base || va-base>mbi.RegionSize ||
     mbi.RegionSize-(va-base)<0x15u ||
     !(protect==PAGE_READONLY || protect==PAGE_READWRITE ||
       protect==PAGE_WRITECOPY || protect==PAGE_EXECUTE_READ ||
       protect==PAGE_EXECUTE_READWRITE || protect==PAGE_EXECUTE_WRITECOPY))
    return 0;
  *selection=(int)*(const volatile BYTE *)(va+0x14u);
  return 1;
}
static void ps1_trace_native_quit(int menu2d,int gameplay3d,
                                   int pause_active,int cross_edge,
                                   int circle_edge,int share_edge) {
  static int old_selection=-2,old_restart=-2,old_request=-1,old_pause=-1;
  static int old_context=-1;
  DWORD confirmation_owner,restart_owner,request,mouse_flags;
  int selection=-1,restart_selection=-1;
  int confirmed,restarted,context;
  char line[360];
  if(!ps1_preview_active || !ps1_quit_trace_requested ||
     ps1_quit_trace_count>=130u)return;
  confirmation_owner=*(const volatile DWORD *)(uintptr_t)0x00682968u;
  restart_owner=*(const volatile DWORD *)(uintptr_t)0x00682960u;
  request=*(const volatile DWORD *)(uintptr_t)0x0060CFA4u;
  context=*(const volatile DWORD *)(uintptr_t)0x00682950u ? 1:0;
  mouse_flags=*(const volatile DWORD *)(uintptr_t)0x02E0988Cu;
  confirmed=ps1_read_native_selection(confirmation_owner,&selection);
  restarted=ps1_read_native_selection(restart_owner,&restart_selection);
  if(selection!=old_selection || restart_selection!=old_restart ||
     (int)request!=old_request || pause_active!=old_pause ||
     context!=old_context || cross_edge || circle_edge || share_edge) {
    snprintf(line,sizeof(line),
      "GAMEPAD-PS1: QUIT-TRACE sample=%u 2d=%d 3d=%d pause-visible=%d native-ctx=%d confirmation-owner=%d choice=%d readable=%d restart-owner=%d restart-choice=%d readable=%d request=%lu mouse-off=%d CROSS-edge=%d CIRCLE-edge=%d SHARE-edge=%d [PASSIVE PER-FLIP]",
      ++ps1_quit_trace_count,menu2d,gameplay3d,pause_active,context,
      confirmation_owner!=0u,selection,confirmed,restart_owner!=0u,
      restart_selection,restarted,(unsigned long)request,
      (mouse_flags&0x10u)!=0u,cross_edge,circle_edge,share_edge);
    report(line);
    old_selection=selection;old_restart=restart_selection;
    old_request=(int)request;old_pause=pause_active;old_context=context;
  }
}

/* Buffered opt-in diagnostics: no logging within normal input polls. */
static int ps1_history_enabled(void) {
  return ps1_preview_active && ps1_input_flight_recorder_requested &&
         (ps1_post_quit_guard_requested || ps1_transition_edge_ownership_requested);
}
static void ps1_history_record(unsigned event,unsigned request,int mode2d,
    int mode3d,int movie,int pause,unsigned prev,const DIJOYSTATE2 *state) {
  SpiderPS1HistoryEntry e;
  if(!ps1_history_enabled())return;
  memset(&e,0,sizeof(e));
  e.sequence=ps1_input_history.total+1u;
  e.tick_ms=(uint32_t)GetTickCount();
  e.native_request=request;
  e.previous_confirm=prev;
  e.event=(uint8_t)event;
  e.menu2d=(uint8_t)(mode2d!=0);
  e.gameplay3d=(uint8_t)(mode3d!=0);
  e.movie=(uint8_t)(movie!=0);
  e.native_pause=(uint8_t)(pause!=0);
  if(state && event==SPIDER_PS1_HISTORY_READ_OK) {
    e.cross=(uint8_t)((state->rgbButtons[1]&0x80u)!=0);
    e.options=(uint8_t)((state->rgbButtons[9]&0x80u)!=0);
    e.circle=(uint8_t)((state->rgbButtons[2]&0x80u)!=0);
    e.share=(uint8_t)((state->rgbButtons[8]&0x80u)!=0);
  }
  spider_ps1_history_push(&ps1_input_history,&e);
}
static void ps1_history_dump(void) {
  unsigned i,n=spider_ps1_history_count(&ps1_input_history);
  const SpiderPS1HistoryEntry *e,*prev;
  char line[288];
  if(!ps1_history_enabled())return;
  snprintf(line,sizeof(line),"GAMEPAD-PS1: FLIGHT-RECORDER START buffered=%u events: 0=OK 1=FOCUS-LOSS 2=DI8-FAIL 3=DEVICE-MISSING",n);
  report(line);
  for(i=0;i<n;i++) {
    int changed;
    e=spider_ps1_history_at(&ps1_input_history,i);
    prev=i?spider_ps1_history_at(&ps1_input_history,i-1u):NULL;
    if(!e)continue;
    changed=!prev || e->event!=prev->event ||
      e->cross!=prev->cross || e->options!=prev->options ||
      e->circle!=prev->circle || e->share!=prev->share ||
      e->menu2d!=prev->menu2d || e->gameplay3d!=prev->gameplay3d ||
      e->movie!=prev->movie || e->native_pause!=prev->native_pause ||
      e->native_request!=prev->native_request ||
      e->previous_confirm!=prev->previous_confirm;
    if(!changed && i+8u<n)continue;
    snprintf(line,sizeof(line),
      "GAMEPAD-PS1: FLIGHT-RECORDER seq=%lu ms=%lu event=%u 2d=%u 3d=%u bink=%u pause=%u request=%lu CROSS=%u OPTIONS=%u CIRCLE=%u SHARE=%u previous-mask=%lu [DI8, NOT HUMAN PRESS]",
      (unsigned long)e->sequence,(unsigned long)e->tick_ms,
      (unsigned)e->event,(unsigned)e->menu2d,
      (unsigned)e->gameplay3d,(unsigned)e->movie,
      (unsigned)e->native_pause,(unsigned long)e->native_request,
      (unsigned)e->cross,(unsigned)e->options,(unsigned)e->circle,
      (unsigned)e->share,(unsigned long)e->previous_confirm);
    report(line);
  }
  report("GAMEPAD-PS1: FLIGHT-RECORDER END");
}

/*
 * Poll the already-verified secondary Sony DS4 while the custom focus PAUSED
 * surface blocks the original game's Flip. This routine deliberately never
 * calls begin_scancode, SendInput, or modifies the native joystick mapping.
 * A complete fresh digital press/release is required to wake; sticks/POV
 * cannot wake the game through noise, nor can a held button upon Alt+Tab.
 * The controller cannot take foreground on its own: user must first switch
 * back to the game window normally. No hotplug enumeration in this path.
 */
int SpiderGamepadUI_PauseWake_Begin(void) {
  /* This is a snapshot of the verified device acquired during startup,
   * not a new enumeration or hotplug mechanism. No controller -> keyboard
   * instruction only; mouse and original native controls are unaffected. */
  int available=configured && detected && device!=NULL;
  memset(&pause_wake,0,sizeof(pause_wake));
  pause_wake_active=available;
  pause_wake_retry_logged=0;
  pause_wake_notacquired_attempts=0;
  return available;
}
unsigned SpiderGamepadUI_PauseWake_Poll(HWND hwnd) {
  DIJOYSTATE2 state;
  HRESULT hr;
  unsigned i,buttons=0;
  if(!pause_wake_active)return 0;
  if(!configured || !detected || !device)return 0;
  if(!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) ||
     GetForegroundWindow()!=hwnd) {
    /* A pending partial press must never survive another focus loss. */
    memset(&pause_wake,0,sizeof(pause_wake));
    return 0;
  }
  hr=IDirectInputDevice8_Poll(device);
  /* Many DirectInput devices return DI_NOEFFECT from Poll. The actual
   * GetDeviceState result, not optional Poll, determines readiness. */
  (void)hr;
  memset(&state,0,sizeof(state));
  hr=IDirectInputDevice8_GetDeviceState(device,sizeof(state),&state);
  if(FAILED(hr)) {
    char line[235];
    memset(&pause_wake,0,sizeof(pause_wake));
    /* DIERR_NOTACQUIRED is not a physical disconnect: a foreground-only
     * nonexclusive device loses acquisition on Alt+Tab. We have a verified
     * live secondary handle from before PAUSED; re-Acquire it just once,
     * only while our exact game HWND is again foreground. Do not poll or
     * read this same frame; next loop must first see neutral, then a new
     * complete press/release. INPUTLOST and UNPLUGGED fail closed because
     * the device may have been physically disconnected. */
    if(hr==DIERR_NOTACQUIRED && pause_wake_notacquired_attempts==0u) {
      HRESULT acquire_hr;
      pause_wake_notacquired_attempts=1u;
      acquire_hr=IDirectInputDevice8_Acquire(device);
      snprintf(line,sizeof(line),
        "FOCUS-PAUSE-GAMEPAD: foreground secondary DIERR_NOTACQUIRED=0x%08lX; single guarded Acquire=0x%08lX; fresh neutral+press+release required [NO input injection]",
        (unsigned long)hr,(unsigned long)acquire_hr);
      report(line);
      if(SUCCEEDED(acquire_hr))return 0;
    }
    pause_wake_active=0; /* stop polling potentially stale input until Flip */
    if(!pause_wake_retry_logged) {
      snprintf(line,sizeof(line),
        "FOCUS-PAUSE-GAMEPAD: secondary controller read unavailable hr=0x%08lX; no Acquire for INPUTLOST/UNPLUGGED; keyboard wake remains available",
        (unsigned long)hr);
      report(line);
      pause_wake_retry_logged=1;
    }
    return 0;
  }
  pause_wake_retry_logged=0;
  for(i=0;i<12u;i++)
    if(state.rgbButtons[i]&0x80u)buttons|=(1u<<i);
  return spider_pause_pad_wake_step(&pause_wake,buttons);
}
void SpiderGamepadUI_PauseWake_End(void) {
  pause_wake_active=0;
  memset(&pause_wake,0,sizeof(pause_wake));
  pause_wake_retry_logged=0;
  pause_wake_notacquired_attempts=0;
  /* The normal UI bridge resumes on the next Flip. Our wake used NO
   * synthetic key; reset its edges so the pause input cannot be inherited. */
  previous_buttons=0;
  previous_back=0;
  previous_pause=0;
  previous_share=0;
  previous_native_start=0;
}
/* Strictly passive Xbox trigger probe. Samples raw DirectInput axes from the
 * independently acquired non-exclusive device, only in foreground 3D.
 * Xbox RT/LT may share lZ under XUSB; Wine joystick overrides and Holtek
 * firmware can report separate sliders or alternate axes. Never equate a
 * particular raw axis or sign with RT without observing a real press.
 * A bounded delta-only logger avoids altering the native controller data. */
static void xbox_trace_trigger_axes(const DIJOYSTATE2 *now) {
  int changed=0;
  DWORD tick=GetTickCount();
  const DIJOYSTATE2 *prev=&xbox_trigger_previous;
  char line[285];
  if(!now || xbox_trigger_probe_samples>=48u)return;
  if(xbox_trigger_probe_has_previous) {
    if((DWORD)(tick-xbox_trigger_probe_tick)<100u)return;
#define XBOX_AXIS_CHANGED(field) \
    (((int64_t)now->field-(int64_t)prev->field>4096) || \
     ((int64_t)prev->field-(int64_t)now->field>4096))
    changed=XBOX_AXIS_CHANGED(lZ) || XBOX_AXIS_CHANGED(lRx) ||
      XBOX_AXIS_CHANGED(lRy) || XBOX_AXIS_CHANGED(lRz) ||
      XBOX_AXIS_CHANGED(rglSlider[0]) || XBOX_AXIS_CHANGED(rglSlider[1]);
#undef XBOX_AXIS_CHANGED
    if(!changed)return;
  }
  snprintf(line,sizeof(line),
    "GAMEPAD-XBOX360: TRIGGER-PROBE #%u RAW lZ=%ld lRx=%ld lRy=%ld lRz=%ld slider0=%ld slider1=%ld (passive; press RT and LT separately)",
    xbox_trigger_probe_samples+1u,(long)now->lZ,(long)now->lRx,
    (long)now->lRy,(long)now->lRz,(long)now->rglSlider[0],
    (long)now->rglSlider[1]);
  report(line);
  xbox_trigger_previous=*now;
  xbox_trigger_probe_has_previous=1;
  xbox_trigger_probe_tick=tick;
  xbox_trigger_probe_samples++;
}
void SpiderGamepadUI_OnFlip(HWND hwnd,int menu_2d,int gameplay_3d,
                            int bink_active,int native_pause_active) {
  DIJOYSTATE2 state;
  HRESULT hr;
  unsigned pressed,back_pressed,confirm_edges,effective_confirm;
  int menu_allowed,movie_allowed,confirm_allowed,gameplay_allowed,pause_pressed;
  int share_pressed,share_edge,native_start_pressed;
  int post_quit_block=0;
  unsigned ownership_block=0,native_request=0;
  /* Always apply game-only mouse policy, even with ControllerProfile=Off
   * or no controller; no desktop mouse or keyboard hooks are installed. */
  SpiderPS1Mouse_OnFlip(hwnd);
  if(!configured || !hwnd || !IsWindow(hwnd)){
    xbox_rt_swing_release("game window unavailable");
    return;
  }
  /* Critical: release injected key before focus pause can block this thread. */
  release_active_key();
  if(GetForegroundWindow()!=hwnd || !IsWindowVisible(hwnd)) {
    xbox_rt_swing_release("focus lost");
    /* Native game mouse state was already restored by mouse OnFlip. */
    ps1_history_record(SPIDER_PS1_HISTORY_FOCUS_LOST,0u,menu_2d,
      gameplay_3d,bink_active,native_pause_active,(unsigned)previous_buttons,NULL);
    previous_buttons=0;
    previous_back=0;
    previous_pause=0;
    previous_share=0;
    previous_native_start=0;
    spider_ds4_hotplug_focus_lost(&hotplug);
    return;
  }
  /* Native request 7 is a 3D -> menu return. PS1 and Xbox adapters must
   * not duplicate the last confirmation press into a New Game Enter.
   * This only affects our synthetic UI key; original engine input is
   * observed but left completely untouched. */
  if((ps1_preview_active &&
      (ps1_post_quit_guard_requested || ps1_transition_edge_ownership_requested)) ||
     (xbox360_preview_active && detected_kind==SPIDER_RECONNECT_XBOX360)) {
    native_request=(unsigned)*(const volatile DWORD *)(uintptr_t)0x0060CFA4u;
    if(ps1_post_quit_guard_requested)
      post_quit_block=spider_ps1_post_quit_guard_flip(&ps1_post_quit_guard,
        1,menu_2d,gameplay_3d,bink_active,native_request);
  }
  /* Discovery is foreground-only, bounded and non-invasive. A DS4 may be
   * connected after startup, or the previously attached DS4 reconnected.
   * Native PS1 gameplay mapping is only activated during the verified
   * startup bootstrap; a late-first device does NOT silently rewrite the
   * original game's controller tables or force a runtime config reload. */
  if(!probing || spider_ds4_hotplug_scan_due(&hotplug,
                                             (uint32_t)GetTickCount(),detected))
    detect_device(hwnd);
  /* If this Xbox was attached only AFTER the initial game bootstrap,
   * still lock its native Joystick Configuration before any UI action.
   * Never run on an unknown pad, Sony profile or in the background. */
  xbox_try_readonly_menu();
  /* Keep the native Joystick viewer locked for the CURRENT physically
   * detected supported pad, including late-first attachment and swaps.
   * Reusing an existing eight-site lock changes only the label formatter,
   * not native action bindings (runtime profile switching is separate). */
  if(detected && device && detected_kind==SPIDER_RECONNECT_DS4)
    SpiderGamepadUI_ArmPS1ReadOnly();
  else if(detected && device &&
          detected_kind==SPIDER_RECONNECT_XBOX360 && xbox_readonly_active)
    SpiderXbox360_InstallReadOnlyJoystick(log_message);
  if(!detected||!device) {
    ps1_history_record(SPIDER_PS1_HISTORY_DEVICE_MISSING,native_request,
      menu_2d,gameplay_3d,bink_active,native_pause_active,
      (unsigned)previous_buttons,NULL);
    return;
  }
  hr=IDirectInputDevice8_Poll(device);
  if(FAILED(hr) && hr!=DI_NOEFFECT) {
    /* Some DirectInput joysticks use implicit polling; read even when the
     * optional Poll returns a harmless non-success status. */
  }
  memset(&state,0,sizeof(state));
  hr=IDirectInputDevice8_GetDeviceState(device,sizeof(state),&state);
  if(FAILED(hr)) {
    ps1_history_record(SPIDER_PS1_HISTORY_READ_FAILED,native_request,
      menu_2d,gameplay_3d,bink_active,native_pause_active,
      (unsigned)previous_buttons,NULL);
    xbox_rt_swing_release("secondary DirectInput read failure");
    if(bad_reads++<4u) {
      char line[155];
      snprintf(line,sizeof(line),
        "GAMEPAD-UI: independent DirectInput read hr=0x%08lX; no Return emitted",
        (unsigned long)hr);
      report(line);
    }
    /* A failed GetDeviceState was the final trace before the user's
     * unplug crash. Never call Acquire on that possibly disconnected COM
     * instance in the same frame. DIERR_INPUTLOST is strong enough to
     * drop it immediately; other failures must repeat three times. */
    previous_buttons=0;
    previous_back=0;
    previous_pause=0;
    previous_share=0;
    previous_native_start=0;
    {
      int repeatedly_failed=spider_ds4_hotplug_read_failed(&hotplug);
      if(hr==DIERR_INPUTLOST || repeatedly_failed)
        spider_hotplug_drop_device(hr,
          hr==DIERR_INPUTLOST?"INPUTLOST (unplug/focus device loss)":
          "three consecutive read errors");
      else
        report("GAMEPAD-HOTPLUG: transient secondary read failure; no stale Acquire; awaiting next foreground Flip");
    }
    return;
  }
  bad_reads=0;
  if(xbox_trigger_probe_requested && detected_kind==SPIDER_RECONNECT_XBOX360 &&
     gameplay_3d && !bink_active && !native_pause_active)
    xbox_trace_trigger_axes(&state);
  /* The PS1 layout requires a validated pre-game native config load.
   * If DS4 first appears after that startup moment, refuse a misleading
   * legacy Cross/Options synthetic mapping while PS1 was requested:
   * the user must relaunch with the DS4 already connected to activate
   * verified seven-button gameplay mapping. Never reload game config
   * mid-level or rewrite unknown native controller objects. */
  if(detected_kind==SPIDER_RECONNECT_DS4 && ps1_preview_requested && !ps1_preview_active) {
    if(!hotplug_late_preview_reported) {
      report("GAMEPAD-HOTPLUG: DS4 discovered but PS1 gameplay preview was not loaded at startup; synthetic actions disabled until restart with controller present");
      hotplug_late_preview_reported=1;
    }
    return;
  }
  {
    unsigned i,held=0;
    for(i=0;i<12u;i++)
      if(state.rgbButtons[i]&0x80u)held|=(1u<<i);
    if(!spider_ds4_hotplug_accept_buttons(&hotplug,held)) {
      xbox_rt_swing_release("controller reconnect/edge quarantine");
      /* Never leak a held reconnect/focus-return button as a synthetic
       * Return/Escape. The original native controller reads are untouched. */
      previous_buttons=0;previous_back=0;previous_pause=0;
      previous_share=0;previous_native_start=0;
      return;
    }
  }
  /* Xbox 360-compatible DirectInput: no DS4 PS1 mapping, no guess about
   * native 3D controls. The Holtek VID=045E PID=028E joystick is already
   * read by the original game for analog/POV navigation. Only bridge
   * fresh A/B press edges to Enter/Escape in known menu/movie contexts.
   * Log the first 32 changed physical masks to verify driver button order. */
  if(detected_kind==SPIDER_RECONNECT_XBOX360) {
    uint32_t held=spider_xbox360_held_buttons(state.rgbButtons);
    uint32_t fresh=held & ~xbox_previous_mask;
    unsigned action=spider_xbox360_menu_edges(held,xbox_previous_mask);
    /* Start is deliberately ignored by all Xbox UI synthesis, including
     * menus and 3D; Back is the equivalent of PS1 Share. */
    unsigned pause_button_index=SPIDER_XBOX360_BACK_DI_BUTTON;
    int pause_requested=(fresh & (1u<<SPIDER_XBOX360_BACK_DI_BUTTON))!=0;
    int ui_allowed=(menu_2d && !gameplay_3d) || bink_active ||
                   native_pause_active;
    unsigned inherited_a;
    int post_quit_block;
    int return_boundary=(xbox360_preview_active && menu_2d && !gameplay_3d &&
                         !bink_active && xbox_transition_ownership.seen_3d &&
                         native_request==7u);
    if(held!=xbox_previous_mask && xbox_button_trace_count<32u) {
      char xbox_line[192];
      snprintf(xbox_line,sizeof(xbox_line),
        "GAMEPAD-XBOX360: BUTTON-EDGE #%u held=0x%03lX changed=0x%03lX (physical DInput indexes 0..9; candidate A=0 B=1)",
        ++xbox_button_trace_count,(unsigned long)held,
        (unsigned long)(held^xbox_previous_mask));
      report(xbox_line);
    }
    xbox_previous_mask=held;
    inherited_a=spider_native_menu_return_gate_flip(
      &xbox_transition_ownership,xbox360_preview_active,
      menu_2d,gameplay_3d,bink_active,native_request,
      held & (1u<<SPIDER_XBOX360_A_DI_BUTTON));
    post_quit_block=spider_ps1_post_quit_guard_flip(
      &xbox_post_quit_guard,xbox360_preview_active,
      menu_2d,gameplay_3d,bink_active,native_request);
    if(return_boundary && xbox_return_to_menu_reports++<12u) {
      char line[200];
      snprintf(line,sizeof(line),
        "GAMEPAD-XBOX360: POST-QUIT RETURN native_request=7 A-held=%d inherited-A=%d settle=%d; Start disabled; native controller unmodified",
        !!(held&1u),!!inherited_a,post_quit_block);
      report(line);
    }
    /* RT is a physical analog axis; the controller native button array
     * never sees it as BUTTON9. Sustain keyboard Swing only in unpaused
     * live 3D and release before Back pause. */
    xbox_rt_swing_update((long)state.lZ,
      gameplay_3d && !menu_2d && !bink_active && !native_pause_active &&
      !pause_requested);
    /* Gameplay: Back synthesizes Escape only when UNBOUND from all
     * original native actions. Start does not synthesize any key.
     * The game will still see the controller,
     * so never fire both Swing/Aim and Pause on one physical press.
     * No synthetic key is injected for the other 3D buttons. */
    if((gameplay_3d || native_pause_active) && !bink_active &&
       pause_requested && xbox_pause_button_free_of_native_actions(pause_button_index)) {
      begin_scancode(0x01u,"Escape");
      report("GAMEPAD-XBOX360: 3D MENU Back -> Escape (no saved native action conflict); Start inert");
    } else if(ui_allowed && action==SPIDER_XBOX360_BACK)
      begin_scancode(0x01u,"Escape");
    else if(ui_allowed && action==SPIDER_XBOX360_ENTER &&
            (inherited_a || post_quit_block)) {
      report("GAMEPAD-XBOX360: POST-QUIT-GUARD suppressed inherited/too-early synthetic A->Return; original controller untouched");
    } else if(ui_allowed && action==SPIDER_XBOX360_ENTER)
      begin_scancode(0x1Cu,"Return");
    return;
  }
  xbox_rt_swing_release("active device is not Xbox 360");
  pressed=(state.rgbButtons[1]&0x80u?SPIDER_CONFIRM_CROSS:0) |
          (state.rgbButtons[9]&0x80u?SPIDER_CONFIRM_OPTIONS:0);
  /* PS1 preview: Cross is the ONLY synthetic menu confirmation. The
   * legacy ControllerEnterButtons choice is honored outside PS1 preview.
   * OPTIONS/Start remains free of synthetic Enter even if the old INI
   * still says Both; native Start/BUTTON11 is handled separately. */
  effective_confirm=spider_ps1_menu_confirm_mask(ps1_preview_active,(unsigned)confirm);
  /* Per-button edges are still tracked independently for legacy mode. */
  confirm_edges=spider_ps1_menu_confirm_edges(pressed,(unsigned)previous_buttons,effective_confirm);
  ps1_history_record(SPIDER_PS1_HISTORY_READ_OK,native_request,menu_2d,
    gameplay_3d,bink_active,native_pause_active,(unsigned)previous_buttons,&state);
  if(ps1_preview_active && ps1_transition_edge_ownership_requested) {
    ownership_block=spider_ps1_transition_edge_ownership_flip(
      &ps1_transition_ownership,1,menu_2d,gameplay_3d,bink_active,
      native_request,pressed & effective_confirm);
  }
  back_pressed=(state.rgbButtons[2]&0x80u?SPIDER_BACK_CIRCLE:0) |
               (state.rgbButtons[8]&0x80u?SPIDER_BACK_SHARE:0);
  /* Original GameFMV_Active remains authoritative even outside the
   * widescreen menu presentation. 3D triangle/script input is untouched. */
  movie_allowed=bink_active!=0;
  menu_allowed=(menu_2d && !gameplay_3d) ||
    (ps1_preview_active && native_pause_active && !movie_allowed);
  confirm_allowed=menu_allowed || movie_allowed;
  gameplay_allowed=gameplay_3d && !menu_2d && !movie_allowed &&
                   !(ps1_preview_active && native_pause_active);
  pause_pressed=state.rgbButtons[9]&0x80u;
  share_pressed=state.rgbButtons[8]&0x80u;
  share_edge=share_pressed && !previous_share;
  if(ps1_quit_trace_requested)
    ps1_trace_native_quit(menu_2d,gameplay_3d,native_pause_active,
      (pressed&SPIDER_CONFIRM_CROSS) && !(previous_buttons&SPIDER_CONFIRM_CROSS),
      (back_pressed&SPIDER_BACK_CIRCLE) && !(previous_back&SPIDER_BACK_CIRCLE),
      share_edge);
  native_start_pressed=(state.rgbButtons[11]&0x80u)!=0;
  /* Passive test of the original BUTTON11/camera press. The native
   * table modification being accepted does not by itself prove which
   * controller table the runtime actually reads. Do NOT synthesize
   * or suppress an input here; record only the physical press edge. */
  if(ps1_preview_active && native_start_pressed && !previous_native_start &&
     ps1_native_start_reports++<12u) {
    char line[210];
    snprintf(line,sizeof(line),
      "GAMEPAD-PS1: ORIGINAL-BUTTON11 PHYSICAL PRESS; Start-RAM-unbound=%d native-pause-current=%d [READ-ONLY; test old button separately from SHARE]",
      ps1_native_start_unbound,native_pause_active!=0);
    report(line);
  }
  if(ps1_preview_active && ps1_options_skip_requested &&
     gameplay_3d && !menu_2d && !movie_allowed && !native_pause_active &&
     pause_pressed && !previous_pause) {
    if(!ps1_request_native_options_cutscene_skip() &&
       ps1_cutscene_reports++<8u)
      report("GAMEPAD-UI: PS1-3D-OPTIONS refused: original CutSceneSkipAllow absent, inactive, unreadable or already requested");
  }
  if(ps1_preview_active && (gameplay_allowed || native_pause_active) &&
     !movie_allowed && share_edge) {
    begin_scancode(0x01u,"Escape");
    if(ps1_native_start_unbound)
      report("GAMEPAD-UI: PS1-PREVIEW SHARE -> Escape during 3D/pause; original Start RAM unbound (runtime behavior still to verify)");
    else
      report("GAMEPAD-UI: PS1-PREVIEW SHARE -> Escape during 3D/pause; original Start mapping retained");
  } else if(menu_allowed && (back_pressed & (unsigned)back) &&
     !(previous_back & (unsigned)back)) {
    begin_scancode(0x01u,"Escape");
  } else if(confirm_allowed && confirm_edges &&
            !spider_ps1_menu_confirm_unowned(confirm_edges,ownership_block) &&
            (confirm_edges & ownership_block)) {
    /* Only old-session confirmation bits are consumed. A fresh OTHER
     * confirm button may still send Return even while one bit is held. */
    report("GAMEPAD-PS1: TRANSITION-OWNERSHIP consumed inherited menu confirmation (PS1: Cross only; legacy: Cross/Options) on first 2D frame after native request 7; rearm on physical release");
    ps1_history_dump();
  } else if(confirm_allowed && confirm_edges && post_quit_block) {
    /* Legacy 8-Flip guard stays optional and suppresses any fresh Enter.
     * Per-button edges are still consumed by previous_buttons below. */
    report("GAMEPAD-PS1: POST-QUIT-GUARD suppressed synthetic Return on first 2D menu frames after native request 7");
    ps1_history_dump();
  } else if(confirm_allowed &&
            spider_ps1_menu_confirm_unowned(confirm_edges,ownership_block)) {
    begin_scancode(0x1Cu,"Return");
    if(movie_allowed && !menu_allowed && postmenu_bink_reports++<8u)
      report("GAMEPAD-UI: POST-MENU BINK Return attempted with original GameFMV_Active=1; native video skip gate remains authoritative");
  } else if(!ps1_preview_active && gameplay_allowed && pause_button==SPIDER_PAUSE_OPTIONS &&
            pause_pressed && !previous_pause &&
            options_free_of_native_actions()) {
    begin_scancode(0x01u,"Escape");
    report("GAMEPAD-UI: 3D PAUSE REQUEST OPTIONS -> Escape; no saved native BUTTON9 gameplay assignment");
  } else if(!menu_allowed && !movie_allowed && !gameplay_allowed &&
            confirm_edges && outside_mode_reports++<4u) {
    report("GAMEPAD-UI: unclassified engine stage; no keyboard injection");
  }
  previous_buttons=(int)pressed;
  previous_back=(int)back_pressed;
  previous_pause=pause_pressed;
  previous_share=share_pressed;
  previous_native_start=native_start_pressed;
}
