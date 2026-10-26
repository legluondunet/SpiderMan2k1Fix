#ifndef SPIDERMAN2K1FIX_SDL3_AUDIO_PROBE_H
#define SPIDERMAN2K1FIX_SDL3_AUDIO_PROBE_H
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
void SpiderSDL3Probe_Init(void (*logger)(const char*),int enabled,int replace_swing5);
/* TRUE only if the verified original source was queued/resumed successfully. */
int SpiderSDL3Probe_PlaySwing5(int slot,IDirectSoundBuffer *original,
                             LONG ds_volume,LONG ds_pan,DWORD actual_hz);
/* One owner per native 32-voice slot. Hooks and Flip share this lifecycle. */
void SpiderSDL3Probe_StopSlot(int slot,const char *reason);
void SpiderSDL3Probe_PauseAll(int paused);
void SpiderSDL3Probe_OnVoiceFlip(int menu_active);
void SpiderSDL3Probe_OnFlip(HWND game);
#endif
