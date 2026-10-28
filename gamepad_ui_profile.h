#ifndef SPIDERMAN2K1FIX_GAMEPAD_UI_PROFILE_H
#define SPIDERMAN2K1FIX_GAMEPAD_UI_PROFILE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
 * 2D UI profile. Own DirectInput8 enumeration/polling is independent from
 * the original game's COM objects. Never touches native controller tables.
 * Only the verified game window receives scan-code pulses: Return in the
 * 2D menu OR a native Bink movie, Escape in 2D submenus, and optionally
 * Pause/Escape on OPTIONS in 3D only if no game action uses BUTTON9.
 * Never overwrite the seven original gameplay mappings or 3D triangle.
 *
 * Enter UI is user-validated under Wine; Bink-after-menu and 3D Pause
 * extensions are still experimental.
 */
void SpiderGamepadUI_Init(const char *ini,void (*logger)(const char *));
/* Preview must only activate AFTER the game's verified CRT re-read. */
int SpiderGamepadUI_PreparePS1(HWND game_window);
void SpiderGamepadUI_ActivatePS1(void);
void SpiderGamepadUI_OnFlip(HWND game_window,int menu_2d,int gameplay_3d,
                             int native_bink_active,int native_pause_active);
#endif
