/* One-shot first-game-launch logging policy for unified Windows/Wine builds.
 *
 * An INI not yet created by the game is the only auto-log trigger. The
 * generated INI retains [Debug] Log=0, so subsequent sessions are quiet.
 * An existing INI -- including an explicit user Log=0 or Log=1 -- is NEVER
 * rewritten for diagnostics. The installer log remains independent.
 * No extra marker is added to state/ (only two SHA receipts belong there).
 */
#ifndef SPIDER_FIRST_RUN_LOG_POLICY_H
#define SPIDER_FIRST_RUN_LOG_POLICY_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static inline int SpiderFirstRunLog_IniMissingA(const char *ini) {
  DWORD attr;
  if(!ini || !*ini)return 0;
  attr=GetFileAttributesA(ini);
  return attr==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND;
}
static inline int SpiderFirstRunLog_IniMissingW(const wchar_t *ini) {
  DWORD attr;
  if(!ini || !*ini)return 0;
  attr=GetFileAttributesW(ini);
  return attr==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND;
}
#endif
