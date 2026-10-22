#ifndef SPIDERMAN2K1FIX_QUICK_EXIT_F12_H
#define SPIDERMAN2K1FIX_QUICK_EXIT_F12_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Opt-in, no input injection or DirectInput hook. Must be initialized after
 * verified EXE and validated/generated SpiderMan2k1Fix.ini.
 * WM_CLOSE is delivered to the game's own window/message loop, leaving its
 * normal window and subsystem teardown in control of the process exit. */
void SpiderQuickExitF12_Init(void (*log_fn)(const char *));
void SpiderQuickExitF12_AttachWindow(HWND game_window);
int SpiderQuickExitF12_Enabled(void);

#endif
