#ifndef SPIDERMAN2K1FIX_PS1_FOCUS_PAUSE_WAKE_H
#define SPIDERMAN2K1FIX_PS1_FOCUS_PAUSE_WAKE_H
/*
 * Pure C99 digital-button wake state for the custom Alt+Tab PAUSED screen.
 * This is NOT the original game's Escape/3D pause menu.
 *
 * A newly returned foreground session must first observe ALL relevant DS4
 * buttons released; a full new press AND release then wakes. Requiring
 * neutral first prevents a button held while Alt+Tabbing from auto-resuming.
 * Only physical digital buttons 0..11 are used, never stick/axis/POV drift.
 * No synthetic key or native gamepad state is changed by this state machine.
 */
#define SPIDER_PAUSE_PAD_DIGITAL_MASK 0x0fffu

typedef struct {
  unsigned seen_neutral;
  unsigned pending;
  unsigned initial_press;
} SpiderPausePadWake;

static unsigned spider_pause_pad_wake_step(SpiderPausePadWake *w,
                                           unsigned held_buttons) {
  unsigned result;
  if(!w)return 0u;
  held_buttons&=SPIDER_PAUSE_PAD_DIGITAL_MASK;
  if(!w->seen_neutral) {
    if(!held_buttons)w->seen_neutral=1u;
    return 0u;
  }
  if(!w->pending) {
    if(held_buttons) {
      w->pending=1u;
      w->initial_press=held_buttons;
    }
    return 0u;
  }
  if(held_buttons)return 0u;
  result=w->initial_press;
  w->pending=0u;
  w->initial_press=0u;
  return result;
}
/* Multi-pad SDL3 wake: state follows exact instance identity rather than
 * array order. Invalid/detached instances disappear on the next sample.
 * A new device or focus epoch must observe full neutral before any press.
 * Competing pending presses cancel all candidates until a new neutral. */
#include <stdint.h>
#include <string.h>
#define SPIDER_PAUSE_MAX_PADS 16u
typedef struct {
  uint32_t id;
  uint16_t vid,pid;
  unsigned buttons,neutral;
} SpiderPausePadSample;
typedef struct {
  SpiderPausePadSample identity;
  SpiderPausePadWake wake;
} SpiderPausePadSlot;
typedef struct {
  SpiderPausePadSlot slots[SPIDER_PAUSE_MAX_PADS];
  unsigned count;
} SpiderPausePads;
static inline void spider_pause_pads_reset(SpiderPausePads *s) {
  memset(s,0,sizeof(*s));
}
static inline unsigned spider_pause_pads_step(SpiderPausePads *s,
    const SpiderPausePadSample *samples,unsigned count,SpiderPausePadSample *winner) {
  SpiderPausePads next={0};
  unsigned i,j,contenders=0,result=0;
  memset(winner,0,sizeof(*winner));
  if(count>SPIDER_PAUSE_MAX_PADS) {spider_pause_pads_reset(s);return 0;}
  for(i=0;i<count;i++) {
    SpiderPausePadSlot *slot=&next.slots[next.count];
    unsigned completed;
    if(!samples[i].id || !samples[i].vid || !samples[i].pid)continue;
    for(j=0;j<next.count;j++)if(next.slots[j].identity.id==samples[i].id) {
      spider_pause_pads_reset(s);return 0;
    }
    slot->identity=samples[i];
    for(j=0;j<s->count;j++)if(s->slots[j].identity.id==samples[i].id &&
        s->slots[j].identity.vid==samples[i].vid &&
        s->slots[j].identity.pid==samples[i].pid) {
      slot->wake=s->slots[j].wake;break;
    }
    next.count++;
    /* Full neutral before arming and after release. Analog movement or
     * ignored buttons alone cannot complete a digital wake. */
    if(!samples[i].buttons && !samples[i].neutral) {
      memset(&slot->wake,0,sizeof(slot->wake));continue;
    }
    completed=spider_pause_pad_wake_step(&slot->wake,samples[i].buttons);
    if(slot->wake.pending || completed)contenders++;
    if(completed) {result=completed;*winner=samples[i];}
  }
  if(contenders>1u) {
    spider_pause_pads_reset(s);memset(winner,0,sizeof(*winner));return 0;
  }
  *s=next;
  return result;
}
#endif
