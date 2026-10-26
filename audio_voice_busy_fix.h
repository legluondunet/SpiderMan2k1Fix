#ifndef SPIDERMAN2K1FIX_AUDIO_VOICE_BUSY_FIX_H
#define SPIDERMAN2K1FIX_AUDIO_VOICE_BUSY_FIX_H
/* Opt-in guarded repair of the native GetStatus LOOPING vs PLAYING mistake.
 * Call only after the original EXE SHA-256 and INI are validated. */
void SpiderVoiceBusyFix_Init(void (*log_fn)(const char *));
#endif
