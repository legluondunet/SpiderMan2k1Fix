#ifndef SPIDERMAN2K1FIX_SDL3_ORIGINAL_ACTION_LAYOUT_H
#define SPIDERMAN2K1FIX_SDL3_ORIGINAL_ACTION_LAYOUT_H
/*
 * Exact SHA-verified 32-bit retail SpideyPC.exe, pure data reference.
 * Native 0x00505720 invokes 0x00479540 twenty times per call:
 * state records VA 0x00661100..0x00661230, 16 bytes each.
 * Mask order recovered from the actual x86 at 0x00505759..0x005058E3.
 * Mask 0 is an intentional always-released entry; repeated nonzero
 * masks are intentionally distinct records, not errors.
 *
 * This header is not connected to the game. It only describes the
 * original memory layout for future passive context/counter studies.
 * No DirectInput hook and no rights to write these addresses.
 */
#include <stdint.h>
enum {
  SPIDER_ORIGINAL_INPUT_STATE_BASE = 0x00661100u,
  SPIDER_ORIGINAL_INPUT_STATE_STRIDE = 16u,
  SPIDER_ORIGINAL_INPUT_STATE_COUNT = 20u,
  SPIDER_ORIGINAL_INPUT_GLOBAL_COUNTER = 0x0066129Cu
};
static inline uint32_t spider_original_action_latch_address(unsigned slot) {
  return slot<SPIDER_ORIGINAL_INPUT_STATE_COUNT?
      SPIDER_ORIGINAL_INPUT_STATE_BASE+
        slot*SPIDER_ORIGINAL_INPUT_STATE_STRIDE:0u;
}
static inline uint32_t spider_original_action_latch_mask(unsigned slot) {
  static const uint32_t masks[SPIDER_ORIGINAL_INPUT_STATE_COUNT]={
    0x80u,0x40u,0x20u,0x10u,0x100u,0u,0x200u,0x400u,
    0x04u,0x08u,0x01u,0x02u,0u,0u,0x1000u,0u,
    0x10u,0x80u,0x40u,0x20u
  };
  return slot<SPIDER_ORIGINAL_INPUT_STATE_COUNT?masks[slot]:0u;
}
static inline int spider_original_action_latch_held(
    uint32_t shared_held_mask,unsigned slot) {
  uint32_t mask=spider_original_action_latch_mask(slot);
  return slot<SPIDER_ORIGINAL_INPUT_STATE_COUNT && mask &&
         (shared_held_mask&mask)!=0;
}
#endif
