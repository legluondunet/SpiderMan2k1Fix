#ifndef SPIDERMAN2K1FIX_SDL3_AUDIO_PROBE_H
#define SPIDERMAN2K1FIX_SDL3_AUDIO_PROBE_H
#include <windows.h>
void SpiderSDL3Probe_Init(void (*logger)(const char*),int enabled);
void SpiderSDL3Probe_OnFlip(HWND game);
#endif
