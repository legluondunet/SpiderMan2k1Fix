#ifndef SPIDERMAN2K1FIX_PKR_AUDIO_PLAY_HOOK_H
#define SPIDERMAN2K1FIX_PKR_AUDIO_PLAY_HOOK_H
/* Intercept a verified original x86 DXSOUND_Play callsite only to
 * inspect/restore source-backed WAV data before the native Play.
 * No replacement sound engine, DirectSound Stop patch or SDL runtime. */
void SpiderPKRAudioPlayHook_Init(void (*logger)(const char*),int enabled,int trace);
#endif
