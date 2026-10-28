#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_SELECTION_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_SELECTION_H

/* Portable, driver-independent one-player selection policy.
 * SDL3 instance IDs are uint32_t, not product IDs or enumeration positions.
 * A neutral Start sample must precede any takeover. All other gamepads are
 * spectators; this policy NEVER merges competing analog axes. */
#include <stdint.h>
#include <string.h>
#define SPIDER_SDL3_MAX_GAMEPADS 16u

typedef struct {
  uint32_t id;
  unsigned start_armed;
  unsigned previous_start;
} SpiderSDL3Slot;

typedef struct {
  SpiderSDL3Slot slots[SPIDER_SDL3_MAX_GAMEPADS];
  unsigned count;
  uint32_t active;
} SpiderSDL3Selection;

/* Synchronize discovered OPENED SDL gamepads. Keep an existing selection.
 * No selection when several pads exist until a clean Start press.
 * Automatically select when exactly one valid mapped pad is available. */
static void spider_sdl3_selection_sync(SpiderSDL3Selection *s,
                                       const uint32_t *ids,unsigned count) {
  SpiderSDL3Slot next[SPIDER_SDL3_MAX_GAMEPADS]={{0}};
  unsigned i,j,n=0;
  if(!s)return;
  if(count>SPIDER_SDL3_MAX_GAMEPADS)count=SPIDER_SDL3_MAX_GAMEPADS;
  for(i=0;i<count;i++) {
    uint32_t id=ids?ids[i]:0u;
    if(!id)continue;
    for(j=0;j<n;j++)if(next[j].id==id)break;
    if(j<n)continue; /* duplicate instance, never duplicate ownership */
    next[n].id=id;
    for(j=0;j<s->count;j++)if(s->slots[j].id==id) {
      next[n]=s->slots[j];break;
    }
    n++;
  }
  memset(s->slots,0,sizeof(s->slots));
  memcpy(s->slots,next,n*sizeof(next[0]));
  s->count=n;
  if(s->active) {
    for(j=0;j<n;j++)if(s->slots[j].id==s->active)break;
    if(j==n)s->active=0;
  }
  if(!s->active && n==1u)s->active=s->slots[0].id;
}

/* Returns 1 only when an unclaimed multi-pad session is claimed by Start.
 * Ignore held Start on attach/focus return, including first neutral sample. */
static int spider_sdl3_selection_start(SpiderSDL3Selection *s,uint32_t id,
                                       int pressed) {
  unsigned i;
  if(!s || !id)return 0;
  pressed=pressed!=0;
  for(i=0;i<s->count;i++)if(s->slots[i].id==id) {
    SpiderSDL3Slot *p=&s->slots[i];
    if(!p->start_armed) {
      if(!pressed)p->start_armed=1u;
      p->previous_start=(unsigned)pressed;
      return 0;
    }
    if(!s->active && pressed && !p->previous_start) {
      s->active=id;
      p->previous_start=1u;
      return 1;
    }
    p->previous_start=(unsigned)pressed;
    return 0;
  }
  return 0;
}
#endif
