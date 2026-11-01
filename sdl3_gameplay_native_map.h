/* Translate the seven 3D SDL3 binding choices to known Sony DS4
 * DirectInput rgbButtons indices. This header NEVER changes game memory.
 * Only the exact physical mapping observed in the supported Sony DS4
 * profile is allowed; D-pad is POV, not native rgbButtons, and Xbox
 * triggers may be merged axes. L2 is not verified and is unsupported. An unsupported input rejects the WHOLE
 * seven-action map, not an individual action or an invented button. */
#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPLAY_NATIVE_MAP_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPLAY_NATIVE_MAP_H
#include "sdl3_controller_profile.h"
#include "ps1_gamepad_mapping.h"
static inline int spider_sdl3_ds4_native_button(
    SpiderSDL3Binding b,uint32_t *out) {
  static const int map[SPIDER_SDL3_BUTTON_COUNT]={
    1,2,0,3, 8,-1,-1,-1,-1, 4,5,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };
  if(!out)return 0;
  if(b.kind==SPIDER_SDL3_BIND_BUTTON) {
    if(b.index>=SPIDER_SDL3_BUTTON_COUNT || map[b.index]<0)return 0;
    *out=(uint32_t)map[b.index];return 1;
  }
  if(b.kind==SPIDER_SDL3_BIND_TRIGGER) {
    /* Sony R2=BUTTON7 is backed by the verified canonical native
     * Swing binding. Sony L2's 3D native rgbButton index has NOT been
     * independently observed: refuse it rather than assuming BUTTON6. */
    if(b.index==SPIDER_SDL3_AXIS_RT){*out=7u;return 1;}
  }
  return 0;
}
static inline int spider_sdl3_native_ds4_seven(
    const SpiderSDL3ControllerProfile *p,
    uint32_t out[SPIDER_PS1_BUTTON_COUNT]) {
  uint32_t candidate[SPIDER_PS1_BUTTON_COUNT];
  unsigned i,j;
  if(!p || !out || p->vendor!=0x054cu ||
     !(p->product==0x05c4u || p->product==0x09ccu ||
       p->product==0x0ba0u) || !spider_sdl3_profile_valid(p))
    return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++) {
    if(!spider_sdl3_ds4_native_button(p->actions[i],&candidate[i]))
      return 0;
    for(j=0;j<i;j++)
      if(candidate[i]==candidate[j])return 0;
  }
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)out[i]=candidate[i];
  return 1;
}
/* Exact wired Xbox 360 045E:028E verified SDL3/DirectInput mapping:
 * SDL South/East/West/North=DI BUTTON0/1/2/3; LB/RB=BUTTON4/5.
 * The native Swing slot MUST stay 0x4000 (UNBOUND). Its SDL RT axis is
 * handled separately by the existing guarded keyboard RT-Swing bridge.
 * Never map RT to BUTTON9 (stick click), and never guess LT or D-pad/POV.
 * Refuse the entire profile on an unsupported button/trigger. */
static inline int spider_sdl3_xbox360_native_button(
    SpiderSDL3Binding b,uint32_t *out) {
  static const int map[SPIDER_SDL3_BUTTON_COUNT]={
    0,1,2,3, -1,-1,-1,-1,-1, 4,5,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };
  if(!out || b.kind!=SPIDER_SDL3_BIND_BUTTON ||
     b.index>=SPIDER_SDL3_BUTTON_COUNT || map[b.index]<0)return 0;
  *out=(uint32_t)map[b.index];
  return 1;
}
static inline int spider_sdl3_native_xbox360_seven(
    const SpiderSDL3ControllerProfile *p,
    uint32_t out[SPIDER_PS1_BUTTON_COUNT]) {
  uint32_t candidate[SPIDER_PS1_BUTTON_COUNT];
  unsigned i,j;
  if(!p || !out || p->vendor!=0x045eu || p->product!=0x028eu ||
     !spider_sdl3_profile_valid(p))return 0;
  /* RT is a separate SDL3 axis, not native rgbButtons; do not claim
   * support for a user reassignment of Swing to any other source. */
  if(p->actions[SPIDER_SDL3_ACTION_SWING].kind!=
        SPIDER_SDL3_BIND_TRIGGER ||
     p->actions[SPIDER_SDL3_ACTION_SWING].index!=
        SPIDER_SDL3_AXIS_RT)return 0;
  for(i=0;i<SPIDER_SDL3_ACTION_SWING;i++) {
    if(!spider_sdl3_xbox360_native_button(p->actions[i],
                                          &candidate[i]))return 0;
    for(j=0;j<i;j++)if(candidate[i]==candidate[j])return 0;
  }
  candidate[SPIDER_SDL3_ACTION_SWING]=0x4000u;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)out[i]=candidate[i];
  return 1;
}
#endif
