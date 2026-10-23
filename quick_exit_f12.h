#ifndef SPIDERMAN2K1FIX_QUICK_EXIT_F12_H
#define SPIDERMAN2K1FIX_QUICK_EXIT_F12_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Experimental opt-in native MENU Quit dispatch.
 * The F12 worker never calls the game's cleanup or touches its Quit flag.
 * The native menu command dispatcher selects action 18 on the game thread.
 * In other scenes the request times out and does not alter game state. */
void SpiderQuickExitF12_Init(void (*log_fn)(const char *));
void SpiderQuickExitF12_AttachWindow(HWND game_window);
int SpiderQuickExitF12_Enabled(void);
void SpiderQuickExitF12_ObserveMovieFrame(void);
/* Rendering diagnostic only: this is NOT a native comic-scene owner. */
void SpiderQuickExitF12_ObserveComicRender(LONG flip_index, DWORD caller_rva,
                                          DWORD texture_w, DWORD texture_h);
/* Read-only engine-mode snapshots at the existing DirectDraw Flip site. */
void SpiderQuickExitF12_ObserveEngineState(LONG flip_index, LONG renderer_menu);

#endif
