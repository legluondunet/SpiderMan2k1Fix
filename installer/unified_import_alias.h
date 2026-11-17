/* Experimental in-memory bootstrap for the exact supported retail PE32.
 * All operations require a CREATE_SUSPENDED process whose only thread has
 * not been resumed. The caller MUST verify the executable's complete disk
 * SHA-256 and terminate the child if any step fails.
 * No on-disk original-game file is modified.
 */
#ifndef SPIDERMAN_UNIFIED_IMPORT_ALIAS_H
#define SPIDERMAN_UNIFIED_IMPORT_ALIAS_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Find only the original game's one DirectDrawCreateEx import and replace
 * its imported DLL name with the same-length "fix.dll" alias in child memory.
 * The original name's case may vary; total byte count MUST be ten. */
int SpiderUnified_AliasSuspendedDDraw(HANDLE process);

/* Apply the original game's existing signature-gated No-CD patch while the
 * game is suspended, before its original entry point can execute. */
int SpiderUnified_PatchSuspendedNoCD(HANDLE process);

/* Prevent the original native IDirectSoundBuffer::Stop NULL dereference
 * during emergency cleanup; refuses unrecognized original code/cave. */
int SpiderUnified_PatchSuspendedPrimaryNullStop(HANDLE process);
#endif
