#ifndef SPIDERMAN2K1FIX_PKR_AUDIO_INDEX_H
#define SPIDERMAN2K1FIX_PKR_AUDIO_INDEX_H
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
/* Read-only: index original PCM16 WAVs in data.pkr, compare native game
 * buffers by format, exact length and a substantial matching prefix.
 * NO audio replacement, data mutations, or game-owned PKR calls. */
void SpiderPKRAudioIndex_Init(void (*logger)(const char*),int enabled);
void SpiderPKRAudioIndex_Probe(int slot,IDirectSoundBuffer *buffer,int looping);
#endif
