#ifndef SPIDERMAN2K1FIX_ALT_LATCH_RECOVERY_POLICY_H
#define SPIDERMAN2K1FIX_ALT_LATCH_RECOVERY_POLICY_H
/* Portable C99 policy ONLY. No Windows APIs and no game-memory writes.
 * A fix may be authorized ONLY following a confirmed external loss of focus
 * and a foreground wake, never after a generic Bink/pause transition.
 */
#include <stdint.h>

enum {
  SPIDER_ALT_LATCH_MIN_UP_FRAMES = 6,
  SPIDER_ALT_LATCH_MIN_UP_MS = 180,
  SPIDER_ALT_LATCH_MAX_AFTER_RETURN_MS = 5000
};
typedef struct {
  unsigned confirmed_loss;
  unsigned pending;
  unsigned stable_up_frames;
} SpiderAltLatchRecoveryPolicy;

/* A new loss invalidates any pending recovery for the previous focus epoch. */
static inline void spider_alt_latch_policy_loss(
    SpiderAltLatchRecoveryPolicy *p) {
  if(!p)return;
  p->confirmed_loss=1u;
  p->pending=0u;
  p->stable_up_frames=0u;
}
/* Only a foreground, explicit return from the game's focus-pause may arm. */
static inline void spider_alt_latch_policy_return(
    SpiderAltLatchRecoveryPolicy *p,unsigned is_foreground) {
  if(!p)return;
  p->pending=p->confirmed_loss&&!!is_foreground;
  p->confirmed_loss=0u;
  p->stable_up_frames=0u;
}
/* Result: 0 wait/no-op, 1 one-byte release authorized, 2 naturally
 * cleared/changed (do nothing), 3 expired (do nothing).
 * Physical Alt+Tab/RAlt still down RESETS the streak; it never authorizes
 * writes. The original DI Left Alt must be EXACTLY 0x7F (not fresh 0xFF).
 * A return permits at most one release marker; no repeated injections.
 */
static inline int spider_alt_latch_policy_step(
    SpiderAltLatchRecoveryPolicy *p,unsigned eligible,
    uint8_t di_left_alt,unsigned all_os_modifiers_up,
    unsigned other_original_modifiers_up,unsigned ms_since_return) {
  if(!p||!p->pending)return 0;
  if(ms_since_return>SPIDER_ALT_LATCH_MAX_AFTER_RETURN_MS) {
    p->pending=0u;
    return 3;
  }
  if(!eligible) {
    p->stable_up_frames=0u;
    return 0;
  }
  if(di_left_alt!=0x7fu) {
    p->pending=0u;
    return 2;
  }
  if(!all_os_modifiers_up||!other_original_modifiers_up) {
    p->stable_up_frames=0u;
    return 0;
  }
  if(p->stable_up_frames<SPIDER_ALT_LATCH_MIN_UP_FRAMES)
    p->stable_up_frames++;
  if(p->stable_up_frames>=SPIDER_ALT_LATCH_MIN_UP_FRAMES &&
     ms_since_return>=SPIDER_ALT_LATCH_MIN_UP_MS) {
    p->pending=0u;
    return 1;
  }
  return 0;
}
#endif
