/* Spider-Man 2001: retired native DirectInput joystick rebind.
 *
 * SDL3-mapped gamepads are the only supported controller backend.
 * The universal SDL3 adapter always claims explicit, neutral-first input,
 * with the game's original joystick pointer temporarily isolated.
 *
 * Keep ABI-compatible, inert entry points while removing native COM
 * EnumDevices/CreateDevice/Acquire/rebind, manufacturer-specific guesses
 * and the original joystick button-table diagnostics. Callers still
 * compile and can be independently simplified in later small PRs.
 *
 * The original game's signature-gated DIERR_UNPLUGGED crash guard lives in
 * native_joystick_unplug.c and MUST NOT be removed with this module.
 * No-controller SDL3 state leaves original keyboard input available.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include "native_joystick_reconnect.h"

void SpiderNativeReconnect_Init(const char *ini,void (*logger)(const char *)) {
  (void)ini;
  if(logger)logger("NATIVE-RECONNECT: retired; SDL3 is the only supported controller mapping backend");
}
void SpiderNativeReconnect_OnFlip(long flip_index,HWND game_hwnd) {
  (void)flip_index;(void)game_hwnd;
}
int SpiderNativeReconnect_SDL3SelectionReady(uint16_t vid,uint16_t pid) {
  (void)vid;(void)pid;
  return 0; /* Legacy native-owner modes cannot be reactivated. */
}
int SpiderNativeReconnect_CanonicalXboxMapping(void) {return 0;}
int SpiderNativeReconnect_CanonicalDS4Mapping(void) {return 0;}
void SpiderNativeReconnect_LogSyntheticMenuKey(unsigned scan,const char *phase) {
  (void)scan;(void)phase;
}
void SpiderNativeReconnect_ObserveMode(long flip_index,int menu_2d,
                                       int gameplay_3d,int bink_active) {
  (void)flip_index;(void)menu_2d;(void)gameplay_3d;(void)bink_active;
}
