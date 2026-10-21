#ifndef SPIDER_PAUSE_ANY_KEY_H
#define SPIDER_PAUSE_ANY_KEY_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* The ONLY focus-pause wake path: any keyboard key (press+release swallowed).
 * The caller MUST pump messages on the SAME thread until Completed().
 * Begin installs a transient foreground-filtered WH_KEYBOARD_LL hook;
 * End always unhooks. If Begin fails, skip the focus pause safely. */
BOOL SpiderPauseAny_Begin(HWND game_hwnd);
BOOL SpiderPauseAny_Completed(void);
BOOL SpiderPauseAny_DiscardGameKeyMessage(const MSG *msg, HWND game_hwnd);
DWORD SpiderPauseAny_FirstKey(void);
void SpiderPauseAny_End(void);
#endif
