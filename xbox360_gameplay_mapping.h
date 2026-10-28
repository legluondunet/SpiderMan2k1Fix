/* Xbox 360 layout mirrors the previously accepted Sony PS1 actions:
 *
 * Xbox A  -> Jump    (Sony Cross)
 * Xbox B  -> Kick    (Sony Circle)
 * Xbox X  -> Punch   (Sony Square)
 * Xbox Y  -> Web     (Sony Triangle / quick web)
 * Xbox LB -> Aim     (Sony L1)
 * Xbox RB -> Zipline (Sony R1)
 * Xbox RT -> Swing   (Sony R2, TARGET ONLY, NOT IMPLEMENTED HERE)
 *
 * DirectInput indices 0..5 match Microsoft's standard XUSB layout, but
 * only buttons 0 (A) and 1 (B) have actually been observed on the
 * user's Holtek 045E:028E under Wine. The separate RT trigger is often
 * represented by an axis, not a button. Until a physical axis trace
 * proves its location, BUTTON9 (right-stick click on standard XUSB)
 * REMAINS A TEMPORARY DIGITAL SWING FALLBACK, NOT AN RT mapping.
 * The exact seven joystick DWORD offsets are read/write overlay only;
 * saved INI and RawHex are never modified by this preset.
 * Back=BUTTON6 and Start=BUTTON7 remain reserved for Pause.
 */
#ifndef SPIDERMAN2K1FIX_XBOX360_GAMEPLAY_MAPPING_H
#define SPIDERMAN2K1FIX_XBOX360_GAMEPLAY_MAPPING_H
#include "ps1_gamepad_mapping.h"
enum { SPIDER_XBOX360_NATIVE_UNBOUND=0x4000u };
static const uint32_t spider_xbox360_buttons[SPIDER_PS1_BUTTON_COUNT]={
  0u,1u,2u,3u,4u,5u,9u
};
static inline int spider_xbox360_apply(uint8_t *data,size_t length){
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
static inline int spider_xbox360_button_free_for_pause(unsigned candidate){
  size_t i;
  if(candidate!=6u && candidate!=7u)return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    if(spider_xbox360_buttons[i]==candidate)return 0;
  return 1;
}
#endif
