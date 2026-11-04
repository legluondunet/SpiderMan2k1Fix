#ifndef SPIDERMAN2K1FIX_INPUT_FOCUS_TRACE_H
#define SPIDERMAN2K1FIX_INPUT_FOCUS_TRACE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Opt-in focus tracing and separately opt-in guarded DirectInput8 reacquisition. */
void SpiderInputFocus_Install(void (*log_fn)(const char *));
void SpiderInputFocus_AttachWindow(HWND hwnd);
void SpiderInputFocus_OnFlip(LONG flip_number);
/* Opt-in Wine fallback: record an app deactivation that survived >=150 ms
 * but never reached an active Flip. Return a single deferred pause request
 * only after WM_ACTIVATEAPP(TRUE), never from an unrelated window. */
BOOL SpiderInputFocus_ConsumeDeferredPause(DWORD *elapsed_ms);
/* A normal Flip-driven background pause supersedes the deferred request.
 * Avoid a duplicate PAUSED panel when the application is reactivated. */
void SpiderInputFocus_DiscardDeferredPause(void);
#endif
