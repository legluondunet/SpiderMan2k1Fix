#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_PROBE_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_PROBE_H
#include <windows.h>
#include <stdint.h>

/* OFF-by-default SDL3 discovery and selection telemetry only.
 * NO game input writes, synthetic keys, hooks or native DI device mutations.
 * SDL3.dll is optional and is only loaded from the executable directory. */
void SpiderSDL3Probe_Init(const char *ini,void (*logger)(const char *));
void SpiderSDL3Probe_OnFlip(long flip,HWND game_hwnd);
/* Opt-in native ownership experiment. Read only from the same Flip thread.
 * 'Available' means SDL initialized and selected an instance; it does NOT
 * mean DirectInput has been rebound. Duplicate VID/PID must fail closed. */
int SpiderSDL3Probe_NativeOwnerEnabled(void);
unsigned SpiderSDL3Probe_MappedCount(void);
int SpiderSDL3Probe_SelectedIdentity(uint16_t *vid,uint16_t *pid,
                                     uint32_t *instance_id);

#endif
