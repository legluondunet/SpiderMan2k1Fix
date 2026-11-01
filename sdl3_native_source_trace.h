#ifndef SPIDERMAN2K1FIX_SDL3_NATIVE_SOURCE_TRACE_H
#define SPIDERMAN2K1FIX_SDL3_NATIVE_SOURCE_TRACE_H
/* Additional opt-in, passive, game Flip-thread-only source correlation.
 * Reads separately authenticated original keyboard and joystick latches.
 * No DI polling, no COM intercept, no native writes or added hooks.
 * Original game action mask is ALREADY merged; correlation is not proof of
 * which source caused a particular action or which SDL3 instance owns it.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "sdl3_native_source_observer.h"
#include "sdl3_original_action_layout.h"
static SpiderNativeSourceObserver spider_source_trace_observer;
static int spider_source_trace_enabled=0;
static void (*spider_source_trace_log)(const char *)=NULL;
static int spider_source_trace_binary_ok(void) {
  static const unsigned char keyboard[]={0x8a,0x98,0x2c,0x79,0x6b,0x00};
  static const unsigned char joystick[]={0xb1,0x7f};
  static const unsigned char action[]={0x8b,0x44,0x24,0x04};
  return memcmp((const void *)(uintptr_t)0x00501C56u,
                keyboard,sizeof(keyboard))==0 &&
         memcmp((const void *)(uintptr_t)0x00501F13u,
                joystick,sizeof(joystick))==0 &&
         memcmp((const void *)(uintptr_t)0x00479540u,
                action,sizeof(action))==0;
}
static void spider_source_trace_init(const char *ini,
                                     void (*logger)(const char *)) {
  spider_native_source_reset(&spider_source_trace_observer);
  spider_source_trace_log=logger;
  spider_source_trace_enabled=ini&&logger&&
    GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1&&
    GetPrivateProfileIntA("Debug","SDL3NativeSourceTrace",0,ini)==1;
  if(spider_source_trace_enabled) {
    if(!spider_source_trace_binary_ok()) {
      logger("SDL3-NATIVE-SOURCE: REFUSED native keyboard/joystick/latch signatures mismatch");
      spider_source_trace_enabled=0;
    } else {
      logger("SDL3-NATIVE-SOURCE: ARMED read-only keyboard/joystick latches; 96 changes + 8 reserved postfocus frame samples + 12 focus-status events maximum");
    }
  }
}
static void spider_source_trace_flip(HWND hwnd,int menu2d,int gameplay3d,
                                     int bink,int paused) {
  uint8_t keyboard[SPIDER_SOURCE_KEY_BYTES],joystick[SPIDER_SOURCE_JOY_BYTES];
  SpiderNativeSourceSnapshot sample;
  unsigned i,ctx;
  uint32_t game_held=0u;
  int active;
  char line[310];
  if(!spider_source_trace_enabled||!spider_source_trace_log)return;
  active=hwnd&&GetForegroundWindow()==hwnd&&!bink&&!paused&&
         (menu2d||gameplay3d);
  ctx=(menu2d?1u:0u)|(gameplay3d?2u:0u);
  if(spider_native_source_focus_step(&spider_source_trace_observer,active)) {
    snprintf(line,sizeof(line),
      "SDL3-NATIVE-SOURCE: sampling=%s ctx=%s [NO INPUT READ WHILE INACTIVE]",
      active?"ACTIVE":"SUSPENDED",
      gameplay3d?"3D":(menu2d?"MENU":"OTHER"));
    spider_source_trace_log(line);
  }
  if(!active||(spider_source_trace_observer.emitted>=SPIDER_SOURCE_MAX_CHANGES&&
       (!spider_source_trace_observer.pending_resume||
        spider_source_trace_observer.resume_samples>=8u)))return;
  /* These original PE32 addresses are verified by the supported EXE SHA and
   * the x86 preflight above. Never read on background, movie or pause. */
  for(i=0;i<SPIDER_SOURCE_KEY_BYTES;i++)
    keyboard[i]=((const volatile uint8_t *)(uintptr_t)0x006B792Cu)[i];
  for(i=0;i<SPIDER_SOURCE_JOY_BYTES;i++)
    joystick[i]=((const volatile uint8_t *)(uintptr_t)0x006B7A34u)[i];
  spider_native_source_decode(keyboard,joystick,&sample);
  for(i=0;i<SPIDER_ORIGINAL_INPUT_STATE_COUNT;i++) {
    const volatile uint8_t *slot=(const volatile uint8_t *)(uintptr_t)
      spider_original_action_latch_address(i);
    if(slot[0])game_held|=spider_original_action_latch_mask(i);
  }
  /* Include the merged game mask in change detection. Joystick axes/POV
   * can move while the 32 native BUTTON states remain unchanged. */
  sample.merged_actions=game_held;
  /* Compare native DI latches to an independent Win32 key-down sample.
   * These three non-typing keys are sampled ONLY while foreground and
   * only when the passive trace is explicitly enabled. No key injection. */
  if(GetAsyncKeyState(VK_LMENU)&0x8000)sample.os_alt_tab|=1u;
  if(GetAsyncKeyState(VK_RMENU)&0x8000)sample.os_alt_tab|=2u;
  if(GetAsyncKeyState(VK_TAB)&0x8000)sample.os_alt_tab|=4u;
  if(!spider_native_source_changed(&spider_source_trace_observer,
                                    &sample,ctx))return;
  snprintf(line,sizeof(line),
    "SDL3-NATIVE-SOURCE: ctx=%s kb-held=%u kb-fresh=%u kb-released=%u "
    "joy-held=%08lX joy-fresh=%08lX joy-released=%08lX "
    "merged-actions=%04lX alt-tab=%u os-alt-tab=%u postfocus=%u [READ ONLY, CORRELATION NOT PROVENANCE]",
    gameplay3d?"3D":"MENU",(unsigned)sample.keyboard_held,
    (unsigned)sample.keyboard_fresh,(unsigned)sample.keyboard_released,
    (unsigned long)sample.joystick_held,
    (unsigned long)sample.joystick_fresh,
    (unsigned long)sample.joystick_released,
    (unsigned long)game_held,(unsigned)sample.keyboard_alt_tab,
    (unsigned)sample.os_alt_tab,
    spider_source_trace_observer.last_resume);
  spider_source_trace_log(line);
}
#endif
