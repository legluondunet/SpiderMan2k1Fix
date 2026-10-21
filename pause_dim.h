#ifndef SPIDERMAN2K1FIX_PAUSE_DIM_H
#define SPIDERMAN2K1FIX_PAUSE_DIM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Foreground: original 55% gridded game-owned alpha popup.
 * Background: sparse (dithered) 16px gridded nonlayered WS_CHILD
 * clipped to the game, never to another desktop application.
 * Both effects are disabled by [Input] PauseDim=0. */
void SpiderPauseDim_Init(void (*log_fn)(const char *));
void SpiderPauseDim_Show(HWND game_window);
void SpiderPauseDim_ShowBackground(HWND game_window);
void SpiderPauseDim_Hide(void);

#endif
