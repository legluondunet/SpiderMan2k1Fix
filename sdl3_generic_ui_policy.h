/* PS3/other generic SDL3 pad -> single foreground 2D/menu or Bink pulse.
 * Portable, testable policy; ZERO Windows, native DirectInput, or SDL calls.
 * Physical START/GUIDE are never actions. A full neutral frame must arm
 * each new press, including after focus, controller or engine-mode changes.
 * A held South can never auto-skip a following video/scene.
 * No 3D gameplay, scripted cutscene, still or global key synthesis here. */
#ifndef SPIDERMAN2K1FIX_SDL3_GENERIC_UI_POLICY_H
#define SPIDERMAN2K1FIX_SDL3_GENERIC_UI_POLICY_H
#include <stdint.h>
#include <string.h>
#include "sdl3_controller_profile.h"

enum {
  SPIDER_GENERIC_UI_NONE=0,
  SPIDER_GENERIC_UI_MENU=1,
  SPIDER_GENERIC_UI_BINK=2,
  /* Values are DirectInput DIK codes, NOT Unicode characters. */
  SPIDER_GENERIC_DIK_UP=0xC8,
  SPIDER_GENERIC_DIK_DOWN=0xD0,
  SPIDER_GENERIC_DIK_LEFT=0xCB,
  SPIDER_GENERIC_DIK_RIGHT=0xCD,
  SPIDER_GENERIC_DIK_ENTER=0x1C,
  SPIDER_GENERIC_DIK_ESCAPE=0x01
};
typedef struct {
  uint32_t id;
  unsigned context;
  unsigned armed;
  unsigned awaiting_neutral;
} SpiderSDL3GenericUIState;

static inline void spider_sdl3_generic_ui_reset(SpiderSDL3GenericUIState *s) {
  if(s)memset(s,0,sizeof(*s));
}
static inline int spider_sdl3_generic_ui_neutral(
    const SpiderSDL3ControllerSample *sample) {
  unsigned i;
  if(!spider_sdl3_profile_neutral(sample))return 0;
  /* The 3D left stick may be held at a menu or Bink boundary. */
  for(i=0;i<2u;i++)
    if(sample->axis[i]>10000 || sample->axis[i]<-10000)return 0;
  return 1;
}
/* Return ONE DIK scan after a new neutral->press edge. The caller is
 * responsible for a bounded down/up pulse on the VERIFIED game window.
 * Invalid/ambiguous multi-action inputs are consumed until neutral. */
static inline unsigned spider_sdl3_generic_ui_step(
    SpiderSDL3GenericUIState *state,uint32_t instance,
    unsigned context,const SpiderSDL3ControllerSample *sample) {
  uint32_t buttons;
  unsigned count=0,scan=0;
  if(!state)return 0;
  if(!instance || !sample ||
     (context!=SPIDER_GENERIC_UI_MENU && context!=SPIDER_GENERIC_UI_BINK) ||
     sample->axis[SPIDER_SDL3_AXIS_LT]<0 ||
     sample->axis[SPIDER_SDL3_AXIS_RT]<0) {
    spider_sdl3_generic_ui_reset(state);
    return 0;
  }
  if(state->id!=instance || state->context!=context) {
    spider_sdl3_generic_ui_reset(state);
    state->id=instance;
    state->context=context;
  }
  if(spider_sdl3_generic_ui_neutral(sample)) {
    state->armed=1;
    state->awaiting_neutral=0;
    return 0;
  }
  if(!state->armed || state->awaiting_neutral)return 0;
  state->awaiting_neutral=1;
  buttons=sample->buttons;
  /* No command for START/GUIDE, a trigger or an unknown extra button.
   * Suppress simultaneous controls instead of choosing an arbitrary one. */
  if(sample->axis[SPIDER_SDL3_AXIS_LT]>=5000 ||
     sample->axis[SPIDER_SDL3_AXIS_RT]>=5000)return 0;
  if(context==SPIDER_GENERIC_UI_BINK) {
    if(buttons==(1u<<0))return SPIDER_GENERIC_DIK_ENTER;
    return 0;
  }
  if(buttons&(1u<<0)) {scan=SPIDER_GENERIC_DIK_ENTER;count++;}
  if(buttons&(1u<<1)) {scan=SPIDER_GENERIC_DIK_ESCAPE;count++;}
  if(buttons&(1u<<11)) {scan=SPIDER_GENERIC_DIK_UP;count++;}
  if(buttons&(1u<<12)) {scan=SPIDER_GENERIC_DIK_DOWN;count++;}
  if(buttons&(1u<<13)) {scan=SPIDER_GENERIC_DIK_LEFT;count++;}
  if(buttons&(1u<<14)) {scan=SPIDER_GENERIC_DIK_RIGHT;count++;}
  if(sample->axis[SPIDER_SDL3_AXIS_LEFTX]>=16000) {
    scan=SPIDER_GENERIC_DIK_RIGHT;count++;
  } else if(sample->axis[SPIDER_SDL3_AXIS_LEFTX]<=-16000) {
    scan=SPIDER_GENERIC_DIK_LEFT;count++;
  }
  if(sample->axis[SPIDER_SDL3_AXIS_LEFTY]>=16000) {
    scan=SPIDER_GENERIC_DIK_DOWN;count++;
  } else if(sample->axis[SPIDER_SDL3_AXIS_LEFTY]<=-16000) {
    scan=SPIDER_GENERIC_DIK_UP;count++;
  }
  /* A single actionable input is mandatory. This also rejects a face
   * button held with a shoulder, START or an unknown hardware button. */
  if(count!=1u)return 0;
  if(buttons & ~((1u<<0)|(1u<<1)|
                 (1u<<11)|(1u<<12)|(1u<<13)|(1u<<14)))return 0;
  return scan;
}
#endif
