#ifndef SPIDERMAN2K1FIX_PS1_INPUT_FLIGHT_RECORDER_H
#define SPIDERMAN2K1FIX_PS1_INPUT_FLIGHT_RECORDER_H
/*
 * Strictly opt-in, bounded, in-memory diagnostic for DS4 button edges
 * near the original game's native 3D -> 2D Quit transition.
 * This header is pure C99, usable by regression tests on Linux.
 * NO allocation, OS calls, logging, keyboard injection or native writes.
 */
#include <stdint.h>

#define SPIDER_PS1_HISTORY_CAPACITY 96u
enum SpiderPS1HistoryEvent {
  SPIDER_PS1_HISTORY_READ_OK=0,
  SPIDER_PS1_HISTORY_FOCUS_LOST=1,
  SPIDER_PS1_HISTORY_READ_FAILED=2,
  SPIDER_PS1_HISTORY_DEVICE_MISSING=3
};

typedef struct {
  uint32_t sequence;
  uint32_t tick_ms;
  uint32_t native_request;
  uint32_t previous_confirm; /* old BUTTON1/BUTTON9 mask */
  uint8_t cross;
  uint8_t options;
  uint8_t circle;
  uint8_t share;
  uint8_t menu2d;
  uint8_t gameplay3d;
  uint8_t movie;
  uint8_t native_pause;
  uint8_t event;
} SpiderPS1HistoryEntry;

typedef struct {
  SpiderPS1HistoryEntry entries[SPIDER_PS1_HISTORY_CAPACITY];
  uint32_t total;
} SpiderPS1InputHistory;

/* Recorder pushes exact pre-edge state, including a DI/focus interruption.
 * A single memset+assignment per Flip; no disk writes during sampling. */
static void spider_ps1_history_push(SpiderPS1InputHistory *history,
                                   const SpiderPS1HistoryEntry *entry) {
  if(!history || !entry)return;
  history->entries[history->total%SPIDER_PS1_HISTORY_CAPACITY]=*entry;
  ++history->total;
}
static unsigned spider_ps1_history_count(const SpiderPS1InputHistory *history) {
  if(!history)return 0u;
  return history->total<SPIDER_PS1_HISTORY_CAPACITY?
    (unsigned)history->total:SPIDER_PS1_HISTORY_CAPACITY;
}
/* index 0 = oldest surviving sample, not the ring's physical index. */
static const SpiderPS1HistoryEntry *spider_ps1_history_at(
    const SpiderPS1InputHistory *history,unsigned index) {
  unsigned count=spider_ps1_history_count(history);
  uint32_t oldest;
  if(!history || index>=count)return (const SpiderPS1HistoryEntry *)0;
  oldest=history->total-(uint32_t)count;
  return &history->entries[(oldest+index)%SPIDER_PS1_HISTORY_CAPACITY];
}
#endif
