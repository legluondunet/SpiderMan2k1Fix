/* SDL3 game-thread input diagnostics retained after retiring the old
 * DirectInput-native controller ownership experiment.
 *
 * The former optional 3D plan required a game-owned DirectInput joystick
 * that no longer exists as a supported controller backend. Its physical
 * rebind checks and native action-trace path were unreachable with the
 * mandatory universal SDL3 adapter. Keep the independent read-only
 * native-latch/source observers and the SDL3 live lifecycle shim. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "sdl3_gameplay_plan_probe.h"
#include "sdl3_gameplay_live.h"
#include "sdl3_native_latch_trace.h"
#include "sdl3_native_source_trace.h"

void SpiderSDL3GameplayPlan_Init(const char *ini,void (*logger)(const char *)) {
  spider_latch_trace_init(ini,logger);
  spider_source_trace_init(ini,logger);
  SpiderSDL3GameplayLive_Init(ini,logger);
}
void SpiderSDL3GameplayPlan_OnFlip(HWND hwnd,int menu2d,int gameplay3d,
                                  int bink,int pause) {
  SpiderSDL3GameplayLive_OnFlip(hwnd,menu2d,gameplay3d,bink,pause);
  spider_latch_trace_flip(hwnd,menu2d,gameplay3d,bink,pause);
  spider_source_trace_flip(hwnd,menu2d,gameplay3d,bink,pause);
}
