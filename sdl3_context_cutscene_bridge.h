/* Selected SDL3 profile -> native scripted 3D cutscene skip edge.
 * Pure C99 policy only: no input injection, SDL calls or native addresses.
 * The caller MUST prove the game's own CutSceneSkipAllow and native owner.
 * Keep the original Jump and seven gameplay actions completely unchanged.
 */
#ifndef SPIDERMAN2K1FIX_SDL3_CONTEXT_CUTSCENE_BRIDGE_H
#define SPIDERMAN2K1FIX_SDL3_CONTEXT_CUTSCENE_BRIDGE_H
#include "sdl3_context_menu_bridge.h"

enum {
  SPIDER_SDL3_CUTSCENE_REFUSED=-1,
  SPIDER_SDL3_CUTSCENE_WAIT=0,
  SPIDER_SDL3_CUTSCENE_NO_EDGE=1,
  SPIDER_SDL3_CUTSCENE_PRESS=2
};
typedef struct {
  uint32_t instance;
  uint16_t vid,pid;
  SpiderSDL3Binding binding;
  unsigned armed,held;
} SpiderSDL3ContextCutsceneBridge;

static inline void spider_sdl3_context_cutscene_reset(
    SpiderSDL3ContextCutsceneBridge *s) {
  if(s)memset(s,0,sizeof(*s));
}
static inline int spider_sdl3_context_cutscene_step(
    SpiderSDL3ContextCutsceneBridge *s,
    const SpiderSDL3ControllerProfile *profile,
    const SpiderSDL3ControllerSample *sample,
    uint32_t instance,uint16_t vid,uint16_t pid,
    int verified_native_owner,int native_script_skip_allowed) {
  SpiderSDL3Binding bind;
  int held;
  if(!s || !profile || !sample || !instance || !vid || !pid ||
     !verified_native_owner || !native_script_skip_allowed ||
     profile->vendor!=vid || profile->product!=pid ||
     !spider_sdl3_profile_valid(profile)) {
    spider_sdl3_context_cutscene_reset(s);
    return SPIDER_SDL3_CUTSCENE_REFUSED;
  }
  bind=profile->context[SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE];
  if(bind.kind==SPIDER_SDL3_BIND_NONE) {
    spider_sdl3_context_cutscene_reset(s);
    return SPIDER_SDL3_CUTSCENE_REFUSED;
  }
  if(s->instance!=instance || s->vid!=vid || s->pid!=pid ||
     !spider_sdl3_binding_equal(s->binding,bind)) {
    spider_sdl3_context_cutscene_reset(s);
    s->instance=instance;s->vid=vid;s->pid=pid;
    s->binding=bind;
  }
  /* Do not inherit a pressed button across Bink/3D, focus or hot-swap. */
  if(!s->armed) {
    if(spider_sdl3_profile_neutral(sample))s->armed=1u;
    return SPIDER_SDL3_CUTSCENE_WAIT;
  }
  held=spider_sdl3_context_binding_down(bind,sample,(int)s->held);
  if(held<0) {
    spider_sdl3_context_cutscene_reset(s);
    return SPIDER_SDL3_CUTSCENE_REFUSED;
  }
  if(held && !s->held) {
    s->held=1u;
    return SPIDER_SDL3_CUTSCENE_PRESS;
  }
  s->held=(unsigned)held;
  return SPIDER_SDL3_CUTSCENE_NO_EDGE;
}
#endif
