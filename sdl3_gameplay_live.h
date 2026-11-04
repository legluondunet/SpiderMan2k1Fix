#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPLAY_LIVE_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPLAY_LIVE_H
#include <windows.h>
/* Retired transitional physical Xbox/DS4 mapping. No-op ABI shim
 * for existing passive trace callers; no native input-table writes. */
void SpiderSDL3GameplayLive_Init(const char *ini,void (*logger)(const char *));
void SpiderSDL3GameplayLive_PreNativeOnFlip(HWND hwnd);
void SpiderSDL3GameplayLive_OnFlip(HWND hwnd,int menu,int gameplay,
                                  int bink,int pause);
int SpiderSDL3GameplayLive_OwnsPause(void);
int SpiderSDL3GameplayLive_VerifiedXboxSwingMap(void);
#endif
