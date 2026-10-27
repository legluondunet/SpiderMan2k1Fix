#ifndef SPIDERMAN2K1FIX_GAMEPAD_START_H
#define SPIDERMAN2K1FIX_GAMEPAD_START_H

#include <stdint.h>
#include <stddef.h>

/*
 * The 2001 PC game's native controller mappings are 14 (action,button)
 * uint32 pairs. The 0x1000 Start action is NOT among the seven exposed
 * gameplay binds in the virtual Spidey.cfg and factory maps to button 11.
 * These structural checks are pure, source-compatible C11 helpers.
 *
 * They do NOT establish the DS4 Options/Share physical button numbering;
 * users must first identify the desired zero-based DirectInput index.
 */
#define SPIDER_START_MAP_COUNT 14u
#define SPIDER_START_MAP_BYTES (SPIDER_START_MAP_COUNT * 8u)
#define SPIDER_START_MAP_BUTTON_OFFSET (11u * 8u + 4u)
#define SPIDER_START_ACTION 0x1000u
#define SPIDER_START_FACTORY_BUTTON 11u
#define SPIDER_START_UNUSED_BUTTON 0x4000u

static inline uint32_t SpiderGamepad_LoadLE32(const unsigned char *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
         ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline int SpiderGamepad_MappingRecognized(const unsigned char *p, size_t len) {
  static const uint32_t actions[SPIDER_START_MAP_COUNT]={
    0x1u,0x2u,0x4u,0x8u,0x10u,0x20u,0x40u,0x80u,
    0x100u,0x200u,0x400u,0x1000u,0x2000u,0x8000u};
  unsigned i;
  if(!p || len<SPIDER_START_MAP_BYTES)return 0;
  for(i=0;i<SPIDER_START_MAP_COUNT;i++) {
    uint32_t btn=SpiderGamepad_LoadLE32(p+i*8u+4u);
    if(SpiderGamepad_LoadLE32(p+i*8u)!=actions[i])return 0;
    if(i<4u || i==12u) {
      if(btn!=SPIDER_START_UNUSED_BUTTON)return 0;
    } else if(i==11u) {
      if(btn!=SPIDER_START_FACTORY_BUTTON)return 0;
    } else if(i==13u) {
      if(btn!=0u)return 0;
    } else if(btn>31u && btn!=SPIDER_START_UNUSED_BUTTON)return 0;
  }
  return 1;
}
/* The remapped physical button must NOT also fire a gameplay action. */
static inline int SpiderGamepad_StartButtonAvailable(
    const unsigned char *p, size_t len, uint32_t button) {
  unsigned i;
  if(button>31u || !SpiderGamepad_MappingRecognized(p,len))return 0;
  for(i=4u;i<=10u;i++)
    if(SpiderGamepad_LoadLE32(p+i*8u+4u)==button)return 0;
  return 1;
}
#endif
