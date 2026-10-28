#ifndef SPIDERMAN2K1FIX_PS1_FOCUS_PAUSE_WAKE_H
#define SPIDERMAN2K1FIX_PS1_FOCUS_PAUSE_WAKE_H
/*
 * Pure C99 digital-button wake state for the custom Alt+Tab PAUSED screen.
 * This is NOT the original game's Escape/3D pause menu.
 *
 * A newly returned foreground session must first observe ALL relevant DS4
 * buttons released; a full new press AND release then wakes. Requiring
 * neutral first prevents a button held while Alt+Tabbing from auto-resuming.
 * Only physical digital buttons 0..11 are used, never stick/axis/POV drift.
 * No synthetic key or native gamepad state is changed by this state machine.
 */
#define SPIDER_PAUSE_PAD_DIGITAL_MASK 0x0fffu

typedef struct {
  unsigned seen_neutral;
  unsigned pending;
  unsigned initial_press;
} SpiderPausePadWake;

static unsigned spider_pause_pad_wake_step(SpiderPausePadWake *w,
                                           unsigned held_buttons) {
  unsigned result;
  if(!w)return 0u;
  held_buttons&=SPIDER_PAUSE_PAD_DIGITAL_MASK;
  if(!w->seen_neutral) {
    if(!held_buttons)w->seen_neutral=1u;
    return 0u;
  }
  if(!w->pending) {
    if(held_buttons) {
      w->pending=1u;
      w->initial_press=held_buttons;
    }
    return 0u;
  }
  if(held_buttons)return 0u;
  result=w->initial_press;
  w->pending=0u;
  w->initial_press=0u;
  return result;
}
#endif
