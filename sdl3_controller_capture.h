/* Pure, portable one-control-at-a-time SDL3 binding capture.
 * No OS calls, native writes, keyboard synthesis or SDL event consumption.
 * Fresh neutral -> exactly one supported physical button / trigger edge.
 * The selected device identity is owned by the caller; cancel this capture
 * on device switch, unplug, focus loss or dialog close. */
#ifndef SPIDERMAN2K1FIX_SDL3_CONTROLLER_CAPTURE_H
#define SPIDERMAN2K1FIX_SDL3_CONTROLLER_CAPTURE_H
#include "sdl3_controller_profile.h"
typedef struct {
  unsigned armed;
  unsigned active;
} SpiderSDL3Capture;
static inline void spider_sdl3_capture_reset(SpiderSDL3Capture *c) {
  if(c) {c->armed=0;c->active=0;}
}
static inline void spider_sdl3_capture_start(SpiderSDL3Capture *c) {
  if(c) {c->active=1;c->armed=0;}
}
/* 0 = waiting; 1 = unique candidate; -1 = invalid data / capture reset.
 * The caller must validate and apply binding atomically to a profile.
 * No committed mapping is altered merely by calling this function. */
static inline int spider_sdl3_capture_feed(
    SpiderSDL3Capture *c,const SpiderSDL3ControllerSample *s,
    SpiderSDL3Binding *candidate) {
  unsigned i,n=0;
  SpiderSDL3Binding b=spider_sdl3_bind(0,0);
  if(candidate)*candidate=b;
  if(!c || !s || !candidate || !c->active)return -1;
  if(s->axis[SPIDER_SDL3_AXIS_LT]<0 ||
     s->axis[SPIDER_SDL3_AXIS_RT]<0 ||
     s->axis[SPIDER_SDL3_AXIS_LT]>32767 ||
     s->axis[SPIDER_SDL3_AXIS_RT]>32767) {
    c->armed=0;return -1;
  }
  if(!c->armed) {
    if(spider_sdl3_profile_neutral(s))c->armed=1;
    return 0;
  }
  /* START/GUIDE never count as candidate, and abort a held capture
   * until the entire pad has returned to neutral again. */
  if(s->buttons&((1u<<SPIDER_SDL3_BUTTON_START)|
                (1u<<SPIDER_SDL3_BUTTON_GUIDE))) {
    c->armed=0;return 0;
  }
  for(i=0;i<SPIDER_SDL3_BUTTON_COUNT;i++)
    if(s->buttons&(1u<<i)) {
      if(!spider_sdl3_binding_valid(spider_sdl3_bind(
             SPIDER_SDL3_BIND_BUTTON,i)))continue;
      b=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,i);n++;
    }
  if(s->axis[SPIDER_SDL3_AXIS_LT]>=11000) {
    b=spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_LT);n++;
  }
  if(s->axis[SPIDER_SDL3_AXIS_RT]>=11000) {
    b=spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_RT);n++;
  }
  if(n!=1) {
    if(n>1)c->armed=0; /* ambiguous: require release before retry */
    return 0;
  }
  *candidate=b;c->active=0;c->armed=0;
  return 1;
}
#endif
