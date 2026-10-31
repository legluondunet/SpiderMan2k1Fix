/* SDL3-selected original South/Cross/A + East/Circle/B menu/Bink policy.
 * Pure C99: NO Windows, SDL, SendInput, game pointers, threads or I/O.
 *
 * The caller must enforce: exact game foreground, sole selected SDL3 owner,
 * independent original game-owned DirectInput identity, valid UI context
 * (main menu or Bink only), safe native keyboard injection and released
 * synthetic scan at the next Flip.
 *
 * Physical South (Cross/A) is fixed for Confirm/Skip and physical East
 * (Circle/B) for Back/Cancel, irrespective of old v2 custom context fields.
 * Native 3D
 * cutscenes remain entirely game-owned. Distinct intro still pages do not
 * run through this menu/video classifier: they require a verified owner.
 */
#ifndef SPIDERMAN2K1FIX_SDL3_CONTEXT_MENU_BRIDGE_H
#define SPIDERMAN2K1FIX_SDL3_CONTEXT_MENU_BRIDGE_H
#include "sdl3_controller_profile.h"

enum {
  SPIDER_SDL3_CTX_NONE=0,
  SPIDER_SDL3_CTX_MENU=1,
  SPIDER_SDL3_CTX_VIDEO=2
};
enum {
  SPIDER_SDL3_CTX_REFUSED=-1,
  SPIDER_SDL3_CTX_WAIT_NEUTRAL=0,
  SPIDER_SDL3_CTX_NO_EDGE=1,
  SPIDER_SDL3_CTX_CONFIRM_EDGE=2,
  SPIDER_SDL3_CTX_BACK_EDGE=3
};
typedef struct {
  uint32_t instance;
  uint16_t vid,pid;
  unsigned context;
  unsigned armed,confirm_held,back_held;
  SpiderSDL3Binding confirm_binding,back_binding;
} SpiderSDL3ContextMenuBridge;

static inline void spider_sdl3_context_menu_reset(
    SpiderSDL3ContextMenuBridge *state) {
  if(state)memset(state,0,sizeof(*state));
}
static inline int spider_sdl3_context_binding_down(
    SpiderSDL3Binding b,const SpiderSDL3ControllerSample *sample,
    int previous) {
  if(b.kind==SPIDER_SDL3_BIND_NONE)return 0;
  if(b.kind==SPIDER_SDL3_BIND_BUTTON)
    return (sample->buttons&(1u<<b.index))!=0;
  if(b.kind==SPIDER_SDL3_BIND_TRIGGER) {
    int v=sample->axis[b.index];
    if(v<0)return -1;
    return v>=(previous?5000:11000);
  }
  return -1;
}
/* The first neutral sample *arms* the controller but does not fire a key.
 * Any owner/context/binding change restarts this gate, so a held gamepad
 * button cannot leak from 3D -> menu or a new controller -> old movie. */
static inline int spider_sdl3_context_menu_step(
    SpiderSDL3ContextMenuBridge *state,
    const SpiderSDL3ControllerProfile *profile,
    const SpiderSDL3ControllerSample *sample,
    uint32_t instance,uint16_t vid,uint16_t pid,
    unsigned context,int verified_native_owner) {
  SpiderSDL3Binding confirm,back;
  int c,b;
  if(!state || !profile || !sample || !instance || !vid || !pid ||
     !verified_native_owner ||
     (context!=SPIDER_SDL3_CTX_MENU && context!=SPIDER_SDL3_CTX_VIDEO) ||
     profile->vendor!=vid || profile->product!=pid ||
     !spider_sdl3_profile_valid(profile)) {
    spider_sdl3_context_menu_reset(state);
    return SPIDER_SDL3_CTX_REFUSED;
  }
  /* Use the SAME physical South button as the original Jump. Never
   * replay a legacy Square/West Confirm/Skip stored in profile v2. */
  confirm=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,0u);
  /* Match the game's menu convention: East is always Back/Cancel.
   * Ignore old v2 MenuBack overrides (without modifying those files). */
  back=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,1u);
  if(instance!=state->instance || vid!=state->vid || pid!=state->pid ||
     context!=state->context ||
     !spider_sdl3_binding_equal(confirm,state->confirm_binding) ||
     !spider_sdl3_binding_equal(back,state->back_binding)) {
    spider_sdl3_context_menu_reset(state);
    state->instance=instance;state->vid=vid;state->pid=pid;
    state->context=context;
    state->confirm_binding=confirm;state->back_binding=back;
  }
  if(!state->armed) {
    if(spider_sdl3_profile_neutral(sample))state->armed=1;
    return SPIDER_SDL3_CTX_WAIT_NEUTRAL;
  }
  c=spider_sdl3_context_binding_down(confirm,sample,
                                    (int)state->confirm_held);
  b=spider_sdl3_context_binding_down(back,sample,
                                    (int)state->back_held);
  if(c<0 || b<0) {
    spider_sdl3_context_menu_reset(state);
    return SPIDER_SDL3_CTX_REFUSED;
  }
  /* Never synthesize simultaneous contradictory Return and Escape,
   * even on migrated v2 profiles where menu/video mappings differ. */
  if(c && b) {
    state->confirm_held=(unsigned)c;
    state->back_held=(unsigned)b;
    return SPIDER_SDL3_CTX_NO_EDGE;
  }
  if(b && !state->back_held) {
    state->back_held=1;state->confirm_held=0;
    return SPIDER_SDL3_CTX_BACK_EDGE;
  }
  if(c && !state->confirm_held) {
    state->confirm_held=1;state->back_held=0;
    return SPIDER_SDL3_CTX_CONFIRM_EDGE;
  }
  state->confirm_held=(unsigned)c;
  state->back_held=(unsigned)b;
  return SPIDER_SDL3_CTX_NO_EDGE;
}
#endif
