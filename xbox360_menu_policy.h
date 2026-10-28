/* Experimental Xbox-360-compatible DirectInput menu mapping.
 *
 * With the 2001 retail game, analog/POV movement is already native.
 * A and B are commonly DInput rgbButtons[0]/[1]. Unlike XInput this
 * ordering is not guaranteed for Holtek/third-party adapters. Emit a
 * bounded physical-button trace so an actual 045E:028E test can confirm
 * the map before it is promoted from experimental status.
 *
 * Portable C99; no Win32 dependencies or game memory writes.
 */
#ifndef SPIDERMAN2K1FIX_XBOX360_MENU_POLICY_H
#define SPIDERMAN2K1FIX_XBOX360_MENU_POLICY_H
#include <stdint.h>
#define SPIDER_XBOX360_A_DI_BUTTON 0u
#define SPIDER_XBOX360_B_DI_BUTTON 1u
#define SPIDER_XBOX360_BUTTONS_TO_TRACE 10u
#define SPIDER_XBOX360_ENTER 1u
#define SPIDER_XBOX360_BACK 2u

static inline uint32_t spider_xbox360_held_buttons(const unsigned char *buttons) {
  unsigned i;
  uint32_t held=0;
  if(!buttons)return 0;
  for(i=0;i<SPIDER_XBOX360_BUTTONS_TO_TRACE;i++)
    if(buttons[i]&0x80u)held|=(uint32_t)1u<<i;
  return held;
}
static inline unsigned spider_xbox360_menu_edges(uint32_t held,uint32_t previous) {
  const uint32_t fresh=held&~previous;
  if(fresh&((uint32_t)1u<<SPIDER_XBOX360_B_DI_BUTTON))
    return SPIDER_XBOX360_BACK;  /* Esc has priority if A+B pressed together */
  if(fresh&((uint32_t)1u<<SPIDER_XBOX360_A_DI_BUTTON))
    return SPIDER_XBOX360_ENTER;
  return 0;
}
#endif
