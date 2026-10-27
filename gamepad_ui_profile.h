#ifndef SPIDERMAN2K1FIX_GAMEPAD_UI_PROFILE_H
#define SPIDERMAN2K1FIX_GAMEPAD_UI_PROFILE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
 * 2D UI profile. Own DirectInput8 enumeration/polling is independent from
 * the original game's COM objects. Never touches native controller tables.
 * Only the verified game window receives a VK_RETURN scan-code pulse, and
 * only in the 2D menu/video context (never in 3D gameplay).
 *
 * Compatibility is experimental until Windows and Wine runtime validation.
 */
void SpiderGamepadUI_Init(const char *ini,void (*logger)(const char *));
void SpiderGamepadUI_OnFlip(HWND game_window,int menu_2d,int gameplay_3d);
#endif
