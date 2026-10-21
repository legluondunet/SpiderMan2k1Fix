#ifndef SPIDERMAN2K1FIX_PAUSE_DIM_H
#define SPIDERMAN2K1FIX_PAUSE_DIM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Render only while focus pause is latched. Native Windows 11 layered child:
 * black 40% uniform alpha, NOACTIVATE, transparent hit testing, parent bound.
 * Independent of the existing centered PAUSE banner.
 * Default on for active pauses; [Input] PauseDim=0 to turn off. */
void SpiderPauseDim_Init(void (*log_fn)(const char *));
void SpiderPauseDim_Show(HWND game_window);
void SpiderPauseDim_Hide(void);

#endif
