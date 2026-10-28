/* Passive, engine-owned 14-action DirectInput joystick table fingerprint.
 *
 * Exact supported 2001 EXE: the 14 (action-mask, button-index) pairs at
 * RVA 0x168408 were identified in the original Start-button read-only
 * analysis. The seven gameplay actions are indices 4..10.
 *
 * A canonical match is useful diagnostic evidence, NOT permission to
 * change the original game's 112 bytes at runtime.
 *
 * Pure C99, host-testable without Wine/Windows. */
#ifndef SPIDERMAN2K1FIX_NATIVE_JOYSTICK_SWAP_PROBE_H
#define SPIDERMAN2K1FIX_NATIVE_JOYSTICK_SWAP_PROBE_H
#include <stdint.h>
#include <stddef.h>
#include "ps1_gamepad_mapping.h"
#include "xbox360_gameplay_mapping.h"
enum { SPIDER_NATIVE_SWAP_SLOTS=14, SPIDER_NATIVE_SWAP_FIRST_ACTION=4 };
static const uint32_t spider_native_swap_masks[SPIDER_NATIVE_SWAP_SLOTS]={
  0x1u,0x2u,0x4u,0x8u,0x10u,0x20u,0x40u,0x80u,
  0x100u,0x200u,0x400u,0x1000u,0x2000u,0x8000u
};
static inline int spider_native_swap_read(const uint8_t *table,size_t bytes,
                                          uint32_t buttons[SPIDER_PS1_BUTTON_COUNT]) {
  unsigned i;
  if(!table || !buttons || bytes<SPIDER_NATIVE_SWAP_SLOTS*8u)return 0;
  for(i=0;i<SPIDER_NATIVE_SWAP_SLOTS;i++) {
    uint32_t mask=spider_ps1_read_word(table,8u*i);
    uint32_t button=spider_ps1_read_word(table,8u*i+4u);
    if(mask!=spider_native_swap_masks[i])return 0;
    if(button>127u && button!=0x4000u)return 0;
    if((i<4u || i==12u) && button!=0x4000u)return 0;
    if(i==13u && button!=0u)return 0;
  }
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    buttons[i]=spider_ps1_read_word(table,(SPIDER_NATIVE_SWAP_FIRST_ACTION+i)*8u+4u);
  return 1;
}
static inline unsigned spider_native_swap_preset(
    const uint32_t buttons[SPIDER_PS1_BUTTON_COUNT]) {
  unsigned i,ds4=1,xbox=1;
  if(!buttons)return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++) {
    if(buttons[i]!=spider_ps1_buttons[i])ds4=0;
    if(buttons[i]!=spider_xbox360_buttons[i])xbox=0;
  }
  return ds4?1u:(xbox?2u:0u);
}
#endif
