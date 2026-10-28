#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_SLOTS_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_SLOTS_H
/* SDL3 device slots: portable C99, deliberately independent of Windows and
 * the SDL DLL. Detaching one device must close its handle EXACTLY once.
 * Remove the entire stale slot before the next enumeration, compacting the
 * live array to avoid repeated SDL_CloseGamepad(NULL) and log spam. */
#include <stdint.h>
#include "sdl3_gamepad_selection.h"

typedef struct {uint32_t id; void *handle;} SpiderSDL3Opened;
typedef void (*SpiderSDL3CloseFn)(void *);
typedef void (*SpiderSDL3NotifyFn)(uint32_t);

static unsigned spider_sdl3_drop_absent(SpiderSDL3Opened *pads,
                                       unsigned *count,
                                       const uint32_t *present,
                                       unsigned present_count,
                                       SpiderSDL3CloseFn close_device,
                                       SpiderSDL3NotifyFn on_detached) {
  unsigned i=0,removed=0;
  if(!pads || !count || *count>SPIDER_SDL3_MAX_GAMEPADS)return 0;
  while(i<*count) {
    unsigned j;
    int found=0;
    if(pads[i].handle && pads[i].id && present)
      for(j=0;j<present_count;j++)
        if(present[j]==pads[i].id) {found=1;break;}
    if(found) {i++;continue;}
    if(pads[i].handle) {
      if(on_detached)on_detached(pads[i].id);
      if(close_device)close_device(pads[i].handle);
      removed++;
    }
    /* Compact immediately, including malformed empty entries left by a
     * past partial scan. No retained stale instance can be closed twice. */
    for(j=i+1;j<*count;j++)pads[j-1]=pads[j];
    (*count)--;
    pads[*count].id=0;
    pads[*count].handle=0;
  }
  return removed;
}
#endif
