#ifndef SPIDERMAN2K1FIX_FOCUS_DSOUND_PAUSE_H
#define SPIDERMAN2K1FIX_FOCUS_DSOUND_PAUSE_H
/* Optional, exact-executable secondary DirectSound buffer pause.
 * It never patches DirectSound, alters COM vtables or stops the primary.
 * Pauses only voices already playing, resumes those same voices if still
 * present in the game-owned voice table. Off by default. */
void SpiderFocusDSound_Init(void (*log_fn)(const char *));
/* Read-only per-Flip diagnostic; OFF by default. in_3d excludes menus. */
void SpiderFocusDSound_TraceOnFlip(long flip,int in_3d);
void SpiderFocusDSound_Pause(void);
void SpiderFocusDSound_Resume(void);
#endif
