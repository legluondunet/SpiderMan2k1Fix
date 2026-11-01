#ifndef SPIDERMAN2K1FIX_SDL3_GENERIC_ENGINE_LATCH_REFERENCE_H
#define SPIDERMAN2K1FIX_SDL3_GENERIC_ENGINE_LATCH_REFERENCE_H
/*
 * Phase 1, read-only mathematical reference: the original game's
 * 0x00479540 x86 16-byte per-action state updater.
 * Not connected to the game, does not touch global 0x0066129C, and
 * does not model the 0x004796A0 post-update routine.
 * Crucially the +1 event byte is set on initial press but NOT cleared
 * by this original function; do not claim an automatic one-frame edge.
 */
#include <stdint.h>
typedef struct {
  uint8_t held;
  uint8_t event_indicator;
  uint8_t early_press;
  uint8_t reserved;
  uint32_t held_frames;
  uint32_t released_frames;
  uint32_t since_press;
} SpiderSDL3EngineLatchReference;
static inline int spider_sdl3_engine_latch_reference_step(
    SpiderSDL3EngineLatchReference *p,uint32_t action_mask) {
  uint32_t age;
  if(!p)return 0;
  age=p->since_press+1u;
  p->since_press=age;
  if(!p->held && action_mask) {
    p->event_indicator=1u;
    p->early_press=age<10u?1u:0u;
    p->since_press=0u;
  } else if(p->held) {
    p->early_press=0u;
  }
  p->held=action_mask?1u:0u;
  if(p->held) {
    p->held_frames++;
    p->released_frames=0u;
  } else {
    p->held_frames=0u;
    p->released_frames++;
  }
  return p->held?1:0;
}
#endif
