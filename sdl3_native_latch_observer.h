#ifndef SPIDERMAN2K1FIX_SDL3_NATIVE_LATCH_OBSERVER_H
#define SPIDERMAN2K1FIX_SDL3_NATIVE_LATCH_OBSERVER_H
/* Research-only read-only decoder of ORIGINAL 20 action-latch records.
 * Snapshot is supplied by the caller: this header has no global addresses,
 * Windows APIs, DirectInput calls, input injection or game writes.
 */
#include <stdint.h>
#include <string.h>
#include "sdl3_original_action_layout.h"
typedef struct {
  uint32_t held_slots,event_slots,early_slots,held_actions;
} SpiderNativeLatchSnapshot;
typedef struct {
  SpiderNativeLatchSnapshot previous;
  unsigned context;
  unsigned have_previous;
  unsigned emitted;
} SpiderNativeLatchObserver;
static inline void spider_native_latch_observer_reset(
    SpiderNativeLatchObserver *o) {
  if(o)memset(o,0,sizeof(*o));
}
static inline void spider_native_latch_decode(
    const uint8_t records[SPIDER_ORIGINAL_INPUT_STATE_COUNT][16],
    SpiderNativeLatchSnapshot *out) {
  unsigned i;
  if(!out)return;
  memset(out,0,sizeof(*out));
  if(!records)return;
  for(i=0;i<SPIDER_ORIGINAL_INPUT_STATE_COUNT;i++) {
    if(records[i][0]) {
      out->held_slots|=1u<<i;
      out->held_actions|=spider_original_action_latch_mask(i);
    }
    /* +1 is persistent, NOT a one-frame press edge. */
    if(records[i][1])out->event_slots|=1u<<i;
    if(records[i][2])out->early_slots|=1u<<i;
  }
}
static inline int spider_native_latch_observer_changed(
    SpiderNativeLatchObserver *o,const SpiderNativeLatchSnapshot *sample,
    unsigned context) {
  if(!o||!sample||o->emitted>=64u)return 0;
  if(o->have_previous && o->context==context &&
     memcmp(&o->previous,sample,sizeof(*sample))==0)return 0;
  o->previous=*sample;
  o->context=context;
  o->have_previous=1u;
  o->emitted++;
  return 1;
}
#endif
