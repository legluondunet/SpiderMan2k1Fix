#ifndef SPIDERMAN2K1FIX_SDL3_NATIVE_LATCH_TRACE_H
#define SPIDERMAN2K1FIX_SDL3_NATIVE_LATCH_TRACE_H
/*
 * Optional passive research telemetry. Not a controller input bridge.
 * Called only from the already-executing original game Flip thread.
 * Does not install hooks, call input readers, modify COM objects or
 * write native addresses. It reads ONLY the first 3 bytes of each
 * authenticated 16-byte original action state record.
 *
 * An exact-hash main executable gate is already enforced by the fix.
 * Require source-known instruction signatures here as an extra
 * fail-closed preflight before reading game state.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "sdl3_native_latch_observer.h"

static SpiderNativeLatchObserver spider_latch_trace_observer;
static int spider_latch_trace_enabled=0;
static int spider_latch_trace_preflight_ok=0;
static void (*spider_latch_trace_log)(const char *)=NULL;

static int spider_latch_trace_binary_ok(void) {
  static const unsigned char aggregate[]={0xc7,0x03,0,0,0,0};
  static const unsigned char native_stage[]={0x68,0x94,0x83,0x56,0};
  static const unsigned char state_step[]={0x8b,0x44,0x24,0x04};
  return memcmp((const void *)(uintptr_t)0x0050A19Fu,
                aggregate,sizeof(aggregate))==0 &&
         memcmp((const void *)(uintptr_t)0x0050A29Eu,
                native_stage,sizeof(native_stage))==0 &&
         memcmp((const void *)(uintptr_t)0x00479540u,
                state_step,sizeof(state_step))==0;
}
static void spider_latch_trace_init(const char *ini,
                                   void (*logger)(const char *)) {
  spider_native_latch_observer_reset(&spider_latch_trace_observer);
  spider_latch_trace_log=logger;
  spider_latch_trace_enabled=ini && logger &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1 &&
    GetPrivateProfileIntA("Debug","SDL3NativeStateTrace",0,ini)==1;
  spider_latch_trace_preflight_ok=0;
  if(spider_latch_trace_enabled) {
    spider_latch_trace_preflight_ok=spider_latch_trace_binary_ok();
    if(!spider_latch_trace_preflight_ok) {
      logger("SDL3-NATIVE-STATE: REFUSED original instruction signatures mismatch");
      spider_latch_trace_enabled=0;
    } else {
      logger("SDL3-NATIVE-STATE: passive native action state monitoring enabled; 64 changes maximum");
    }
  }
}
static void spider_latch_trace_flip(HWND hwnd,int menu2d,int gameplay3d,
                                    int bink,int paused) {
  uint8_t states[SPIDER_ORIGINAL_INPUT_STATE_COUNT][16]={{0}};
  SpiderNativeLatchSnapshot state;
  unsigned i,ctx;
  char message[256];
  if(!spider_latch_trace_enabled || !spider_latch_trace_preflight_ok ||
     !spider_latch_trace_log ||
     spider_latch_trace_observer.emitted>=64u)return;
  if(!hwnd || GetForegroundWindow()!=hwnd || bink || paused) {
    /* Discard the comparison baseline, but never reset the session cap.
     * A held native state after Alt+Tab should be logged once on return. */
    spider_latch_trace_observer.have_previous=0u;
    return;
  }
  ctx=(menu2d?1u:0u)|(gameplay3d?2u:0u);
  for(i=0;i<SPIDER_ORIGINAL_INPUT_STATE_COUNT;i++) {
    const volatile uint8_t *record=
      (const volatile uint8_t *)(uintptr_t)
      spider_original_action_latch_address(i);
    states[i][0]=record[0];
    states[i][1]=record[1];
    states[i][2]=record[2];
  }
  spider_native_latch_decode(states,&state);
  if(!spider_native_latch_observer_changed(
        &spider_latch_trace_observer,&state,ctx))return;
  snprintf(message,sizeof(message),
    "SDL3-NATIVE-STATE: ctx=%s held-slots=%05lX actions=%04lX "
    "event-slots=%05lX early-slots=%05lX [READ ONLY, SOURCE UNKNOWN]",
    gameplay3d?"3D":(menu2d?"MENU":"OTHER"),
    (unsigned long)state.held_slots,(unsigned long)state.held_actions,
    (unsigned long)state.event_slots,(unsigned long)state.early_slots);
  spider_latch_trace_log(message);
}
#endif
