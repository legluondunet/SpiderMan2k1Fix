/* Pure gamepad focus-column policy for the SDL3 setup modal.
 * Windows UI focus is applied by the dialog on the original game thread.
 * Never mutates game input, SDL3 bindings, or Win32 controls itself. */
#ifndef SPIDERMAN2K1FIX_SDL3_SETUP_FOCUS_H
#define SPIDERMAN2K1FIX_SDL3_SETUP_FOCUS_H
#include "sdl3_controller_setup_nav.h"
enum {
  SPIDER_SETUP_FOCUS_ACTIONS=0,SPIDER_SETUP_FOCUS_BUTTONS=1,
  SPIDER_SETUP_BUTTON_CAPTURE=0,SPIDER_SETUP_BUTTON_RESET=1,
  SPIDER_SETUP_BUTTON_CLOSE=2,SPIDER_SETUP_BUTTON_COUNT=3
};
typedef struct {
  unsigned column;
  unsigned button;
} SpiderSDL3SetupFocus;
static inline void spider_sdl3_setup_focus_reset(SpiderSDL3SetupFocus *f) {
  if(f) {f->column=SPIDER_SETUP_FOCUS_ACTIONS;f->button=0;}
}
/* 1 means focus selection changed, 0 leaves it as it was.
 * Vertical navigation in the actions column remains owned by the
 * existing eight-action dialog list, not by this policy. */
static inline int spider_sdl3_setup_focus_step(
    SpiderSDL3SetupFocus *f,uint32_t edge) {
  if(!f)return 0;
  if(edge==(1u<<SPIDER_SETUP_NAV_RIGHT)) {
    if(f->column==SPIDER_SETUP_FOCUS_BUTTONS)return 0;
    f->column=SPIDER_SETUP_FOCUS_BUTTONS;
    return 1;
  }
  if(edge==(1u<<SPIDER_SETUP_NAV_LEFT)) {
    if(f->column==SPIDER_SETUP_FOCUS_ACTIONS)return 0;
    f->column=SPIDER_SETUP_FOCUS_ACTIONS;
    return 1;
  }
  if(f->column!=SPIDER_SETUP_FOCUS_BUTTONS)return 0;
  if(edge==(1u<<SPIDER_SETUP_NAV_UP)) {
    f->button=(f->button+SPIDER_SETUP_BUTTON_COUNT-1u)%
              SPIDER_SETUP_BUTTON_COUNT;
    return 1;
  }
  if(edge==(1u<<SPIDER_SETUP_NAV_DOWN)) {
    f->button=(f->button+1u)%SPIDER_SETUP_BUTTON_COUNT;
    return 1;
  }
  return 0;
}
#endif
