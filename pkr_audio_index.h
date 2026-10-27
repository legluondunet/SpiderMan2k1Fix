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
/* Separate opt-in: restore ONLY a source-identified, verified corrupt
 * trailing partial 2048-byte WAV sector inside a non-playing DS buffer.
 * Original game and all other audio semantics remain native DirectSound. */
void SpiderPKRAudioIndex_EnableTailRestore(int enabled);
int SpiderPKRAudioIndex_TryTailRestore(int slot,IDirectSoundBuffer *buffer,int looping);
#endif
