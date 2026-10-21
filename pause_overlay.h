#ifndef SPIDERMAN2K1FIX_PAUSE_OVERLAY_H
#define SPIDERMAN2K1FIX_PAUSE_OVERLAY_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* One atomic nonactivating owned popup (ULW_ALPHA). No old shaped child.
 * Only foreground game windows may create the comic-style banner. */
void SpiderPauseOverlay_Init(void (*log_fn)(const char *));
int SpiderPauseOverlay_GameForeground(HWND game_window);
/* Foreground: composited translucent popup. Background: ordinary
 * opaque, rounded WS_CHILD attached to the game (never desktop-floating). */
void SpiderPauseOverlay_Show(HWND game_window);
void SpiderPauseOverlay_ShowBackground(HWND game_window);
void SpiderPauseOverlay_Hide(void);

/* Entire one-piece panel rectangle in GAME CLIENT coordinates. */
int SpiderPauseOverlay_GetBounds(HWND game_window, RECT *bounds);
/* Exclude the FULL rounded panel from the game-wide alpha shade/grid. */
int SpiderPauseOverlay_ExcludeFromRegion(HWND game_window, HRGN visible_region);
#endif
