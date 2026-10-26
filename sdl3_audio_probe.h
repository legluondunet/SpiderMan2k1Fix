#ifndef SPIDERMAN2K1FIX_SDL3_AUDIO_PROBE_H
#define SPIDERMAN2K1FIX_SDL3_AUDIO_PROBE_H
#include <windows.h>
void SpiderSDL3Probe_Init(void (*logger)(const char*),int enabled,int replace_swing5);
/* TRUE only if the verified original source was queued/resumed successfully. */
int SpiderSDL3Probe_PlaySwing5(int slot,LONG ds_volume,LONG ds_pan,DWORD actual_hz);
void SpiderSDL3Probe_OnFlip(HWND game);
#endif
