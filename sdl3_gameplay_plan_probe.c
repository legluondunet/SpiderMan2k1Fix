/* Strict SDL3 semantic 3D telemetry wrapper (Debug SDL3GameplayPlan).
 * The existing profile edge trace remains observational and never writes.
 * An independent, additionally opt-in DS4-only runtime module may apply
 * seven verified SDL3 physical bindings via the game's existing canonical
 * 14-slot native DirectInput map (not the SDL3 axis index). Known DS4
 * LT/RT translate to native physical buttons 6/7; unknown SDL buttons
 * or Xbox triggers cause an atomic refusal, not a guessed injection.
 * The eighth action Pause uses the existing one-frame Escape key owner.
 * This module preserves the old read-only trace regardless of live mode.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_gameplay_plan_probe.h"
#include "sdl3_gameplay_plan.h"
#include "sdl3_gameplay_live.h"
#include "sdl3_gamepad_probe.h"
#include "native_joystick_reconnect.h"
#include "quick_exit_f12.h"

static int requested=0;
static unsigned reports=0;
static int reported_unready=0;
static SpiderSDL3GameplayPlan state;
static void (*report)(const char *)=NULL;
void SpiderSDL3GameplayPlan_Init(const char *ini,void (*logger)(const char *)) {
  report=logger;
  SpiderSDL3GameplayLive_Init(ini,logger); /* separate opt-in runtime */
  reports=0;reported_unready=0;
  spider_sdl3_gameplay_plan_reset(&state);
  requested=ini && GetPrivateProfileIntA("Debug","SDL3GameplayPlan",0,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3NativeOwner",0,ini)==1;
  if(requested && report)
    report("SDL3-3D-PLAN: read-only per-model SDL3 action translator enabled; native 3D gameplay NOT modified; max 64 edge logs");
}
void SpiderSDL3GameplayPlan_OnFlip(HWND hwnd,int menu2d,int gameplay3d,
                                  int bink,int pause) {
  uint32_t id=0,pressed=0,released=0;
  uint16_t vid=0,pid=0;
  SpiderSDL3ControllerSample sample;
  SpiderSDL3ControllerProfile *profile;
  int rc,ready;
  char line[245];
  /* Live bridge is a SEPARATE opt-in and must run even if the
   * passive [Debug] SDL3GameplayPlan trace is disabled. */
  SpiderSDL3GameplayLive_OnFlip(hwnd,menu2d,gameplay3d,bink,pause);
  if(!requested)return;
  if(!hwnd || GetForegroundWindow()!=hwnd || menu2d ||
     !gameplay3d || bink || pause) {
    SpiderQuickExitF12_CutscenePhysicalSample(0,0,0,0,0);
    spider_sdl3_gameplay_plan_reset(&state);
    return;
  }
  if(!SpiderSDL3Probe_SelectedSample(&id,&vid,&pid,&sample)) {
    SpiderQuickExitF12_CutscenePhysicalSample(0,0,0,0,0);
    spider_sdl3_gameplay_plan_reset(&state);
    return;
  }
  ready=SpiderNativeReconnect_SDL3SelectionReady(vid,pid);
  if(!ready) {
    SpiderQuickExitF12_CutscenePhysicalSample(0,0,0,0,0);
    spider_sdl3_gameplay_plan_reset(&state);
    if(!reported_unready && reports<64u) {
      reported_unready=1;reports++;
      report("SDL3-3D-PLAN: REFUSED, SDL3 selection not verified as original game-owned DirectInput controller");
    }
    return;
  }
  reported_unready=0;
  profile=SpiderSDL3Probe_SetupProfile(vid,pid);
  if(!profile) {
    SpiderQuickExitF12_CutscenePhysicalSample(0,0,0,0,0);
    return;
  }
  /* Read-only physical South/West correlation, independently of the
   * saved semantic action map. Same selected, verified original owner. */
  SpiderQuickExitF12_CutscenePhysicalSample(id,vid,pid,sample.buttons,ready);
  /* Both SDL3 and the original game must be ready. Physical input is
   * consumed ONCE from the selected SDL3 handle after normal Flip pump. */
  rc=spider_sdl3_gameplay_plan_step(&state,profile,&sample,id,vid,pid,
                                     1,ready,&pressed,&released);
  if(rc==SPIDER_PLAN_WAIT_NEUTRAL && reports<64u &&
     state.armed && state.held==0u) {
    snprintf(line,sizeof(line),
      "SDL3-3D-PLAN: ARMED selected=%lu VID:PID=%04X:%04X (neutral first; saved semantic mappings; READ-ONLY, no game input changes)",
      (unsigned long)id,(unsigned)vid,(unsigned)pid);
    report(line);reports++;
  }
  if(rc!=SPIDER_PLAN_READY || (!pressed && !released) || reports>=64u)
    return;
  snprintf(line,sizeof(line),
    "SDL3-3D-PLAN: selected=%lu VID:PID=%04X:%04X pressed=0x%02lX released=0x%02lX held=0x%02lX LT=%d RT=%d ms=%lu [NO NATIVE INJECTION]",
    (unsigned long)id,(unsigned)vid,(unsigned)pid,
    (unsigned long)pressed,(unsigned long)released,
    (unsigned long)state.held,
    (int)sample.axis[SPIDER_SDL3_AXIS_LT],
    (int)sample.axis[SPIDER_SDL3_AXIS_RT],
    (unsigned long)GetTickCount());
  report(line);reports++;
}
