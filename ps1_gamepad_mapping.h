/* Exact seven verified DWORD joystick bindings in virtual Spidey.cfg.
 * C99 header deliberately independent from Win32 so it can be tested on Linux.
 *
 * EXPERIMENTAL: physical Sony DS4 indices, not PS1 game-input meanings.
 * Only mutate the 252-byte in-memory image AFTER verified hardware
 * discovery. Always snapshot/restore previous user bindings on disk.
 */
#ifndef SPIDER_PS1_GAMEPAD_MAPPING_H
#define SPIDER_PS1_GAMEPAD_MAPPING_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
enum { SPIDER_PS1_CFG_SIZE=252, SPIDER_PS1_BUTTON_COUNT=7 };
static const size_t spider_ps1_offsets[SPIDER_PS1_BUTTON_COUNT]={
  0x50u,0x54u,0x58u,0x5Cu,0x60u,0x64u,0x68u
};
/* Jump Cross, Kick Circle, Punch Square, Web Triangle, Aim L1,
 * Zipline R1, Swing R2; indices 1,2,0,3,4,5,7 are zero-based. */
static const uint32_t spider_ps1_buttons[SPIDER_PS1_BUTTON_COUNT]={
  1u,2u,0u,3u,4u,5u,7u
};
static inline uint32_t spider_ps1_read_word(const uint8_t *data,size_t pos){
  return (uint32_t)data[pos]|((uint32_t)data[pos+1]<<8)|
         ((uint32_t)data[pos+2]<<16)|((uint32_t)data[pos+3]<<24);
}
static inline void spider_ps1_write_word(uint8_t *data,size_t pos,uint32_t n){
  data[pos]=(uint8_t)n;
  data[pos+1]=(uint8_t)(n>>8);
  data[pos+2]=(uint8_t)(n>>16);
  data[pos+3]=(uint8_t)(n>>24);
}
static inline int spider_ps1_snapshot(const uint8_t *data,size_t length,
                               uint32_t stored[SPIDER_PS1_BUTTON_COUNT]){
  size_t i;
  if(!data||!stored||length!=SPIDER_PS1_CFG_SIZE)return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    stored[i]=spider_ps1_read_word(data,spider_ps1_offsets[i]);
  return 1;
}
static inline int spider_ps1_apply(uint8_t *data,size_t length){
  size_t i;
  if(!data||length!=SPIDER_PS1_CFG_SIZE)return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    spider_ps1_write_word(data,spider_ps1_offsets[i],spider_ps1_buttons[i]);
  return 1;
}
static inline int spider_ps1_restore(uint8_t *data,size_t length,
                              const uint32_t stored[SPIDER_PS1_BUTTON_COUNT]){
  size_t i;
  if(!data||!stored||length!=SPIDER_PS1_CFG_SIZE)return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    spider_ps1_write_word(data,spider_ps1_offsets[i],stored[i]);
  return 1;
}
#endif
