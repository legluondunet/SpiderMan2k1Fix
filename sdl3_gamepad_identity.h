#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_IDENTITY_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_IDENTITY_H
/* Keep two independent security policies:
 * - Native DirectInput owner: strict UNIQUE VID:PID (no safe DI instance
 *   match for two products claiming the same model).
 * - Universal SDL3 keyboard-only owner: exact connected SDL instance ID,
 *   same model permitted, no association with native DirectInput.
 * A per-model profile is never a per-device identity. */
#include <stdint.h>
typedef struct {
  uint32_t instance_id;
  uint16_t vendor,product;
} SpiderSDL3Identity;

/* Legacy/native DI identity rule. Never weaken for a native rebind. */
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

/* The universal keyboard bridge never binds to the original DI device.
 * It reads ONLY the selected, connected SDL3 handle, so duplicate models
 * are allowed. Fail closed on any duplicate SDL instance ID, unknown
 * identity, stale ID or different VID:PID for the claimed instance. */
static inline int spider_sdl3_selected_instance_valid(
    const SpiderSDL3Identity *open,unsigned count,
    uint32_t selected,uint16_t vendor,uint16_t product) {
  unsigned i,j,found=0;
  if(!open || !selected || !vendor || !product || count>16u)return 0;
  for(i=0;i<count;i++) {
    if(!open[i].instance_id)continue;
    for(j=i+1;j<count;j++)
      if(open[j].instance_id==open[i].instance_id)return 0;
    if(open[i].instance_id==selected) {
      if(open[i].vendor!=vendor || open[i].product!=product)return 0;
      ++found;
    }
  }
  return found==1u;
}

/* Collect every fresh Start edge in one Flip before granting keyboard
 * ownership. Two logical SDL pads reacting to one physical Start (or
 * two people pressing Start simultaneously) must not choose arbitrarily.
 * The caller still needs an exact instance validation before committing. */
typedef struct {
  uint32_t instance_id;
  uint16_t vendor,product;
  unsigned count;
} SpiderSDL3StartClaims;
static inline void spider_sdl3_start_claim_add(
    SpiderSDL3StartClaims *claims,uint32_t id,
    uint16_t vendor,uint16_t product) {
  if(!claims)return;
  if(++claims->count==1u) {
    claims->instance_id=id;
    claims->vendor=vendor;
    claims->product=product;
  } else {
    claims->instance_id=0;
    claims->vendor=0;
    claims->product=0;
  }
}
#endif
