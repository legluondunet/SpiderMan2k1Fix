#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_IDENTITY_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_IDENTITY_H
/* A selected SDL3 instance must have a UNIQUE VID:PID in the currently
 * opened mapped gamepads before the old DirectInput8 native owner can be
 * associated with it.  Different VID:PID gamepads may coexist.
 *
 * This pure C99 function never reads input and never changes game state.
 * A pair of identical controllers cannot be reliably distinguished via
 * native DI product VID:PID alone: reject all operations for that model. */
#include <stdint.h>
typedef struct {
  uint32_t instance_id;
  uint16_t vendor,product;
} SpiderSDL3Identity;
static inline int spider_sdl3_selected_model_unique(
    const SpiderSDL3Identity *open,unsigned count,
    uint32_t selected,uint16_t vendor,uint16_t product) {
  unsigned i,matches=0;
  int found=0;
  if(!open || !selected || !vendor || !product || count>16u)return 0;
  for(i=0;i<count;i++) {
    if(!open[i].instance_id)continue;
    if(open[i].instance_id==selected) {
      if(found || open[i].vendor!=vendor || open[i].product!=product)return 0;
      found=1;
    }
    if(open[i].vendor==vendor && open[i].product==product)
      ++matches;
  }
  return found && matches==1u;
}
#endif
