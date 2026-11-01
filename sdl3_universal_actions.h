/* Hardware-neutral SDL3 semantic action adapter for Spider-Man (PC 2001).
 * Input is SDL_GamepadButton/Axis normalized state and an arbitrary saved
 * profile, NEVER a manufacturer-specific DirectInput button number.
 *
 * Stage 1 is a portable, read-only action engine. It deliberately does not
 * inject native game actions: the original binary's input consumer must be
 * separately proven before the bridge can be enabled safely.
 */
#ifndef SPIDERMAN2K1FIX_SDL3_UNIVERSAL_ACTIONS_H
#define SPIDERMAN2K1FIX_SDL3_UNIVERSAL_ACTIONS_H
#include <stdint.h>
#include <string.h>
#include "sdl3_controller_profile.h"

typedef struct {
  uint32_t instance_id;
  uint32_t held;
  unsigned armed;
} SpiderSDL3UniversalActions;
typedef struct {
  uint32_t held,pressed,released;
  unsigned valid;
} SpiderSDL3UniversalFrame;

static inline void spider_sdl3_universal_reset(
    SpiderSDL3UniversalActions *s) {
  if(s)memset(s,0,sizeof(*s));
}
/* Require an entirely neutral sample after selecting a controller,
 * changing the profile or returning from focus loss. First neutral
 * frame arms but generates no game action. Every failure disarms.
 * Physical Start is reserved for controller selection as elsewhere.
 */
static inline SpiderSDL3UniversalFrame spider_sdl3_universal_step(
    SpiderSDL3UniversalActions *state,
    const SpiderSDL3ControllerProfile *profile,
    const SpiderSDL3ControllerSample *sample,
    uint32_t selected_instance,int foreground,int gameplay) {
  SpiderSDL3UniversalFrame f={0,0,0,0};
  uint32_t held=0;
  if(!state)return f;
  if(!selected_instance || !foreground || !gameplay ||
     !profile || !sample || !spider_sdl3_profile_valid(profile) ||
     !spider_sdl3_profile_actions(profile,sample,state->held,&held)) {
    spider_sdl3_universal_reset(state);
    return f;
  }
  if(state->instance_id!=selected_instance) {
    spider_sdl3_universal_reset(state);
    state->instance_id=selected_instance;
  }
  f.valid=1;
  if(!state->armed) {
    if(spider_sdl3_profile_neutral(sample))state->armed=1;
    return f;
  }
  f.held=held;
  f.pressed=held&~state->held;
  f.released=state->held&~held;
  state->held=held;
  return f;
}
#endif
