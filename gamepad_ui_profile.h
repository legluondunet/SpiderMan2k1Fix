#ifndef SPIDERMAN2K1FIX_GAMEPAD_UI_PROFILE_H
#define SPIDERMAN2K1FIX_GAMEPAD_UI_PROFILE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* SDL3 setup entry + game mouse policy only.
 * Legacy pre-SDL3 Sony/Xbox DirectInput mapping has been removed. */
void SpiderGamepadUI_Init(const char *ini,void (*logger)(const char *));
void SpiderGamepadUI_OnFlip(HWND hwnd,int menu_2d,int gameplay_3d,
                            int bink_active,int native_pause_active);
void SpiderGamepadUI_AfterControllerSetup(HWND game_window);
int SpiderGamepadUI_ReleaseSyntheticReturnBefore3D(HWND hwnd);
void SpiderGamepadUI_SDL3GameplayPause(HWND hwnd);
#endif
