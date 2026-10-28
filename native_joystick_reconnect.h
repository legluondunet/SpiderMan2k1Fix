#ifndef SPIDERMAN2K1FIX_NATIVE_JOYSTICK_RECONNECT_H
#define SPIDERMAN2K1FIX_NATIVE_JOYSTICK_RECONNECT_H

/* Stage 2, fully opt-in: on the native DirectInput render/Flip thread,
 * rebind a newly attached Sony DS4 to the game's ORIGINAL joystick pointer.
 * Depends on the independently validated stage-1 DIERR_UNPLUGGED guard.
 * Does not create another game-wide DirectInput factory or hook COM vtables. */
#include <windows.h>
void SpiderNativeReconnect_Init(const char *ini,void (*logger)(const char *));
void SpiderNativeReconnect_OnFlip(long flip_index, HWND game_hwnd);
/* Read-only, opt-in verified original Xbox action table after native hot swap. */
int SpiderNativeReconnect_CanonicalXboxMapping(void);
/* Passive post-rebind input breadcrumbs; never alters gameplay state. */
void SpiderNativeReconnect_LogSyntheticMenuKey(unsigned scan, const char *phase);
/* Read-only original joystick pointer + render-mode health after rebind. */
void SpiderNativeReconnect_ObserveMode(long flip_index, int menu_2d,
                                       int gameplay_3d, int bink_active);
#endif
