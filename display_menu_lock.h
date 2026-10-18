#ifndef SPIDER_DISPLAY_MENU_LOCK_H
#define SPIDER_DISPLAY_MENU_LOCK_H
#include <windows.h>
/* Verified SpideyPC.exe only. Patch only menu display labels and handling. */
BOOL SpiderDisplayMenu_Install(const char *resolution_setting,
                              const char *aspect_ratio_setting,
                              void (*log_fn)(const char *));
/* Selected physical output after the actual monitor/window probe. */
void SpiderDisplayMenu_SetTarget(DWORD width, DWORD height);
#endif