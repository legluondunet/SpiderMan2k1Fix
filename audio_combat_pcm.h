#ifndef SPIDERMAN2K1FIX_AUDIO_COMBAT_PCM_H
#define SPIDERMAN2K1FIX_AUDIO_COMBAT_PCM_H
#include <windows.h>
#include <dsound.h>
/* Opt-in read-only copies of distinct original game one-shot PCM. */
void SpiderCombatPCM_Init(void (*log_fn)(const char *),int enabled,DWORD start_ms,int manual_f8);
void SpiderCombatPCM_On3DStart(void);
void SpiderCombatPCM_OnF8(DWORD native_ms);
int SpiderCombatPCM_IsManualF8(void);
void SpiderCombatPCM_OnFlip(DWORD native_ms);
void SpiderCombatPCM_Probe(int slot,IDirectSoundBuffer *buf,DWORD bytes,DWORD native_ms,int looping);
#endif
