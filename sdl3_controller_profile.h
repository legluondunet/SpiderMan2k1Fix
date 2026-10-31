/* Portable SDL3 controller profile policy for Spider-Man PC (2001).
 * This is ONLY a semantic mapping model. It does NOT inject input,
 * change DirectInput state, enable the native joystick menu, or write INI.
 * SDL3 button values follow SDL_GamepadButton, axes SDL_GamepadAxis.
 *
 * START (6) is reserved for hot-switch ownership. GUIDE (5) is reserved
 * for the OS. LEFTX/LEFTY/RIGHTX/RIGHTY axes are analog movement, never
 * digital triggers in this first version. LT and RT are independent.
 *
 * Model-level VID/PID identity is deliberate. Identical products cannot
 * yet have distinct settings; a future GUID/serial discriminator must be
 * validated before supporting that feature. Not for live gameplay until
 * the SDL3 action -> original game input bridge is implemented and tested.
 */
#ifndef SPIDERMAN2K1FIX_SDL3_CONTROLLER_PROFILE_H
#define SPIDERMAN2K1FIX_SDL3_CONTROLLER_PROFILE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum {
  SPIDER_SDL3_ACTION_JUMP=0,
  SPIDER_SDL3_ACTION_KICK,
  SPIDER_SDL3_ACTION_PUNCH,
  SPIDER_SDL3_ACTION_WEB,
  SPIDER_SDL3_ACTION_AIM,
  SPIDER_SDL3_ACTION_ZIPLINE,
  SPIDER_SDL3_ACTION_SWING,
  SPIDER_SDL3_ACTION_PAUSE,
  SPIDER_SDL3_ACTION_COUNT=8,
  /* Independent input contexts: duplicate physical buttons are allowed
   * across 3D, menu, movie, still and scripted cutscene contexts. */
  SPIDER_SDL3_CONTEXT_CONFIRM=0,
  SPIDER_SDL3_CONTEXT_BACK,
  SPIDER_SDL3_CONTEXT_SKIP_VIDEO,
  SPIDER_SDL3_CONTEXT_NEXT_STILL,
  SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE,
  SPIDER_SDL3_CONTEXT_COUNT=5,
  SPIDER_SDL3_SETUP_ACTION_COUNT=SPIDER_SDL3_ACTION_COUNT+SPIDER_SDL3_CONTEXT_COUNT,
  SPIDER_SDL3_VISIBLE_ACTION_COUNT=SPIDER_SDL3_ACTION_COUNT,
  SPIDER_SDL3_PROFILE_BANK_CAPACITY=16,
  SPIDER_SDL3_BUTTON_COUNT=21,
  SPIDER_SDL3_AXIS_COUNT=6,
  SPIDER_SDL3_BUTTON_START=6,
  SPIDER_SDL3_BUTTON_GUIDE=5,
  SPIDER_SDL3_AXIS_LT=4,
  SPIDER_SDL3_AXIS_RT=5
};
typedef enum {
  SPIDER_SDL3_BIND_NONE=0,
  SPIDER_SDL3_BIND_BUTTON=1,
  SPIDER_SDL3_BIND_TRIGGER=2
} SpiderSDL3BindKind;
typedef struct {
  uint8_t kind;
  uint8_t index;
} SpiderSDL3Binding;
typedef struct {
  uint16_t vendor;
  uint16_t product;
  SpiderSDL3Binding actions[SPIDER_SDL3_ACTION_COUNT];
  SpiderSDL3Binding context[SPIDER_SDL3_CONTEXT_COUNT];
} SpiderSDL3ControllerProfile;
typedef struct {
  uint32_t buttons; /* SDL3 physical normalized button mask, not DInput */
  int16_t axis[SPIDER_SDL3_AXIS_COUNT]; /* SDL3 trigger: 0..32767 */
} SpiderSDL3ControllerSample;
typedef struct {
  SpiderSDL3ControllerProfile slots[SPIDER_SDL3_PROFILE_BANK_CAPACITY];
  unsigned used;
} SpiderSDL3ProfileBank;
static const char *const spider_sdl3_action_names[SPIDER_SDL3_ACTION_COUNT]={
  "Jump","Kick","Punch","Web","Aim","Zipline","Swing","Pause"
};
/* INI keys are ASCII, stable across translations and format versions.
 * Each applies only to its own original-game input context. */
static const char *const spider_sdl3_context_names[SPIDER_SDL3_CONTEXT_COUNT]={
  "MenuConfirm","MenuBack","SkipVideo","NextStill","SkipCutscene"
};
static inline SpiderSDL3Binding spider_sdl3_bind(unsigned kind,unsigned index) {
  SpiderSDL3Binding result;
  result.kind=(uint8_t)kind;
  result.index=(uint8_t)index;
  return result;
}
static inline int spider_sdl3_binding_valid(SpiderSDL3Binding b) {
  if(b.kind==SPIDER_SDL3_BIND_NONE)return b.index==0;
  if(b.kind==SPIDER_SDL3_BIND_BUTTON)
    return b.index<SPIDER_SDL3_BUTTON_COUNT &&
           b.index!=SPIDER_SDL3_BUTTON_START &&
           b.index!=SPIDER_SDL3_BUTTON_GUIDE;
  if(b.kind==SPIDER_SDL3_BIND_TRIGGER)
    return b.index==SPIDER_SDL3_AXIS_LT ||
           b.index==SPIDER_SDL3_AXIS_RT;
  return 0;
}
static inline int spider_sdl3_binding_equal(SpiderSDL3Binding a,
                                            SpiderSDL3Binding b) {
  return a.kind==b.kind && a.index==b.index;
}
static inline int spider_sdl3_profile_valid(
    const SpiderSDL3ControllerProfile *p) {
  unsigned i,j;
  if(!p || !p->vendor || !p->product)return 0;
  for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++) {
    SpiderSDL3Binding a=p->actions[i];
    if(!spider_sdl3_binding_valid(a))return 0;
    /* Seven core gameplay actions must be set. PAUSE may be unbound. */
    if(i!=SPIDER_SDL3_ACTION_PAUSE &&
       a.kind==SPIDER_SDL3_BIND_NONE)return 0;
    if(a.kind!=SPIDER_SDL3_BIND_NONE)
      for(j=0;j<i;j++)
        if(spider_sdl3_binding_equal(a,p->actions[j]))return 0;
  }
  for(i=0;i<SPIDER_SDL3_CONTEXT_COUNT;i++)
    if(!spider_sdl3_binding_valid(p->context[i]))return 0;
  /* Confirm and Back share the same 2D menu context: separate keys.
   * Other bindings are context-specific and MAY reuse any gameplay key. */
  if(p->context[SPIDER_SDL3_CONTEXT_CONFIRM].kind!=SPIDER_SDL3_BIND_NONE &&
     spider_sdl3_binding_equal(p->context[SPIDER_SDL3_CONTEXT_CONFIRM],
                              p->context[SPIDER_SDL3_CONTEXT_BACK]))return 0;
  return 1;
}
static inline int spider_sdl3_profile_default(
    SpiderSDL3ControllerProfile *p,uint16_t vendor,uint16_t product) {
  if(!p || !vendor || !product)return 0;
  memset(p,0,sizeof(*p));
  p->vendor=vendor;p->product=product;
  /* Standard SDL semantic controls; same action meaning for DS4/Xbox. */
  p->actions[SPIDER_SDL3_ACTION_JUMP]=spider_sdl3_bind(1,0); /* south */
  p->actions[SPIDER_SDL3_ACTION_KICK]=spider_sdl3_bind(1,1); /* east */
  p->actions[SPIDER_SDL3_ACTION_PUNCH]=spider_sdl3_bind(1,2); /* west */
  p->actions[SPIDER_SDL3_ACTION_WEB]=spider_sdl3_bind(1,3); /* north */
  p->actions[SPIDER_SDL3_ACTION_AIM]=spider_sdl3_bind(1,9); /* LB */
  p->actions[SPIDER_SDL3_ACTION_ZIPLINE]=spider_sdl3_bind(1,10); /* RB */
  p->actions[SPIDER_SDL3_ACTION_SWING]=
    spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_RT);
  p->actions[SPIDER_SDL3_ACTION_PAUSE]=spider_sdl3_bind(1,4); /* back */
  /* Movie and second still both currently accept keyboard Enter. Use
   * South as the accessible default, as with menu confirmation.
   * Do not alter the original native skip/timing gates. */
  p->context[SPIDER_SDL3_CONTEXT_CONFIRM]=spider_sdl3_bind(1,0);
  p->context[SPIDER_SDL3_CONTEXT_BACK]=spider_sdl3_bind(1,1);
  p->context[SPIDER_SDL3_CONTEXT_SKIP_VIDEO]=spider_sdl3_bind(1,0);
  p->context[SPIDER_SDL3_CONTEXT_NEXT_STILL]=spider_sdl3_bind(1,0);
  p->context[SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE]=spider_sdl3_bind(1,0);
  return spider_sdl3_profile_valid(p);
}
/* Atomic change for a future in-game wizard. If the new input is already
 * assigned, SWAP the two bindings (except when unbinding optional Pause).
 * Never silently create duplicate actions or mutate on invalid changes. */
static inline int spider_sdl3_profile_assign(
    SpiderSDL3ControllerProfile *p,unsigned action,SpiderSDL3Binding binding) {
  SpiderSDL3ControllerProfile trial;
  unsigned i;
  if(!p || action>=SPIDER_SDL3_ACTION_COUNT ||
     !spider_sdl3_profile_valid(p) ||
     !spider_sdl3_binding_valid(binding) ||
     (binding.kind==SPIDER_SDL3_BIND_NONE &&
      action!=SPIDER_SDL3_ACTION_PAUSE))return 0;
  trial=*p;
  for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++) {
    if(i!=action && binding.kind!=SPIDER_SDL3_BIND_NONE &&
       spider_sdl3_binding_equal(trial.actions[i],binding)) {
      trial.actions[i]=trial.actions[action];
      break;
    }
  }
  trial.actions[action]=binding;
  if(!spider_sdl3_profile_valid(&trial))return 0;
  *p=trial;
  return 1;
}
/* Change exactly one context binding; do not modify 3D bindings.
 * Confirm and Back may swap, because they are active in the same context.
 * Video/still/3D-cutscene are mutually exclusive and may share buttons. */
static inline int spider_sdl3_profile_assign_context(
    SpiderSDL3ControllerProfile *p,unsigned action,SpiderSDL3Binding binding) {
  SpiderSDL3ControllerProfile trial;
  if(!p || action>=SPIDER_SDL3_CONTEXT_COUNT ||
     !spider_sdl3_profile_valid(p) || !spider_sdl3_binding_valid(binding))
    return 0;
  trial=*p;
  if(action==SPIDER_SDL3_CONTEXT_CONFIRM ||
     action==SPIDER_SDL3_CONTEXT_BACK) {
    unsigned other=action==SPIDER_SDL3_CONTEXT_CONFIRM?
      SPIDER_SDL3_CONTEXT_BACK:SPIDER_SDL3_CONTEXT_CONFIRM;
    if(binding.kind!=SPIDER_SDL3_BIND_NONE &&
       spider_sdl3_binding_equal(binding,trial.context[other]))
      trial.context[other]=trial.context[action];
  }
  trial.context[action]=binding;
  if(!spider_sdl3_profile_valid(&trial))return 0;
  *p=trial;
  return 1;
}
static inline int spider_sdl3_profile_assign_setup(
    SpiderSDL3ControllerProfile *p,unsigned action,SpiderSDL3Binding binding) {
  if(action<SPIDER_SDL3_ACTION_COUNT)
    return spider_sdl3_profile_assign(p,action,binding);
  if(action>=SPIDER_SDL3_SETUP_ACTION_COUNT)return 0;
  return spider_sdl3_profile_assign_context(p,
    action-SPIDER_SDL3_ACTION_COUNT,binding);
}
static inline SpiderSDL3Binding spider_sdl3_profile_setup_binding(
    const SpiderSDL3ControllerProfile *p,unsigned action) {
  if(!p)return spider_sdl3_bind(SPIDER_SDL3_BIND_NONE,0);
  if(action<SPIDER_SDL3_ACTION_COUNT)return p->actions[action];
  if(action<SPIDER_SDL3_SETUP_ACTION_COUNT)
    return p->context[action-SPIDER_SDL3_ACTION_COUNT];
  return spider_sdl3_bind(SPIDER_SDL3_BIND_NONE,0);
}
/* Legacy v2 context fields are retained for lossless profile parsing.
 * Menu/movie Confirm/Skip is fixed to South (Cross/A) and Back/Cancel
 * to East (Circle/B). Scripted 3D scenes remain game-native. All five
 * stored context bindings are ignored at runtime, without rewriting
 * any existing per-model file. */
static inline int spider_sdl3_profile_shared_confirm_binding(
    const SpiderSDL3ControllerProfile *p,SpiderSDL3Binding *binding) {
  static const unsigned keys[4]={
    SPIDER_SDL3_CONTEXT_CONFIRM,SPIDER_SDL3_CONTEXT_SKIP_VIDEO,
    SPIDER_SDL3_CONTEXT_NEXT_STILL,SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE
  };
  unsigned i;
  if(!p || !binding || !spider_sdl3_profile_valid(p))return 0;
  *binding=p->context[keys[0]];
  for(i=1;i<4u;i++)
    if(!spider_sdl3_binding_equal(*binding,p->context[keys[i]]))
      return 0;
  return 1;
}
static inline int spider_sdl3_profile_assign_confirm_skip(
    SpiderSDL3ControllerProfile *p,SpiderSDL3Binding binding) {
  SpiderSDL3ControllerProfile trial;
  static const unsigned keys[4]={
    SPIDER_SDL3_CONTEXT_CONFIRM,SPIDER_SDL3_CONTEXT_SKIP_VIDEO,
    SPIDER_SDL3_CONTEXT_NEXT_STILL,SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE
  };
  unsigned i;
  if(!p || !spider_sdl3_profile_valid(p) ||
     !spider_sdl3_binding_valid(binding))return 0;
  trial=*p;
  /* Confirm and Back share menus: swap their physical buttons. */
  if(binding.kind!=SPIDER_SDL3_BIND_NONE &&
     spider_sdl3_binding_equal(binding,trial.context[SPIDER_SDL3_CONTEXT_BACK]))
    trial.context[SPIDER_SDL3_CONTEXT_BACK]=
      trial.context[SPIDER_SDL3_CONTEXT_CONFIRM];
  for(i=0;i<4u;i++)trial.context[keys[i]]=binding;
  if(!spider_sdl3_profile_valid(&trial))return 0;
  *p=trial;
  return 1;
}
static inline int spider_sdl3_profile_assign_back(
    SpiderSDL3ControllerProfile *p,SpiderSDL3Binding binding) {
  SpiderSDL3ControllerProfile trial;
  SpiderSDL3Binding confirm,old_back;
  int unified;
  if(!p || !spider_sdl3_profile_valid(p) ||
     !spider_sdl3_binding_valid(binding))return 0;
  trial=*p;
  old_back=trial.context[SPIDER_SDL3_CONTEXT_BACK];
  unified=spider_sdl3_profile_shared_confirm_binding(&trial,&confirm);
  if(binding.kind!=SPIDER_SDL3_BIND_NONE &&
     spider_sdl3_binding_equal(binding,
                              trial.context[SPIDER_SDL3_CONTEXT_CONFIRM])) {
    /* If the group was already unified, preserve that invariant and
     * swap the whole group. For legacy mixed keys, swap menu Confirm only:
     * keep all other saved contexts exactly as the user configured. */
    if(unified) {
      trial.context[SPIDER_SDL3_CONTEXT_CONFIRM]=old_back;
      trial.context[SPIDER_SDL3_CONTEXT_SKIP_VIDEO]=old_back;
      trial.context[SPIDER_SDL3_CONTEXT_NEXT_STILL]=old_back;
      trial.context[SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE]=old_back;
    } else {
      trial.context[SPIDER_SDL3_CONTEXT_CONFIRM]=old_back;
    }
  }
  trial.context[SPIDER_SDL3_CONTEXT_BACK]=binding;
  if(!spider_sdl3_profile_valid(&trial))return 0;
  *p=trial;
  return 1;
}
static inline int spider_sdl3_profile_assign_visible(
    SpiderSDL3ControllerProfile *p,unsigned action,
    SpiderSDL3Binding binding) {
  if(action<SPIDER_SDL3_ACTION_COUNT)
    return spider_sdl3_profile_assign(p,action,binding);
  /* Confirm/Skip and Back/Cancel are fixed UI conventions, not
   * editable actions. 3D semantic bindings remain independent. */
  return 0;
}
static inline void spider_sdl3_bank_reset(SpiderSDL3ProfileBank *bank) {
  if(bank)memset(bank,0,sizeof(*bank));
}
static inline SpiderSDL3ControllerProfile *spider_sdl3_bank_get(
    SpiderSDL3ProfileBank *bank,uint16_t vendor,uint16_t product) {
  unsigned i;
  if(!bank || !vendor || !product ||
     bank->used>SPIDER_SDL3_PROFILE_BANK_CAPACITY)return NULL;
  for(i=0;i<bank->used;i++)
    if(bank->slots[i].vendor==vendor &&
       bank->slots[i].product==product)return &bank->slots[i];
  if(bank->used==SPIDER_SDL3_PROFILE_BANK_CAPACITY)return NULL;
  if(!spider_sdl3_profile_default(&bank->slots[bank->used],
                                 vendor,product))return NULL;
  return &bank->slots[bank->used++];
}
/* Read-only semantic evaluation of a single selected physical pad.
 * previous is the previous evaluated action MASK, not raw SDL buttons.
 * Trigger 11k press / 5k release; same thresholds as RT compatibility
 * bridge but NO keyboard synthesis and NO native game mapping writes.
 * Caller MUST independently verify selection, foreground and freshness.
 * On selection/focus loss, caller discards previous mask and rearms only
 * after a neutral physical sample to avoid inheriting held inputs. */
static inline int spider_sdl3_profile_actions(
    const SpiderSDL3ControllerProfile *p,
    const SpiderSDL3ControllerSample *sample,
    uint32_t previous,uint32_t *output) {
  unsigned i;
  uint32_t held=0;
  if(output)*output=0;
  if(!output || !sample || !spider_sdl3_profile_valid(p))return 0;
  for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++) {
    SpiderSDL3Binding b=p->actions[i];
    int on=0;
    if(b.kind==SPIDER_SDL3_BIND_BUTTON)
      on=((sample->buttons>>b.index)&1u)!=0;
    else if(b.kind==SPIDER_SDL3_BIND_TRIGGER) {
      int v=sample->axis[b.index];
      if(v<0)return 0; /* inconsistent normalized SDL sample */
      on=v>=(((previous>>i)&1u)?5000:11000);
    }
    if(on)held|=1u<<i;
  }
  *output=held;
  return 1;
}
static inline int spider_sdl3_profile_neutral(
    const SpiderSDL3ControllerSample *sample) {
  if(!sample)return 0;
  /* Require every SDL button including START released, both triggers up.
   * Stick axes remain irrelevant: movement drift must not block ownership. */
  return sample->buttons==0 &&
         sample->axis[SPIDER_SDL3_AXIS_LT]>=0 &&
         sample->axis[SPIDER_SDL3_AXIS_LT]<5000 &&
         sample->axis[SPIDER_SDL3_AXIS_RT]>=0 &&
         sample->axis[SPIDER_SDL3_AXIS_RT]<5000;
}
#endif
