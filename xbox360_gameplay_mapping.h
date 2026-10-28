/* Opt-in Xbox 360-compatible DirectInput gameplay candidate for the
 * verified Holtek 045E:028E. Only A=0 and B=1 have direct runtime
 * confirmation; all other physical labels require a hardware test.
 *
 * Exact seven original 252-byte Spidey.cfg joystick DWORD offsets.
 * Back=BUTTON6 and Start=BUTTON7 are reserved for native menus/Pause.
 * Swing=BUTTON9 is a digital fallback (not a claim about LT/RT axes).
 * No stored INI/RawHex changes are permitted.
 */
#ifndef SPIDERMAN2K1FIX_XBOX360_GAMEPLAY_MAPPING_H
#define SPIDERMAN2K1FIX_XBOX360_GAMEPLAY_MAPPING_H
#include "ps1_gamepad_mapping.h"
enum { SPIDER_XBOX360_NATIVE_UNBOUND=0x4000u };
static const uint32_t spider_xbox360_buttons[SPIDER_PS1_BUTTON_COUNT]={
  0u,1u,2u,3u,4u,5u,9u
};
static int spider_xbox360_apply(uint8_t *data,size_t length){
  size_t i,j;
  if(!data || length!=SPIDER_PS1_CFG_SIZE)return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++){
    if(spider_xbox360_buttons[i]>=128u ||
       spider_xbox360_buttons[i]==6u ||
       spider_xbox360_buttons[i]==7u)return 0;
    for(j=0;j<i;j++)
      if(spider_xbox360_buttons[j]==spider_xbox360_buttons[i])return 0;
  }
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    spider_ps1_write_word(data,spider_ps1_offsets[i],spider_xbox360_buttons[i]);
  return 1;
}
static int spider_xbox360_button_free_for_pause(unsigned candidate){
  size_t i;
  if(candidate!=6u && candidate!=7u)return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    if(spider_xbox360_buttons[i]==candidate)return 0;
  return 1;
}
#endif
