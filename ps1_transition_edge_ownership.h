#ifndef SPIDERMAN2K1FIX_PS1_TRANSITION_EDGE_OWNERSHIP_H
#define SPIDERMAN2K1FIX_PS1_TRANSITION_EDGE_OWNERSHIP_H
/*
 * Native Quit (request 7) owns a Cross/Options press that is ALREADY
 * depressed on the first 2D menu Flip after a verified 3D session.
 *
 * The original game consumes the final "Yes" input before the separate
 * DirectInput8 UI adapter next samples that input at the Flip boundary.
 * Without an ownership boundary, the adapter sends a second Enter into
 * the brand-new main menu, starting New Game.
 *
 * This module is pure C99, with no time delay, OS calls, native memory
 * writes, logging or input synthesis. It reports a per-button mask of
 * old-session confirmation bits until each individual physical release.
 * A fresh press of the OTHER confirmation button must remain eligible.
 */
typedef struct {
  unsigned seen_3d;
  unsigned inherited_buttons;
} SpiderPS1TransitionEdgeOwnership;

static unsigned spider_ps1_transition_edge_ownership_flip(
    SpiderPS1TransitionEdgeOwnership *owner,
    int enabled,int menu2d,int gameplay3d,int movie,
    unsigned native_request,unsigned pressed_confirm_mask) {
  if(!owner)return 0;
  if(!enabled) {
    owner->seen_3d=0u;
    owner->inherited_buttons=0u;
    return 0;
  }
  /* 3D may include the native pause/Yes menu. */
  if(gameplay3d && !menu2d && !movie) {
    owner->seen_3d=1u;
    owner->inherited_buttons=0u;
    return 0;
  }
  if(menu2d && !gameplay3d && !movie) {
    if(owner->seen_3d) {
      owner->seen_3d=0u;
      /* Only original native Quit, not other original transitions. */
      if(native_request==7u)
        owner->inherited_buttons=pressed_confirm_mask;
    }
    /* Clear every inherited bit as soon as it is released. A *new*
     * Cross press after release is immediately eligible for the new
     * menu, even if it is less than 8 frames after the native return. */
    owner->inherited_buttons &= pressed_confirm_mask;
    return owner->inherited_buttons;
  }
  return 0;
}
#endif
