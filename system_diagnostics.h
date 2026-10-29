#ifndef SPIDERMAN2K1FIX_SYSTEM_DIAGNOSTICS_H
#define SPIDERMAN2K1FIX_SYSTEM_DIAGNOSTICS_H

/* Passive one-shot startup system inventory, only when [Debug] Log=1.
 * No shell commands, WMI workers, game-input acquisition or EXE writes.
 * The callback is the game's existing logger; NULL disables the inventory.
 */
void SpiderSystemDiagnostics_Log(void (*logger)(const char *));

#endif
