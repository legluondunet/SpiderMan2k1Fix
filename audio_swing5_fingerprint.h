#ifndef SPIDERMAN2K1FIX_AUDIO_SWING5_FINGERPRINT_H
#define SPIDERMAN2K1FIX_AUDIO_SWING5_FINGERPRINT_H
#include <windows.h>
#include <dsound.h>
/* Explicit opt-in: a brief DirectSound Lock/Unlock is read-only for
 * samples, but can affect audio timing. Diagnostic use only. */
void SpiderSwing5Fingerprint_Init(void (*log_fn)(const char *),int enabled,
                                   int capture_wav);
void SpiderSwing5Fingerprint_Probe(int slot,IDirectSoundBuffer *buffer,
                                    DWORD bytes,DWORD frequency);
#endif
