#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_PROBE_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_PROBE_H
#include <windows.h>

/* OFF-by-default SDL3 discovery and selection telemetry only.
 * NO game input writes, synthetic keys, hooks or native DI device mutations.
 * SDL3.dll is optional and is only loaded from the executable directory. */
void SpiderSDL3Probe_Init(const char *ini,void (*logger)(const char *));
void SpiderSDL3Probe_OnFlip(long flip,HWND game_hwnd);
#endif
