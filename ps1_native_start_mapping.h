#ifndef SPIDERMAN2K1FIX_PS1_NATIVE_START_MAPPING_H
#define SPIDERMAN2K1FIX_PS1_NATIVE_START_MAPPING_H

/* Exact retail (SHA-256-gated) native action/button map:
 * 14 little-endian (action, button) uint32 pairs.
 * The original four unused action slots and action 12 hold 0x4000;
 * this is the game's own unbound-button sentinel, NOT a real button.
 *
 * This helper only validates bytes, and never writes game memory.
 * Experimental unbinding requires an explicit INI opt-in, one unique
 * Sony DS4, a complete writable-.data scan and exactly one strict match.
 */
#include <stddef.h>
#include <stdint.h>

#define SPIDER_PS1_START_MAP_COUNT 14u
#define SPIDER_PS1_START_MAP_BYTES (SPIDER_PS1_START_MAP_COUNT*8u)
#define SPIDER_PS1_START_INDEX 11u
#define SPIDER_PS1_START_BUTTON_OFFSET (SPIDER_PS1_START_INDEX*8u+4u)
#define SPIDER_PS1_START_BUTTON_FACTORY 11u
#define SPIDER_PS1_START_BUTTON_UNBOUND 0x4000u

static inline uint32_t spider_ps1_start_word(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
         ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline int spider_ps1_start_strict_factory(const uint8_t *p,size_t len) {
  static const uint32_t actions[SPIDER_PS1_START_MAP_COUNT]={
    0x1u,0x2u,0x4u,0x8u,0x10u,0x20u,0x40u,0x80u,
    0x100u,0x200u,0x400u,0x1000u,0x2000u,0x8000u};
  unsigned i;
  if(!p || len<SPIDER_PS1_START_MAP_BYTES)return 0;
  for(i=0;i<SPIDER_PS1_START_MAP_COUNT;i++) {
    uint32_t button=spider_ps1_start_word(p+8u*i+4u);
    if(spider_ps1_start_word(p+8u*i)!=actions[i])return 0;
    if(i<4u || i==12u) {
      if(button!=SPIDER_PS1_START_BUTTON_UNBOUND)return 0;
    } else if(i==SPIDER_PS1_START_INDEX) {
      if(button!=SPIDER_PS1_START_BUTTON_FACTORY)return 0;
    } else if(i==13u) {
      if(button!=0u)return 0;
    } else if(button>31u && button!=SPIDER_PS1_START_BUTTON_UNBOUND) {
      return 0;
    }
  }
  return 1;
}
#endif
