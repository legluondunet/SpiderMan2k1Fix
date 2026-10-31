#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPLAY_PLAN_PROBE_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPLAY_PLAN_PROBE_H
#include <windows.h>
void SpiderSDL3GameplayPlan_Init(const char *ini,void (*logger)(const char *));
/* Called only after SDL3 pump + native owner reconcile on original Flip
 * thread. Strictly read-only; does not replace original game controls. */
void SpiderSDL3GameplayPlan_OnFlip(HWND game_hwnd,int menu_2d,
                                  int gameplay_3d,int bink_active,
                                  int native_pause_active);
#endif
