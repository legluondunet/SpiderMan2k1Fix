#ifndef SPIDERMAN2K1FIX_PS1_MOUSE_MODE_H
#define SPIDERMAN2K1FIX_PS1_MOUSE_MODE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/*
 * Original retail game mouse-disable flag, not the Win32 desktop cursor.
 * Requires exact original mouse instructions and a writable native flag.
 * Game mouse sprite, input polling and hit-testing are permanently off
 * for keyboard and controller users; legacy MouseEnabled=1 is ignored.
 * This is never a Windows/Linux desktop mouse or keyboard input hook.
 */
int SpiderPS1Mouse_Arm(void (*logger)(const char *));
void SpiderPS1Mouse_Configure(void (*logger)(const char *));
void SpiderPS1Mouse_OnFlip(HWND game_hwnd);
#endif
