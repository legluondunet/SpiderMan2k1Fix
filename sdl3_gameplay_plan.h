/* Pure C99, read-only SDL3 3D action-state plan.
 * Stage BEFORE game input injection. No COM, keyboard, PE patches or I/O.
 * Physical SELECTED SDL3 device must match VERIFIED game-owned DirectInput.
 * Dropping foreground/3D/pause/selection resets all held action bits.
 * Rearms ONLY after a new neutral sample to avoid stale held transitions. */
#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPLAY_PLAN_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPLAY_PLAN_H
#include "sdl3_controller_profile.h"
enum { SPIDER_PLAN_REJECTED=0, SPIDER_PLAN_WAIT_NEUTRAL=1,
       SPIDER_PLAN_READY=2 };
typedef struct {
  uint32_t instance,held;
  uint16_t vid,pid;
  unsigned armed;
  /* Snapshot the active semantic profile. Identity alone is insufficient:
   * a live rebind on the same pad must require fresh neutral input. */
  SpiderSDL3Binding actions[SPIDER_SDL3_ACTION_COUNT];
} SpiderSDL3GameplayPlan;
static inline void spider_sdl3_gameplay_plan_reset(
    SpiderSDL3GameplayPlan *s) {
  if(s)memset(s,0,sizeof(*s));
}
/* The caller guarantees all fields originate on the SDL3 render thread
 * AFTER one event pump and verifies ownership against the original DI8
 * factory. 'active' means foreground 3D and not Bink/menu/paused. */
static inline int spider_sdl3_gameplay_plan_step(
    SpiderSDL3GameplayPlan *s,
    const SpiderSDL3ControllerProfile *profile,
    const SpiderSDL3ControllerSample *sample,
    uint32_t instance,uint16_t vid,uint16_t pid,
    int active,int verified_native_owner,uint32_t *pressed,
    uint32_t *released) {
  uint32_t held=0;
  unsigned i;
  int changed;
  if(pressed)*pressed=0;
  if(released)*released=0;
  if(!s || !pressed || !released || !active ||
     !verified_native_owner || !instance || !vid || !pid ||
     !profile || !sample || profile->vendor!=vid ||
     profile->product!=pid || !spider_sdl3_profile_valid(profile)) {
    spider_sdl3_gameplay_plan_reset(s);
    return SPIDER_PLAN_REJECTED;
  }
  changed=s->instance!=instance || s->vid!=vid || s->pid!=pid;
  if(!changed)
    for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++)
      if(!spider_sdl3_binding_equal(s->actions[i],profile->actions[i])) {
        changed=1;
        break;
      }
  if(changed) {
    spider_sdl3_gameplay_plan_reset(s);
    s->instance=instance;s->vid=vid;s->pid=pid;
    for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++)
      s->actions[i]=profile->actions[i];
  }
  if(!s->armed) {
    if(spider_sdl3_profile_neutral(sample))s->armed=1;
    return SPIDER_PLAN_WAIT_NEUTRAL;
  }
  if(!spider_sdl3_profile_actions(profile,sample,s->held,&held)) {
    spider_sdl3_gameplay_plan_reset(s);
    return SPIDER_PLAN_REJECTED;
  }
  *pressed=held&~s->held;
  *released=s->held&~held;
  s->held=held;
  return SPIDER_PLAN_READY;
}
#endif
