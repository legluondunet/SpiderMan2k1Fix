#ifndef SPIDERMAN2K1FIX_SDL3_UNIVERSAL_POLICY_H
#define SPIDERMAN2K1FIX_SDL3_UNIVERSAL_POLICY_H
/* Portable, manufacturer-independent SDL3 semantic -> 2001 keyboard action
 * policy. Actual key scanning/sending and DI ownership stay Win32-only. */
#include <stdint.h>
#include <string.h>
#include "sdl3_controller_profile.h"
enum {
  SPIDER_UNIVERSAL_UP=0, SPIDER_UNIVERSAL_DOWN=1,
  SPIDER_UNIVERSAL_LEFT=2, SPIDER_UNIVERSAL_RIGHT=3,
  SPIDER_UNIVERSAL_JUMP=4, SPIDER_UNIVERSAL_KICK=5,
  SPIDER_UNIVERSAL_PUNCH=6, SPIDER_UNIVERSAL_WEB=7,
  SPIDER_UNIVERSAL_AIM=8, SPIDER_UNIVERSAL_ZIPLINE=9,
  SPIDER_UNIVERSAL_SWING=10, SPIDER_UNIVERSAL_KEY_COUNT=11
};
enum { SPIDER_UNIVERSAL_DPAD_UP=11, SPIDER_UNIVERSAL_DPAD_DOWN=12,
       SPIDER_UNIVERSAL_DPAD_LEFT=13, SPIDER_UNIVERSAL_DPAD_RIGHT=14 };
typedef struct {
  uint32_t selected_id;
  uint32_t held_actions;
  unsigned armed;
} SpiderSDL3UniversalState;
static inline void spider_sdl3_universal_reset(SpiderSDL3UniversalState *state) {
  if(state)memset(state,0,sizeof(*state));
}
static inline int spider_sdl3_universal_neutral(
    const SpiderSDL3ControllerSample *sample) {
  if(!sample || !spider_sdl3_profile_neutral(sample))return 0;
  return sample->axis[SPIDER_SDL3_AXIS_LEFTX]>-9000 &&
         sample->axis[SPIDER_SDL3_AXIS_LEFTX]<9000 &&
         sample->axis[SPIDER_SDL3_AXIS_LEFTY]>-9000 &&
         sample->axis[SPIDER_SDL3_AXIS_LEFTY]<9000;
}
/* Return 1 if live, 0 while waiting for neutral, -1 if unsupported.
 * Keyboard desired held actions have eleven bit positions. Pause is an
 * independent edge, never a permanently held keyboard scancode. */
static inline int spider_sdl3_universal_step(SpiderSDL3UniversalState *state,
    const SpiderSDL3ControllerProfile *profile,
    const SpiderSDL3ControllerSample *sample,uint32_t id,int native_pause,
    uint32_t *desired,unsigned *pause_edge) {
  uint32_t actions=0,mask=0;
  const int16_t x=sample?sample->axis[SPIDER_SDL3_AXIS_LEFTX]:0;
  const int16_t y=sample?sample->axis[SPIDER_SDL3_AXIS_LEFTY]:0;
  if(desired)*desired=0;
  if(pause_edge)*pause_edge=0;
  if(!state || !profile || !sample || !desired || !pause_edge || !id ||
     !spider_sdl3_profile_valid(profile)) {
    spider_sdl3_universal_reset(state);
    return -1;
  }
  if(state->selected_id!=id) {
    spider_sdl3_universal_reset(state);
    state->selected_id=id;
  }
  if(!state->armed) {
    if(spider_sdl3_universal_neutral(sample))state->armed=1;
    return 0;
  }
  if(!spider_sdl3_profile_actions(profile,sample,state->held_actions,
                                  &actions)) {
    spider_sdl3_universal_reset(state);return -1;
  }
  *pause_edge=((actions&(1u<<SPIDER_SDL3_ACTION_PAUSE))!=0u) &&
              ((state->held_actions&(1u<<SPIDER_SDL3_ACTION_PAUSE))==0u);
  state->held_actions=actions;
  if(native_pause)return 1; /* no movement/combat while native menu open */
  if(y<-12000 || (sample->buttons&(1u<<SPIDER_UNIVERSAL_DPAD_UP)))
    mask|=1u<<SPIDER_UNIVERSAL_UP;
  if(y>12000 || (sample->buttons&(1u<<SPIDER_UNIVERSAL_DPAD_DOWN)))
    mask|=1u<<SPIDER_UNIVERSAL_DOWN;
  if(x<-12000 || (sample->buttons&(1u<<SPIDER_UNIVERSAL_DPAD_LEFT)))
    mask|=1u<<SPIDER_UNIVERSAL_LEFT;
  if(x>12000 || (sample->buttons&(1u<<SPIDER_UNIVERSAL_DPAD_RIGHT)))
    mask|=1u<<SPIDER_UNIVERSAL_RIGHT;
  if((mask&3u)==3u)mask&=~3u;
  if((mask&12u)==12u)mask&=~12u;
  mask|=(actions&0x7fu)<<SPIDER_UNIVERSAL_JUMP;
  *desired=mask;
  return 1;
}
#endif
