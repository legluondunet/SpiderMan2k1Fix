#ifndef SPIDERMAN2K1FIX_PAUSE_DIM_H
#define SPIDERMAN2K1FIX_PAUSE_DIM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Render only while focus pause is latched. Non-topmost owned WS_POPUP:
 * black 40% uniform alpha, NOACTIVATE, transparent hit testing, owner bound.
 * Excludes the shaped centered PAUSE banner so red/white text stays bright.
 * This works without changing the original game's compatibility manifest.
 * Default on for active pauses; [Input] PauseDim=0 to turn off. */
void SpiderPauseDim_Init(void (*log_fn)(const char *));
void SpiderPauseDim_Show(HWND game_window);
void SpiderPauseDim_Hide(void);

#endif
