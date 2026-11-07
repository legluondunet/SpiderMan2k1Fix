#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_IDENTITY_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_IDENTITY_H
/* Existing strict single-owner policy: a selected SDL3 instance must
 * have a unique VID:PID among currently opened mapped gamepads. Different
 * models may coexist. This deliberately conservative rule is retained
 * during removal of the legacy native DirectInput rebind, so no controller
 * handoff behavior changes in the same PR.
 *
 * Pure C99 identity checks; never reads input or changes game state. */
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
