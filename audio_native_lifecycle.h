#ifndef SPIDERMAN2K1FIX_AUDIO_NATIVE_LIFECYCLE_H
#define SPIDERMAN2K1FIX_AUDIO_NATIVE_LIFECYCLE_H
#include <windows.h>
/* Opt-in native event probe: call from SHA-validated bootstrap and 3D Flip. */
void SpiderNativeAudioTrace_Init(void (*log_fn)(const char *));
void SpiderNativeAudioTrace_OnFlip(LONG flip,int in_3d);
#endif
