/* Pure portable controller-setup navigation edge policy.
 * Unlike capture, navigation accepts only reserved UI buttons and only
 * after complete physical neutral. Gamepad inputs are never injected.
 * Reset on dialog focus loss, selection changes or capture transition. */
#ifndef SPIDERMAN2K1FIX_SDL3_CONTROLLER_SETUP_NAV_H
#define SPIDERMAN2K1FIX_SDL3_CONTROLLER_SETUP_NAV_H
#include "sdl3_controller_profile.h"
enum {
  SPIDER_SETUP_NAV_SOUTH=0, SPIDER_SETUP_NAV_EAST=1,
  SPIDER_SETUP_NAV_NORTH=3, SPIDER_SETUP_NAV_LB=9,
  SPIDER_SETUP_NAV_RB=10, SPIDER_SETUP_NAV_UP=11,
  SPIDER_SETUP_NAV_DOWN=12
};
typedef struct {
  unsigned armed;
  uint32_t previous;
} SpiderSDL3SetupNav;
static inline void spider_sdl3_setup_nav_reset(SpiderSDL3SetupNav *n) {
  if(n){n->armed=0;n->previous=0;}
}
static inline uint32_t spider_sdl3_setup_nav_step(
    SpiderSDL3SetupNav *n,const SpiderSDL3ControllerSample *sample) {
  const uint32_t accepted=(1u<<SPIDER_SETUP_NAV_SOUTH)|
    (1u<<SPIDER_SETUP_NAV_EAST)|(1u<<SPIDER_SETUP_NAV_NORTH)|
    (1u<<SPIDER_SETUP_NAV_LB)|(1u<<SPIDER_SETUP_NAV_RB)|
    (1u<<SPIDER_SETUP_NAV_UP)|(1u<<SPIDER_SETUP_NAV_DOWN);
  uint32_t edges;
  if(!n || !sample)return 0;
  if(sample->axis[SPIDER_SDL3_AXIS_LT]<0 ||
     sample->axis[SPIDER_SDL3_AXIS_RT]<0) {
    spider_sdl3_setup_nav_reset(n);return 0;
  }
  if(!n->armed) {
    if(spider_sdl3_profile_neutral(sample)) {
      n->armed=1;n->previous=0;
    }
    return 0;
  }
  /* Guide/Start should not navigate the wizard. Wait for a fresh
   * neutral after either to prevent using a held controller switch. */
  if(sample->buttons&((1u<<SPIDER_SDL3_BUTTON_START)|
                      (1u<<SPIDER_SDL3_BUTTON_GUIDE))) {
    spider_sdl3_setup_nav_reset(n);return 0;
  }
  edges=(sample->buttons&~n->previous)&accepted;
  n->previous=sample->buttons;
  /* If multiple UI commands arrive on the same sample, do nothing,
   * rearm only after neutral; no accidental close/reset. */
  if(edges && (edges&(edges-1u))) {
    spider_sdl3_setup_nav_reset(n);return 0;
  }
  return edges;
}
#endif
