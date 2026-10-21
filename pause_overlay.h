#ifndef SPIDERMAN2K1FIX_PAUSE_OVERLAY_H
#define SPIDERMAN2K1FIX_PAUSE_OVERLAY_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* A passive child window within the game, never a desktop-topmost overlay.
 * Shown only during an actual focus pause. Does not intercept gameplay input. */
void SpiderPauseOverlay_Init(void (*log_fn)(const char *));
void SpiderPauseOverlay_Show(HWND game_window, int resume_key);
void SpiderPauseOverlay_Hide(void);

#endif
