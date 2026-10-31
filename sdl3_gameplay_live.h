#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPLAY_LIVE_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPLAY_LIVE_H
#include <windows.h>
void SpiderSDL3GameplayLive_Init(const char *ini,void (*logger)(const char *));
void SpiderSDL3GameplayLive_OnFlip(HWND hwnd,int menu2d,int gameplay3d,
                                   int bink,int native_pause);
int SpiderSDL3GameplayLive_OwnsPause(void);
#endif
