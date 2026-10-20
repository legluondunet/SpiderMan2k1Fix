#ifndef SPIDERMAN2K1FIX_FOCUS_EVENT_PAUSE_H
#define SPIDERMAN2K1FIX_FOCUS_EVENT_PAUSE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Diagnostic prototype only; default OFF. Win32/Wine focus-message observer
 * with an opt-in pause gate at a known-safe DirectDraw Flip boundary.
 * This does NOT claim to pause independently timed movie/audio threads or
 * guarantee immediate simulation suspension before the next Flip.
 */
void SpiderFocusEventPause_Init(void (*logger)(const char *));
void SpiderFocusEventPause_AttachWindow(HWND hwnd);
int SpiderFocusEventPause_IsEnabled(void);
void SpiderFocusEventPause_OnFlip(void);

#endif
