#ifndef SPIDERMAN2K1FIX_SDL3_GENERIC_CANDIDATE_H
#define SPIDERMAN2K1FIX_SDL3_GENERIC_CANDIDATE_H

/*
 * PHASE 0 RESEARCH: portable, read-only SDL3 candidate selection.
 *
 * This is deliberately NOT an owner/reconnect interface. SDL3 can
 * recognize an unknown VID/PID or a DS3 without the original 2001
 * engine being able to consume its DirectInput buttons. This policy
 * tracks only SDL3 INSTANCE IDs and fresh Start edges, and therefore
 * cannot authorize native game input/COM writes on its own.
 *
 * Sample all opened devices once per frame, without merging their
 * analog axes. Exactly one neutral-armed Start edge may change the
 * candidate. Competing simultaneous edges refuse to change it.
 */
#include <stdint.h>
#include "sdl3_gamepad_selection.h"

typedef struct {
  SpiderSDL3Selection selection;
  uint32_t candidate_id;
  uint32_t claim_id;
  unsigned claims;
  unsigned frame_open;
} SpiderSDL3GenericCandidate;

/* Reconcile only opened SDL3 instance IDs; never treat VID/PID as
 * an owner identifier. No auto-claim even when only one pad remains.
 * Hot-unplug revokes a missing candidate before the next sample. */
static inline void spider_sdl3_generic_discover(
    SpiderSDL3GenericCandidate *p,const uint32_t *ids,unsigned count) {
  unsigned i,present=0;
  if(!p)return;
  spider_sdl3_selection_sync(&p->selection,ids,count);
  for(i=0;i<p->selection.count;i++)
    if(p->selection.slots[i].id==p->candidate_id)present=1;
  if(!present)p->candidate_id=0u;
  p->selection.active=p->candidate_id;
  p->claim_id=0u;
  p->claims=0u;
  p->frame_open=0u;
}

/* An Alt+Tab cannot create a Start edge. The user must release Start
 * once on every pad before a subsequent fresh press. The last candidate
 * remains a *logical* selection, never a native ownership assertion. */
static inline void spider_sdl3_generic_focus_lost(
    SpiderSDL3GenericCandidate *p) {
  if(!p)return;
  spider_sdl3_selection_focus_lost(&p->selection);
  p->claim_id=0u;
  p->claims=0u;
  p->frame_open=0u;
}

static inline void spider_sdl3_generic_begin(
    SpiderSDL3GenericCandidate *p) {
  if(!p)return;
  p->frame_open=1u;
  p->claim_id=0u;
  p->claims=0u;
}

static inline void spider_sdl3_generic_start(
    SpiderSDL3GenericCandidate *p,uint32_t instance,int pressed) {
  if(!p || !p->frame_open || !instance)return;
  /* The existing selection policy ignores unknown instances and
   * held Start. Each new edge updates its own armed/previous state. */
  if(spider_sdl3_selection_start_edge(&p->selection,instance,pressed)) {
    p->claim_id=instance;
    p->claims++;
  }
}

/* Returns 1 if a SINGLE fresh Start edge changed the candidate.
 * A rejected or already-selected event is not a native-game handoff.
 * Callers may inspect candidate_id, but MUST separately authenticate
 * any proposed game-native bridge. */
static inline int spider_sdl3_generic_end(
    SpiderSDL3GenericCandidate *p) {
  uint32_t before;
  unsigned n;
  if(!p || !p->frame_open)return 0;
  before=p->candidate_id;
  n=p->claims;
  if(n==1u)p->candidate_id=p->claim_id;
  p->selection.active=p->candidate_id;
  p->claim_id=0u;
  p->claims=0u;
  p->frame_open=0u;
  return n==1u && p->candidate_id!=before;
}
#endif
