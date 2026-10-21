#ifndef SPIDER_PAUSE_ANY_KEY_H
#define SPIDER_PAUSE_ANY_KEY_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Optional focus-pause wake on any keyboard key, with the key swallowed.
 * The caller MUST pump messages on the SAME thread until Completed().
 * Begin installs a transient, foreground-filtered WH_KEYBOARD_LL hook;
 * End always unhooks. Return FALSE from Begin to fall back to F9. */
BOOL SpiderPauseAny_Begin(HWND game_hwnd);
BOOL SpiderPauseAny_Completed(void);
BOOL SpiderPauseAny_DiscardGameKeyMessage(const MSG *msg, HWND game_hwnd);
DWORD SpiderPauseAny_FirstKey(void);
void SpiderPauseAny_End(void);
#endif
