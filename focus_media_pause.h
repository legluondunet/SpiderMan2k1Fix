#ifndef SPIDERMAN2K1FIX_FOCUS_MEDIA_PAUSE_H
#define SPIDERMAN2K1FIX_FOCUS_MEDIA_PAUSE_H
/* Experimental native Bink video/audio pause from the existing Flip pause.
 * Enabled only by [Input] PauseBinkOnFocusLoss=1. No DirectSound mutation. */
void SpiderFocusMedia_Init(void (*log_fn)(const char *));
void SpiderFocusMedia_Pause(void);
void SpiderFocusMedia_Resume(void);
#endif
