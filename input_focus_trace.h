#ifndef SPIDERMANFIX_INPUT_FOCUS_TRACE_H
#define SPIDERMANFIX_INPUT_FOCUS_TRACE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Opt-in focus tracing and separately opt-in guarded DirectInput8 reacquisition. */
void SpiderInputFocus_Install(void (*log_fn)(const char *));
void SpiderInputFocus_AttachWindow(HWND hwnd);
void SpiderInputFocus_OnFlip(LONG flip_number);
#endif