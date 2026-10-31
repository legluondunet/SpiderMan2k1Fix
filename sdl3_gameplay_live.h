#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPLAY_LIVE_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPLAY_LIVE_H
#include <windows.h>
void SpiderSDL3GameplayLive_Init(const char *ini,void (*logger)(const char *));
/* Restore any active remap BEFORE native owner hot-rebind/swap if the
 * SDL3 selection, count, foreground or game-owned pointer changed. */
void SpiderSDL3GameplayLive_PreNativeOnFlip(HWND hwnd);
void SpiderSDL3GameplayLive_OnFlip(HWND hwnd,int menu2d,int gameplay3d,
                                   int bink,int native_pause);
int SpiderSDL3GameplayLive_OwnsPause(void);
#endif
