#ifndef SPIDERMAN2K1FIX_PAUSE_OVERLAY_H
#define SPIDERMAN2K1FIX_PAUSE_OVERLAY_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* A passive child window within the game, never a desktop-topmost overlay.
 * Shown only during an actual focus pause. Does not intercept gameplay input. */
void SpiderPauseOverlay_Init(void (*log_fn)(const char *));
void SpiderPauseOverlay_Show(HWND game_window, int resume_key);
void SpiderPauseOverlay_Hide(void);

/* Remove only the visible border + glyph pixels of the centered banner
 * from a proposed dimming region (coordinates relative to game client).
 * Returns 1 when exclusion succeeded or when no banner exists, 0 on error.
 * The caller retains ownership of visible_region. */
int SpiderPauseOverlay_ExcludeFromRegion(HWND game_window, HRGN visible_region);

#endif
