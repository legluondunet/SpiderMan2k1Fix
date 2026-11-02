#ifndef SPIDERMAN2K1FIX_SDL3_UNIVERSAL_GAMEPLAY_H
#define SPIDERMAN2K1FIX_SDL3_UNIVERSAL_GAMEPLAY_H
#include <windows.h>
void SpiderSDL3Universal_Init(const char *ini,void (*logger)(const char *));
/* Requested opt-in: legacy DS4/Xbox keyboard bridges must not double
 * inject while this alternate generic SDL3 input architecture is active. */
int SpiderSDL3Universal_Enabled(void);
/* True while the game-owned native joystick pointer is safely suspended,
 * so native reconnection cannot race against its temporary NULL slot. */
int SpiderSDL3Universal_SuppressesNative(void);
/* Before UI/focus pause, releases held synthetic keys on any lost focus. */
void SpiderSDL3Universal_BeforeUI(HWND hwnd);
/* Before the joystick-setup modal blocks the original Flip thread, restore
 * game-owned DirectInput and release every synthetic held key. */
void SpiderSDL3Universal_BeforeModal(HWND game_hwnd);
/* At normal Flip boundary, after native reconnect and SDL3 polling. */
void SpiderSDL3Universal_OnFlip(HWND hwnd,int menu,int gameplay,
                                int bink,int native_pause);
#endif
