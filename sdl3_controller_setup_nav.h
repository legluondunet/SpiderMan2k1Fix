/* Pure portable controller-setup navigation edge policy.
 * Navigation uses reserved UI buttons and the LEFT stick only. Requiring
 * a centered stick before arming, a 19000 threshold to move, and a 9000
 * release threshold avoids drift and held-input repeated activations.
 * Stick Y = action rows, X = device selector; NEVER synthesize A/B/Y,
 * capture a 3D binding, reset profiles or inject keyboard input.
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
  uint32_t edges,physical,analog=0,old;
  int x,y;
  if(!n || !sample)return 0;
  x=(int)sample->axis[SPIDER_SDL3_AXIS_LEFTX];
  y=(int)sample->axis[SPIDER_SDL3_AXIS_LEFTY];
  if(sample->axis[SPIDER_SDL3_AXIS_LT]<0 ||
     sample->axis[SPIDER_SDL3_AXIS_RT]<0) {
    spider_sdl3_setup_nav_reset(n);return 0;
  }
  if(!n->armed) {
    /* A held stick is not a new navigation event. The usual profile
     * neutral test intentionally ignores stick drift, so check both
     * axes explicitly here before the wizard can be armed. */
    if(spider_sdl3_profile_neutral(sample) &&
       x>-9000 && x<9000 && y>-9000 && y<9000) {
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
  /* SDL3 stick Y grows downward. One tilt -> one UI action; the stick
   * must recenter (<9000) to rearm. Hysteresis survives intermediate
   * samples without allowing noisy 15000-level axis jitter. */
  old=n->previous;
  if(y<=-19000 || ((old&(1u<<SPIDER_SETUP_NAV_UP)) && y<=-9000))
    analog|=1u<<SPIDER_SETUP_NAV_UP;
  if(y>=19000 || ((old&(1u<<SPIDER_SETUP_NAV_DOWN)) && y>=9000))
    analog|=1u<<SPIDER_SETUP_NAV_DOWN;
  if(x<=-19000 || ((old&(1u<<SPIDER_SETUP_NAV_LB)) && x<=-9000))
    analog|=1u<<SPIDER_SETUP_NAV_LB;
  if(x>=19000 || ((old&(1u<<SPIDER_SETUP_NAV_RB)) && x>=9000))
    analog|=1u<<SPIDER_SETUP_NAV_RB;
  physical=(sample->buttons&accepted)|analog;
  edges=physical&~old;
  n->previous=sample->buttons|analog;
  /* If multiple UI commands arrive on the same sample, do nothing,
   * rearm only after neutral; no accidental close/reset. */
  if(edges && (edges&(edges-1u))) {
    spider_sdl3_setup_nav_reset(n);return 0;
  }
  return edges;
}
#endif
