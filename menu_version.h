#ifndef SPIDERMAN2K1FIX_MENU_VERSION_H
#define SPIDERMAN2K1FIX_MENU_VERSION_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

/* Caller must restrict these functions to the original menu stage. */
BOOL SpiderMenuVersion_DrawToSurface(IDirectDrawSurface7 *surface, const RECT *content);
BOOL SpiderMenuVersion_DrawToWindow(HWND window, const RECT *content);

#endif
