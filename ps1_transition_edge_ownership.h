#ifndef SPIDERMAN2K1FIX_PS1_TRANSITION_EDGE_OWNERSHIP_H
#define SPIDERMAN2K1FIX_PS1_TRANSITION_EDGE_OWNERSHIP_H
/*
 * Source compatibility: the original Sony PS1 profile API remains, but
 * native request-7 confirmation ownership is now shared with Xbox in
 * native_menu_return_gate.h. Only synthetic Return may be suppressed.
 * No timing assumptions, controller-type decisions or Win32 calls.
 */
#include "native_menu_return_gate.h"
typedef SpiderNativeMenuReturnGate SpiderPS1TransitionEdgeOwnership;

static inline unsigned spider_ps1_transition_edge_ownership_flip(
    SpiderPS1TransitionEdgeOwnership *owner,
    int enabled,int menu2d,int gameplay3d,int movie,
    unsigned native_request,unsigned pressed_confirm_mask) {
  return spider_native_menu_return_gate_flip(
    owner,enabled,menu2d,gameplay3d,movie,native_request,
    pressed_confirm_mask);
}
#endif
