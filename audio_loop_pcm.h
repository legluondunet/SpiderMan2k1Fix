#ifndef SPIDEY_AUDIO_LOOP_PCM_H
#define SPIDEY_AUDIO_LOOP_PCM_H
#include <windows.h>
#include <dsound.h>
/* Explicit user opt-in, read-only PCM bytes, Lock/Unlock may affect timing. */
void SpiderLoopPCM_Init(void (*logger)(const char *),int enabled,int capture);
void SpiderLoopPCM_Probe(int slot,IDirectSoundBuffer *buf,DWORD bytes,DWORD hz,int loop_flag);
#endif
