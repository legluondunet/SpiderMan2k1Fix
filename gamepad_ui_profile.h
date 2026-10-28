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
int SpiderGamepadUI_PrepareXbox360(HWND game_window);
void SpiderGamepadUI_ActivateXbox360(void);
/* Custom Alt+Tab PAUSED screen only: foreground, already-acquired DS4;
 * a fresh digital button press+release wakes without keyboard synthesis.
 * No effect on native 3D Escape pause or original joystick bindings. */
int SpiderGamepadUI_PauseWake_Begin(void); /* 1=already detected DS4 */
unsigned SpiderGamepadUI_PauseWake_Poll(HWND game_window);
void SpiderGamepadUI_PauseWake_End(void);
/* Strictly opt-in, only after one Sony DS4 and native profile bootstrap. */
int SpiderPS1_InstallReadOnlyJoystick(void (*logger)(const char *));
void SpiderGamepadUI_OnFlip(HWND game_window,int menu_2d,int gameplay_3d,
                             int native_bink_active,int native_pause_active);
#endif
