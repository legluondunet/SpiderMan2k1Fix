#ifndef SPIDER_F9_TOAST_H
#define SPIDER_F9_TOAST_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

/* Called only after the GDI+ PNG encoder has successfully published its
 * final image filename. The worker publishes data; the game thread draws. */
void SpiderF9Toast_PublishSavedPath(const WCHAR *full_path);

/* The caller must invoke this AFTER the backbuffer has been Blt'ed to the
 * windowed primary, so the confirmation can NEVER enter the PNG readback. */
BOOL SpiderF9Toast_DrawAfterPresent(IDirectDrawSurface7 *primary,HWND window,
                                     DWORD width,DWORD height);

#endif
