/* Observational integrity check of the retail game's original 14-slot
 * DirectInput controller table. No pre-SDL3 Sony/Xbox mapping presets.
 * Only verifies table format and reports actual existing user bindings;
 * it does not modify any entry or infer a device's physical button order.
 */
#ifndef SPIDERMAN2K1FIX_NATIVE_JOYSTICK_SWAP_PROBE_H
#define SPIDERMAN2K1FIX_NATIVE_JOYSTICK_SWAP_PROBE_H
#include <stddef.h>
#include <stdint.h>
enum { SPIDER_NATIVE_SWAP_SLOTS=14, SPIDER_NATIVE_SWAP_FIRST_ACTION=4,
       SPIDER_NATIVE_GAMEPLAY_ACTIONS=7 };
static const uint32_t spider_native_swap_masks[SPIDER_NATIVE_SWAP_SLOTS]={
  0x1u,0x2u,0x4u,0x8u,0x10u,0x20u,0x40u,0x80u,
  0x100u,0x200u,0x400u,0x1000u,0x2000u,0x8000u
};
static inline uint32_t spider_native_u32(const uint8_t *p,size_t off) {
  return (uint32_t)p[off]|((uint32_t)p[off+1]<<8)|
         ((uint32_t)p[off+2]<<16)|((uint32_t)p[off+3]<<24);
}
static inline int spider_native_swap_read(
    const uint8_t *table,size_t bytes,
    uint32_t buttons[SPIDER_NATIVE_GAMEPLAY_ACTIONS]) {
  unsigned i;
  if(!table||!buttons||bytes<SPIDER_NATIVE_SWAP_SLOTS*8u)return 0;
  for(i=0;i<SPIDER_NATIVE_SWAP_SLOTS;i++) {
    uint32_t mask=spider_native_u32(table,i*8u);
    uint32_t button=spider_native_u32(table,i*8u+4u);
    if(mask!=spider_native_swap_masks[i])return 0;
    if(button>127u && button!=0x4000u)return 0;
    if((i<4u || i==12u) && button!=0x4000u)return 0;
    if(i==13u && button!=0u)return 0;
  }
  for(i=0;i<SPIDER_NATIVE_GAMEPLAY_ACTIONS;i++)
    buttons[i]=spider_native_u32(table,
                  (SPIDER_NATIVE_SWAP_FIRST_ACTION+i)*8u+4u);
  return 1;
}
#endif
