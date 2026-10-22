#ifndef SPIDER_F9_TOAST_H
#define SPIDER_F9_TOAST_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

/* Called only after the GDI+ PNG encoder has successfully published its
 * final image filename. The worker publishes data; the game thread draws. */
void SpiderF9Toast_PublishSavedPath(const WCHAR *full_path);
/* Display a user-readable failure instead of a false saved confirmation. */
void SpiderF9Toast_PublishError(const WCHAR *reason);

/* A complete scene + notification is composed into a cached offscreen
 * presentation surface before the ONE blit to PRIMARY. Returns NULL when no
 * notice is active or the surface cannot be composed. Never modifies BACK. */
IDirectDrawSurface7 *SpiderF9Toast_CompositeFrame(IDirectDrawSurface7 *back,
                                  HWND window,DWORD width,DWORD height);
void SpiderF9Toast_ResetPresentation(void);
/* Register the game window for no-Flip still-frame fallback. The normal
 * DirectDraw compositor remains the default presentation path. */
void SpiderF9Toast_SetGameWindow(HWND hwnd);
/* Thread-safe publication and FIRST actual onscreen presentation counters;
 * logs can diagnose static scenes with no Flip without altering game input. */
void SpiderF9Toast_GetDisplayStatus(DWORD *published,DWORD *displayed);

#endif
