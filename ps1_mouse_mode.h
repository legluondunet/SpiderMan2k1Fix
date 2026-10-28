#ifndef SPIDERMAN2K1FIX_PS1_MOUSE_MODE_H
#define SPIDERMAN2K1FIX_PS1_MOUSE_MODE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/*
 * Original retail game mouse-disable flag, not the Win32 desktop cursor.
 * Requires exact original mouse instructions and a writable native flag.
 * MouseEnabled=0 (default) suppresses only IN-GAME mouse menu actions for
 * keyboard and any controller; MouseEnabled=1 restores original mouse menus.
 * This is never a Windows/Linux desktop mouse or keyboard input hook.
 */
int SpiderPS1Mouse_Arm(void (*logger)(const char *));
void SpiderPS1Mouse_SetPadActive(int active); /* internal owned-bit update */
void SpiderPS1Mouse_Configure(int mouse_enabled,void (*logger)(const char *));
void SpiderPS1Mouse_OnFlip(HWND game_hwnd);
#endif
