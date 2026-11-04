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
/* The X11 host observer can see Firefox/other Linux windows while Wine
 * GetForegroundWindow() still claims the game HWND. Called on the game's
 * Flip thread with verified native focus (blocked=1 for OTHER/UNKNOWN);
 * flush the selected universal owner's synthetic keys before PAUSED and
 * forbid a new 3D owner until native GAME focus returns. */
void SpiderSDL3Universal_SetHostFocusBlocked(int blocked);
/* Called after the host X11 or Win32 focus-loss detector has LATCHED pause,
 * before blocking the game Flip thread. GetForegroundWindow() can stay
 * incorrectly on the game under Wine/X11; do not trust it here. */
void SpiderSDL3Universal_OnConfirmedFocusPause(void);
/* Before the joystick-setup modal blocks the original Flip thread, restore
 * game-owned DirectInput and release every synthetic held key. */
void SpiderSDL3Universal_BeforeModal(HWND game_hwnd);
/* At normal Flip boundary, after native reconnect and SDL3 polling. */
void SpiderSDL3Universal_OnFlip(HWND hwnd,int menu,int gameplay,
                                int bink,int native_pause);
#endif
