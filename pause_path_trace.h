#ifndef SPIDERMAN2K1FIX_PAUSE_PATH_TRACE_H
#define SPIDERMAN2K1FIX_PAUSE_PATH_TRACE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Passive diagnostic only. Enabled by environment variable and Debug Log=1.
 * Never changes focus, calls game input APIs or waits in a render path. */
void SpiderPausePath_Init(void (*logger)(const char *),int debug_enabled);
void SpiderPausePath_Record(const char *api,HWND hwnd,LONG flip,int menu,
                            const char *surface_role);
DWORD SpiderPausePath_FlipEnter(HWND hwnd,LONG flip,int menu);
void SpiderPausePath_FlipLeave(HWND hwnd,DWORD entered,LONG flip,int menu);
#endif
