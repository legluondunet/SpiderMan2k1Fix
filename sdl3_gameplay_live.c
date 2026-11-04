/* The former SDL3-to-native-DirectInput physical-button adapter is
 * retired: its exact Xbox/DS4 button indices predated the universal
 * keyboard bridge and competed with SDL3 per-model configuration.
 * All gameplay mapping is now in sdl3_universal_gameplay.c.
 * Keep these inert entries while passive 3D diagnostics still link
 * to the old module name; they NEVER modify the game-owned action table.
 */
#include "sdl3_gameplay_live.h"
void SpiderSDL3GameplayLive_Init(const char *ini,void (*log)(const char *)) {
  (void)ini;
  if(log)log("SDL3-3D-LIVE: old fixed native action-map backend retired; enable ExperimentalSDL3UniversalGameplay=1");
}
void SpiderSDL3GameplayLive_PreNativeOnFlip(HWND hwnd) {(void)hwnd;}
void SpiderSDL3GameplayLive_OnFlip(HWND hwnd,int menu,int gameplay,
                                   int bink,int pause) {
  (void)hwnd;(void)menu;(void)gameplay;(void)bink;(void)pause;
}
int SpiderSDL3GameplayLive_OwnsPause(void) {return 0;}
int SpiderSDL3GameplayLive_VerifiedXboxSwingMap(void) {return 0;}
