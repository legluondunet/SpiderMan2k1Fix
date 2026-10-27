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
/* Experimental, OFF by default: correct two proven rogue PCM16 samples
 * in uniquely identified original Heli1.wav, stopped secondary buffer. */
void SpiderPKRAudioIndex_EnableHeli1Declick(int enabled);
int SpiderPKRAudioIndex_TryHeli1Declick(int slot,IDirectSoundBuffer *buffer,int looping);
/* Strictly read-only matching of a WAV file ALREADY read into the game's
 * allocated staging buffer. Returns a pointer into the independently
 * decompressed, CRC32-verified original WAV/RIFF cache only for exactly
 * one FULL original-file length and bytewise full-sector prefix match.
 * Covers 44-byte and 190-byte legitimate RIFF metadata following PCM.
 * Caller never writes into the independently verified source cache.
 * Loader hook uses this to VERIFY any extra native partial-block read
 * before it writes the proven remainder into the original staging buffer. */
const unsigned char *SpiderPKRAudioIndex_ExpectedLoaderWavTail(
  const unsigned char *wav,unsigned complete_bytes,unsigned total_bytes,
  const char **source_name);
/* For skipped native WAV reads ONLY: bounded, read-only original PKR
 * index census. A shared first-sector fingerprint is merely an identity
 * hint; it is NEVER accepted as proof for modifying PCM. */
void SpiderPKRAudioIndex_DescribeLoaderSkip(
  const unsigned char *wav,unsigned complete_bytes,unsigned total_bytes,
  char *out,unsigned out_capacity);
#endif
