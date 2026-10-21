#ifndef SPIDERMAN2K1FIX_PAUSE_SURFACE_H
#define SPIDERMAN2K1FIX_PAUSE_SURFACE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

/* No HWNDs, child surfaces or desktop-owned alpha popups.
 * Paint ONE pause frame through the game's existing DirectDraw backbuffer,
 * present it once via the game's already verified PRIMARY Blt, and restore
 * the unchanged rendered backbuffer before allowing the normal Flip.
 * Any unsupported DirectDraw/GDI combination fails closed. */
typedef HRESULT (WINAPI *SpiderPauseBltFn)(
  void *,LPRECT,IDirectDrawSurface7 *,LPRECT,DWORD,LPDDBLTFX);
void SpiderPauseSurface_Init(void (*log_fn)(const char *));
int SpiderPauseSurface_Begin(IDirectDrawSurface7 *back,
                             IDirectDrawSurface7 *primary,
                             const RECT *primary_dest,SpiderPauseBltFn present);
void SpiderPauseSurface_End(void);
#endif
