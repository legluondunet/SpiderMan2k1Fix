#ifndef SPIDERMAN2K1FIX_QUICK_EXIT_F12_H
#define SPIDERMAN2K1FIX_QUICK_EXIT_F12_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Optional F10 development shortcut, with guarded original-game movie,
 * script and menu transitions. The first-3D Enter/Space key-up safety
 * remains active with F10 disabled. Historical F12 function identifiers
 * are retained for compatibility with the existing game hooks/tests. */
void SpiderQuickExitF12_Init(void (*log_fn)(const char *));
void SpiderQuickExitF12_AttachWindow(HWND game_window);
int SpiderQuickExitF12_Enabled(void);
void SpiderQuickExitF12_ObserveMovieFrame(void);
/* Rendering diagnostic only: this is NOT a native comic-scene owner. */
void SpiderQuickExitF12_ObserveComicRender(LONG flip_index, DWORD caller_rva,
                                          DWORD texture_w, DWORD texture_h);
/* Read-only engine-mode snapshots at the existing DirectDraw Flip site. */
void SpiderQuickExitF12_ObserveEngineState(LONG flip_index, LONG renderer_menu);
/* Optional READ-ONLY SDL3 physical South/West cross-reference. This is
 * published on the normal Flip thread only after the selected gamepad's
 * original DirectInput ownership has been verified. Other contexts reset
 * the sample. Never synthesizes game input or modifies Jump. */
void SpiderQuickExitF12_CutscenePhysicalSample(
    unsigned instance,unsigned vendor,unsigned product,
    unsigned physical_buttons,int selected_owner_valid);


#endif
