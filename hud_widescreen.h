#ifndef SPIDERMANFIX_HUD_WIDESCREEN_H
#define SPIDERMANFIX_HUD_WIDESCREEN_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Only for the previously SHA-256-verified 32-bit SpideyPC.exe.
 * Call once, after the executable verification and configuration load,
 * before any HUD rendering takes place.
 *
 * image_verified MUST come from the existing strict SHA-256 check.
 * config_enabled is the HUDWidescreenFix=0/1 setting.
 *
 * Returns TRUE when disabled or successfully installed, FALSE on failure.
 * Never uninstall hooks while gameplay is active.
 */
BOOL SpiderHudWidescreen_Install(BOOL image_verified, BOOL config_enabled);
BOOL SpiderHudWidescreen_Installed(void);
/* Passive HUD *classification* only: instrument the seven SHA-pinned
 * text/bar/rectangle scope CALL sites without changing any coordinates.
 * Needed to exempt UI draw calls from experimental Hor+ world projection.
 * Returns FALSE and leaves rendering unchanged if signatures do not match.
 */
BOOL SpiderHudWidescreen_InstallScopesOnly(BOOL image_verified);
/* TRUE only while the current game thread is inside an identified HUD scope. */
BOOL SpiderHudWidescreen_InScope(void);
/* Read-only scope entry counter for the experimental HUD pipeline trace. */
LONG SpiderHudWidescreen_TakeScopeEntryCount(void);

/* Optional call from the host DLL's DLL_THREAD_DETACH notification. */
void SpiderHudWidescreen_ThreadDetach(void);

#ifdef __cplusplus
}
#endif
#endif