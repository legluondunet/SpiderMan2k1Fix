#ifndef SPIDERMAN2K1FIX_DS4_HOTPLUG_H
#define SPIDERMAN2K1FIX_DS4_HOTPLUG_H
/*
 * Independent Sony DS4 DirectInput discovery / reattachment state.
 * Portable C99, no Win32 hooks or original-game controller mutation.
 * Timing is wrap-safe for GetTickCount's 32-bit millisecond counter.
 */
#include <stdint.h>
#define SPIDER_DS4_RESCAN_MS 2000u
#define SPIDER_DS4_READ_FAILURE_LIMIT 3u
#define SPIDER_DS4_BUTTON_MASK 0x0fffu

typedef struct {
  uint32_t last_scan;
  unsigned scan_started;
  unsigned read_failures;
  unsigned require_neutral;
} SpiderDS4Hotplug;

static int spider_ds4_hotplug_scan_due(SpiderDS4Hotplug *state,
                                       uint32_t now,int attached) {
  if(!state || attached)return 0;
  if(state->scan_started &&
     (uint32_t)(now-state->last_scan)<SPIDER_DS4_RESCAN_MS)return 0;
  state->scan_started=1u;
  state->last_scan=now;
  return 1;
}
static void spider_ds4_hotplug_attached(SpiderDS4Hotplug *state) {
  if(!state)return;
  state->require_neutral=1u;
  state->read_failures=0u;
}
static void spider_ds4_hotplug_focus_lost(SpiderDS4Hotplug *state) {
  if(!state)return;
  state->require_neutral=1u;
  state->read_failures=0u;
}
static int spider_ds4_hotplug_read_failed(SpiderDS4Hotplug *state) {
  if(!state)return 0;
  state->require_neutral=1u;
  if(state->read_failures<SPIDER_DS4_READ_FAILURE_LIMIT)
    state->read_failures++;
  return state->read_failures>=SPIDER_DS4_READ_FAILURE_LIMIT;
}
/* Returning false blocks synthetic Return/Escape for the whole sample.
 * A new device or reacquired device must first report a neutral button
 * state; no held Cross/Share on reconnect becomes an accidental keypress.
 * Analog axes and POV hats do not trigger synthetic UI keys.
 */
static int spider_ds4_hotplug_accept_buttons(SpiderDS4Hotplug *state,
                                             unsigned held) {
  if(!state)return 0;
  state->read_failures=0u;
  if(!state->require_neutral)return 1;
  if(held&SPIDER_DS4_BUTTON_MASK)return 0;
  state->require_neutral=0u;
  return 0;
}
#endif
