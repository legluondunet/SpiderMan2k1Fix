#ifndef SPIDERMAN2K1FIX_AUDIO_COMBAT_TAIL_REPAIR_H
#define SPIDERMAN2K1FIX_AUDIO_COMBAT_TAIL_REPAIR_H
#include <windows.h>
#include <dsound.h>
/* Exact-SHA guarded optional mitigation for 3 user-auditioned one-shot
 * PCM tail corruptions. No effect on unmatched/looping buffers. */
void SpiderCombatTailRepair_Init(void (*logger)(const char *),int enabled);
void SpiderCombatTailRepair_Probe(int slot,IDirectSoundBuffer *buffer,
                                  DWORD bytes,DWORD hz,int looping);
#endif
