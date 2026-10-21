#ifndef SPIDERMAN2K1FIX_FOCUS_MEDIA_PAUSE_H
#define SPIDERMAN2K1FIX_FOCUS_MEDIA_PAUSE_H
/* Opt-in native Bink cinematic/audio-only music focus pause.
 * [Input] PauseBinkOnFocusLoss=1; DirectSound voices handled separately. */
void SpiderFocusMedia_Init(void (*log_fn)(const char *));
void SpiderFocusMedia_Pause(void);
void SpiderFocusMedia_Resume(void);
#endif
