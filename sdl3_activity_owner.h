/* Portable SDL3 activity-based single-player ownership. A pad does not
 * require Start: a fresh intentional button/trigger/left-stick action after
 * a neutral frame may claim control. Incidental analog drift cannot claim.
 * Claim requests are evaluated for ALL pads before committing; concurrent
 * activity is ambiguous. No SDL, Win32, native DI or clock dependencies. */
#ifndef SPIDERMAN2K1FIX_SDL3_ACTIVITY_OWNER_H
#define SPIDERMAN2K1FIX_SDL3_ACTIVITY_OWNER_H
#include <stdint.h>
#include <string.h>
#include "sdl3_controller_profile.h"
enum { SPIDER_ACTIVITY_MAX_PADS=16u };
typedef struct {
  uint32_t id;
  unsigned neutral_seen;
  unsigned previous_intent;
} SpiderSDL3ActivitySlot;
typedef struct {
  SpiderSDL3ActivitySlot slots[SPIDER_ACTIVITY_MAX_PADS];
  unsigned count;
} SpiderSDL3ActivityOwners;
static inline void spider_sdl3_activity_reset(SpiderSDL3ActivityOwners *state) {
  if(state)memset(state,0,sizeof(*state));
}
/* A focus transition disarms EVERY candidate, without erasing SDL IDs.
 * The next foreground neutral sample is required before a fresh press. */
static inline void spider_sdl3_activity_focus_lost(SpiderSDL3ActivityOwners *state) {
  unsigned i;
  if(!state)return;
  for(i=0;i<state->count;i++) {
    state->slots[i].neutral_seen=0u;
    state->slots[i].previous_intent=1u;
  }
}
static inline void spider_sdl3_activity_sync(
    SpiderSDL3ActivityOwners *state,const uint32_t *ids,unsigned count) {
  SpiderSDL3ActivitySlot next[SPIDER_ACTIVITY_MAX_PADS]={{0}};
  unsigned i,j,n=0;
  if(!state)return;
  if(count>SPIDER_ACTIVITY_MAX_PADS)count=SPIDER_ACTIVITY_MAX_PADS;
  for(i=0;i<count;i++) {
    uint32_t id=ids?ids[i]:0u;
    if(!id)continue;
    for(j=0;j<n;j++)if(next[j].id==id)break;
    if(j<n)continue;
    next[n].id=id;
    for(j=0;j<state->count;j++)if(state->slots[j].id==id) {
      next[n]=state->slots[j];break;
    }
    n++;
  }
  memset(state->slots,0,sizeof(state->slots));
  memcpy(state->slots,next,n*sizeof(next[0]));
  state->count=n;
}
/* Return true for any real gameplay/menu action. Ignore START and GUIDE
 * completely, and exclude the right stick (not bound to game movement).
 * Deliberate analog activation starts at 18k; SDL idle drift is far below.
 * Trigger activation matches the universal action initial 11k threshold. */
static inline int spider_sdl3_activity_intent(const SpiderSDL3ControllerSample *s) {
  uint32_t allowed;
  if(!s)return 0;
  allowed=s->buttons & ~((1u<<SPIDER_SDL3_BUTTON_START)|
                         (1u<<SPIDER_SDL3_BUTTON_GUIDE));
  if(allowed)return 1;
  if(s->axis[SPIDER_SDL3_AXIS_LT]>=11000 ||
     s->axis[SPIDER_SDL3_AXIS_RT]>=11000)return 1;
  if(s->axis[SPIDER_SDL3_AXIS_LEFTX]>=18000 ||
     s->axis[SPIDER_SDL3_AXIS_LEFTX]<=-18000 ||
     s->axis[SPIDER_SDL3_AXIS_LEFTY]>=18000 ||
     s->axis[SPIDER_SDL3_AXIS_LEFTY]<=-18000)return 1;
  return 0;
}
static inline int spider_sdl3_activity_neutral(const SpiderSDL3ControllerSample *s) {
  if(!s || !spider_sdl3_profile_neutral(s))return 0;
  return s->axis[SPIDER_SDL3_AXIS_LEFTX]>-9000 &&
         s->axis[SPIDER_SDL3_AXIS_LEFTX]<9000 &&
         s->axis[SPIDER_SDL3_AXIS_LEFTY]>-9000 &&
         s->axis[SPIDER_SDL3_AXIS_LEFTY]<9000;
}
/* One-shot edge per connected instance, no activation on inherited holds.
 * The caller MUST also validate the exact SDL instance/VID:PID, foreground
 * and whether the current owner has released its physical inputs. */
static inline int spider_sdl3_activity_edge(
    SpiderSDL3ActivityOwners *state,uint32_t id,
    const SpiderSDL3ControllerSample *sample) {
  unsigned i,intent;
  if(!state || !id || !sample)return 0;
  for(i=0;i<state->count;i++)if(state->slots[i].id==id) {
    SpiderSDL3ActivitySlot *slot=&state->slots[i];
    intent=(unsigned)spider_sdl3_activity_intent(sample);
    if(spider_sdl3_activity_neutral(sample)) {
      slot->neutral_seen=1u;
      slot->previous_intent=0u;
      return 0;
    }
    if(intent && !slot->previous_intent && slot->neutral_seen) {
      slot->previous_intent=1u;
      slot->neutral_seen=0u;
      return 1;
    }
    slot->previous_intent=intent;
    return 0;
  }
  return 0;
}
#endif
