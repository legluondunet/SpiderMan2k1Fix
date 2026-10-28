#ifndef VIRTUAL_SPIDEY_CFG_H
#define VIRTUAL_SPIDEY_CFG_H
#include <windows.h>
/* Installs always-on, exe-IAT-only file/stdio interception after EXE SHA verification.
 * Return the number of patched imports; zero means the game may use an alternate IO path.
 */
/* Return nonzero if the INI exists or was created. Never overwrite an existing one. */
int SpiderCfg_EnsureDefaultIni(void (*logger)(const char *));
void SpiderCfg_ValidateCurrentIni(void (*logger)(const char *));
unsigned SpiderCfg_Install(const char *ini_path,void (*logger)(const char *));
void SpiderCfg_ObserveNativeMonitor(unsigned width,unsigned height);
/* One-time game-thread reload after verifying a real virtual CRT read. */
void SpiderCfg_BootstrapGameRead(void);
/* Opt-in RAM-only PS1 gameplay mapping after verified DS4 acquisition.
 * Never writes the overlay into the original user INI. */
int SpiderCfg_EnablePS1Preview(void);
/* True only after an authenticated game-owned loader consumed the overlay. */
int SpiderCfg_PS1BootstrapLoaded(void);
void SpiderCfg_CancelPS1Preview(void);
/* Xbox 360 gameplay candidate: verified single pad + read-only native menu;
 * overlay only RAM until authenticated game CRT bootstrap succeeds. */
int SpiderCfg_EnableXbox360Preview(void);
int SpiderCfg_Xbox360BootstrapLoaded(void);
void SpiderCfg_CancelXbox360Preview(void);
#endif