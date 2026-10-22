#ifndef SPIDERMAN2K1FIX_F9_PNG_H
#define SPIDERMAN2K1FIX_F9_PNG_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

/* Called while the game has successfully locked the real D3D7 backbuffer.
 * Copies a full 32-bit XRGB frame before the caller unlocks it; the PNG
 * encoding runs on an independent worker with no DirectDraw pointers.
 * Returns 1 if queued, 0 if skipped/failed. Never takes ownership of
 * the locked surface or changes its pixels. */
int SpiderF9PNG_Queue(const DDSURFACEDESC2 *frame,void (*log_fn)(const char *));

#endif
