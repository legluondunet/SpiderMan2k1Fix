#ifndef SPIDERMAN2K1FIX_AUDIO_LOOP_TAIL_REPAIR_H
#define SPIDERMAN2K1FIX_AUDIO_LOOP_TAIL_REPAIR_H
#include <windows.h>
#include <dsound.h>
void SpiderLoopTailRepair_Init(void (*log_fn)(const char *),int enabled);
void SpiderLoopTailRepair_Probe(int slot,IDirectSoundBuffer *buf,DWORD bytes,DWORD hz,int looping);
#endif
