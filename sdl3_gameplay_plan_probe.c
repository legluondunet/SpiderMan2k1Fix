/* Strict opt-in SDL3 3D semantic bridge PRE-FLIGHT. The semantic actions
 * come from the exact loaded per-model profile and the selected SDL3 pad.
 * The native engine's DirectInput device is verified to be the same
 * selected VID/PID. This ONLY observes and logs presses/releases.
 *
 * Why NO 3D injection yet? 0x568408 maps engine action masks to physical
 * DirectInput button indices; SDL3 LT/RT are separate analog axes and
 * cannot be written there as a "button index". In particular, blindly
 * copying Swing=trigger:5 would leave an invalid native button and could
 * duplicate the legacy RT keyboard bridge. The next stage must intercept
 * the original buffered joystick reader with exact SHA/pattern validation
 * to replace native button events while retaining its original axes.
 * Never synthesize keys alongside the still-active native button reader.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_gameplay_plan_probe.h"
#include "sdl3_gameplay_plan.h"
#include "sdl3_gamepad_probe.h"
#include "native_joystick_reconnect.h"

static int requested=0;
static unsigned reports=0;
static int reported_unready=0;
static SpiderSDL3GameplayPlan state;
static void (*report)(const char *)=NULL;
void SpiderSDL3GameplayPlan_Init(const char *ini,void (*logger)(const char *)) {
  report=logger;
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
  if(!requested)return;
  if(!hwnd || GetForegroundWindow()!=hwnd || menu2d ||
     !gameplay3d || bink || pause) {
    spider_sdl3_gameplay_plan_reset(&state);
    return;
  }
  if(!SpiderSDL3Probe_SelectedSample(&id,&vid,&pid,&sample)) {
    spider_sdl3_gameplay_plan_reset(&state);
    return;
  }
  ready=SpiderNativeReconnect_SDL3SelectionReady(vid,pid);
  if(!ready) {
    spider_sdl3_gameplay_plan_reset(&state);
    if(!reported_unready && reports<64u) {
      reported_unready=1;reports++;
      report("SDL3-3D-PLAN: REFUSED, SDL3 selection not verified as original game-owned DirectInput controller");
    }
    return;
  }
  reported_unready=0;
  profile=SpiderSDL3Probe_SetupProfile(vid,pid);
  if(!profile)return;
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
