#ifndef SPIDERMAN2K1FIX_SDL3_NATIVE_SOURCE_OBSERVER_H
#define SPIDERMAN2K1FIX_SDL3_NATIVE_SOURCE_OBSERVER_H
/* Portable, read-only snapshot of the game's TWO original DI latches.
 * No Windows calls, hardware access, native pointers or input injection.
 * A count of pressed keyboard scan codes is intentionally used instead of
 * the individual keys, so this diagnostic does not log typed key sequences.
 */
#include <stdint.h>
#include <string.h>

enum { SPIDER_SOURCE_KEY_BYTES=256, SPIDER_SOURCE_JOY_BYTES=32,
       SPIDER_SOURCE_MAX_CHANGES=96, SPIDER_SOURCE_MAX_FOCUS_EVENTS=12 };
typedef struct {
  uint16_t keyboard_held, keyboard_fresh, keyboard_released;
  uint32_t joystick_held, joystick_fresh, joystick_released;
} SpiderNativeSourceSnapshot;
typedef struct {
  SpiderNativeSourceSnapshot previous;
  unsigned have_previous, context, emitted;
  unsigned have_focus, was_active, focus_events;
} SpiderNativeSourceObserver;
static inline void spider_native_source_reset(SpiderNativeSourceObserver *o) {
  if(o)memset(o,0,sizeof(*o));
}
static inline void spider_native_source_decode(
    const uint8_t keyboard[SPIDER_SOURCE_KEY_BYTES],
    const uint8_t joystick[SPIDER_SOURCE_JOY_BYTES],
    SpiderNativeSourceSnapshot *out) {
  unsigned i;
  if(!out)return;
  memset(out,0,sizeof(*out));
  if(!keyboard||!joystick)return;
  for(i=0;i<SPIDER_SOURCE_KEY_BYTES;i++) {
    if(keyboard[i]&0x7fu)out->keyboard_held++;
    if(keyboard[i]==0xffu)out->keyboard_fresh++;
    if(keyboard[i]==0x80u)out->keyboard_released++;
  }
  for(i=0;i<SPIDER_SOURCE_JOY_BYTES;i++) {
    if(joystick[i]&0x7fu)out->joystick_held|=(uint32_t)1u<<i;
    if(joystick[i]==0xffu)out->joystick_fresh|=(uint32_t)1u<<i;
    if(joystick[i]==0x80u)out->joystick_released|=(uint32_t)1u<<i;
  }
}
/* Track foreground/context transitions independently of the sample budget.
 * Returns 1 only when status changed and the separate budget permits logging.
 * 1 means eligible foreground, 0 means skipped (background/Bink/pause).
 * This cannot detect focus loss if the game's Flip callback stops entirely.
 */
static inline int spider_native_source_focus_step(
    SpiderNativeSourceObserver *o,unsigned active) {
  if(!o)return 0;
  active=!!active;
  if(o->have_focus&&o->was_active==active)return 0;
  if(o->have_focus&&o->was_active!=active)o->have_previous=0;
  o->have_focus=1u;
  o->was_active=active;
  if(o->focus_events>=SPIDER_SOURCE_MAX_FOCUS_EVENTS)return 0;
  o->focus_events++;
  return 1;
}
static inline int spider_native_source_changed(
    SpiderNativeSourceObserver *o,const SpiderNativeSourceSnapshot *s,
    unsigned context) {
  if(!o||!s||o->emitted>=SPIDER_SOURCE_MAX_CHANGES)return 0;
  if(o->have_previous&&o->context==context&&
     memcmp(&o->previous,s,sizeof(*s))==0)return 0;
  o->previous=*s;
  o->context=context;
  o->have_previous=1u;
  o->emitted++;
  return 1;
}
#endif
