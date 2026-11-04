#ifndef SPIDERMAN2K1FIX_SDL3_GENERIC_FRAME_PREVIEW_H
#define SPIDERMAN2K1FIX_SDL3_GENERIC_FRAME_PREVIEW_H

/*
 * PHASE 1 - PORTABLE RESEARCH ONLY: single-instance SDL3 semantic input
 * lifecycle. Not included in any runtime C file. No native game pointer,
 * keyboard injection, DirectInput calls or executable memory modifications.
 *
 * The original 0x00505720 game consumer passes HELD input masks to the
 * stateful 0x00479540 action-latch updater, which derives transitions.
 * This preview preserves both HELD and independently computed fresh
 * diagnostic masks for comparison, but does NOT inject either into game.
 *
 * The caller MUST independently establish foreground, mode, SDL instance,
 * virtual-device duplication policy and safe native ownership if a later
 * code path ever uses these preview masks in gameplay. This struct does
 * NOT grant permission to modify the original engine.
 */
#include <stdint.h>
#include <string.h>
#include "sdl3_generic_engine_preview.h"

enum {
  SPIDER_SDL3_FRAME_REJECTED=0,
  SPIDER_SDL3_FRAME_WAIT_NEUTRAL=1,
  SPIDER_SDL3_FRAME_READY=2
};

typedef struct {
  uint32_t instance;
  uint16_t vid,pid;
  SpiderSDL3Binding actions[SPIDER_SDL3_ACTION_COUNT];
  uint32_t previous_actions;
  unsigned armed;
} SpiderSDL3GenericFramePreview;

static inline void spider_sdl3_generic_frame_reset(
    SpiderSDL3GenericFramePreview *state) {
  if(state)memset(state,0,sizeof(*state));
}

static inline int spider_sdl3_generic_frame_step(
    SpiderSDL3GenericFramePreview *state,
    const SpiderSDL3ControllerProfile *profile,
    const SpiderSDL3ControllerSample *sample,
    uint32_t instance,uint16_t vid,uint16_t pid,
    int selected_and_safe_context,
    uint32_t *held_mask,uint32_t *fresh_mask) {
  uint32_t current=0,next_actions=0;
  unsigned i,changed;
  if(held_mask)*held_mask=0u;
  if(fresh_mask)*fresh_mask=0u;
  if(!state || !held_mask || !fresh_mask || !profile || !sample ||
     !selected_and_safe_context || !instance || !vid || !pid ||
     profile->vendor!=vid || profile->product!=pid ||
     !spider_sdl3_profile_valid(profile)) {
    spider_sdl3_generic_frame_reset(state);
    return SPIDER_SDL3_FRAME_REJECTED;
  }
  changed=state->instance!=instance || state->vid!=vid ||
          state->pid!=pid;
  if(!changed)for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++) {
    if(!spider_sdl3_binding_equal(state->actions[i],profile->actions[i])) {
      changed=1u;
      break;
    }
  }
  if(changed) {
    spider_sdl3_generic_frame_reset(state);
    state->instance=instance;
    state->vid=vid;
    state->pid=pid;
    for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++)
      state->actions[i]=profile->actions[i];
  }
  if(!state->armed) {
    /* A held button/Start/trigger cannot create a fresh action when
     * returning from a menu, focus loss, hotplug or profile swap.
     * Analog movement drift alone is not treated as held Start. */
    if(spider_sdl3_profile_neutral(sample))
      state->armed=1u;
    return SPIDER_SDL3_FRAME_WAIT_NEUTRAL;
  }
  if(!spider_sdl3_generic_engine_preview_edges(
       profile,sample,state->previous_actions,
       &current,fresh_mask,&next_actions)) {
    spider_sdl3_generic_frame_reset(state);
    *fresh_mask=0u;
    return SPIDER_SDL3_FRAME_REJECTED;
  }
  *held_mask=current;
  state->previous_actions=next_actions;
  return SPIDER_SDL3_FRAME_READY;
}
#endif
