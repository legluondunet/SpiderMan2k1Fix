#ifndef SPIDERMAN2K1FIX_WIN32_FOCUS_MESSAGE_TRACE_H
#define SPIDERMAN2K1FIX_WIN32_FOCUS_MESSAGE_TRACE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Strictly passive, opt-in Win32 focus-message observation.
 * Does not subclass WndProc, dispatch messages, or alter focus/pause/input. */
void SpiderWin32FocusTrace_Init(void (*logger)(const char *),int debug_enabled);
void SpiderWin32FocusTrace_AttachWindow(HWND hwnd);
void SpiderWin32FocusTrace_Heartbeat(HWND hwnd,DWORD now);
#endif
