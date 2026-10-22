#ifndef SPIDERMAN2K1FIX_QUICK_EXIT_F12_H
#define SPIDERMAN2K1FIX_QUICK_EXIT_F12_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Opt-in, no input injection or DirectInput hook. Must be initialized after
 * verified EXE and validated/generated SpiderMan2k1Fix.ini.
 * F12 sets the game's verified native Quit byte, allowing the original
 * engine to run its own Bink/media, DirectDraw/DirectSound and WinMain
 * cleanup; never destroy just the window with WM_CLOSE. */
void SpiderQuickExitF12_Init(void (*log_fn)(const char *));
void SpiderQuickExitF12_AttachWindow(HWND game_window);
int SpiderQuickExitF12_Enabled(void);
/* Passive: reports a successfully rendered FMV frame to the shutdown guard. */
void SpiderQuickExitF12_ObserveMovieFrame(void);

#endif
