/* SDL3 is the only SpiderMan2k1Fix controller mapping backend.
 * Retire pre-SDL3 Sony/Xbox physical button-index presets, native
 * action-map overlays, and competing synthetic UI key bridges.
 * The original game's joystick defaults remain untouched when the
 * experimental universal adapter is not explicitly enabled.
 * Keep the permanent game-only mouse policy and SDL3 setup entry.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamepad_ui_profile.h"
#include "ps1_mouse_mode.h"
#include "sdl3_controller_setup.h"
#include "ps1_joystick_readonly_menu.h"

static void (*ui_log)(const char *)=NULL;
void SpiderGamepadUI_Init(const char *ini,void (*logger)(const char *)) {
  ui_log=logger;
  SpiderPS1Mouse_Configure(logger);
  SpiderSDL3ControllerSetup_Init(ini,logger);
  if(SpiderSDL3ControllerSetup_Enabled())
    SpiderSDL3_InstallDirectJoystickEntry(logger);
  if(logger)logger("GAMEPAD-UI: retired pre-SDL3 Xbox/DS4 custom maps and synthetic keys; SDL3 per-model profile is the only patch-provided mapping");
}
void SpiderGamepadUI_OnFlip(HWND hwnd,int menu_2d,int gameplay_3d,
                            int bink_active,int native_pause_active) {
  (void)menu_2d;(void)gameplay_3d;
  (void)bink_active;(void)native_pause_active;
  SpiderPS1Mouse_OnFlip(hwnd);
  if(SpiderSDL3ControllerSetup_Enabled())
    SpiderSDL3_InstallDirectJoystickEntry(ui_log);
}
void SpiderGamepadUI_AfterControllerSetup(HWND hwnd) {(void)hwnd;}
/* The engine's SDL3 focus wake and keyboard release are independent
 * from the old Sony-only DirectInput pause helper. */
int SpiderGamepadUI_PauseWake_Begin(void) {return 0;}
unsigned SpiderGamepadUI_PauseWake_Poll(HWND hwnd) {(void)hwnd;return 0;}
void SpiderGamepadUI_PauseWake_End(void) {}
int SpiderGamepadUI_ReleaseSyntheticReturnBefore3D(HWND hwnd) {
  (void)hwnd;return 0;
}
void SpiderGamepadUI_SDL3GameplayPause(HWND hwnd) {(void)hwnd;}
