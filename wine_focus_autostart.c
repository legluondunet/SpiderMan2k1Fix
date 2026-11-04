/* Wine-specific autostart of the separately packaged native X11 observer.
 * Only uses dynamically looked-up Wine exports. No Unixlib-internal ABI,
 * external shell, xdotool, Win32 process injection or DllMain work.
 *
 * GPL-3.0-or-later; legluondunet assisted by ChatGPT (OpenAI).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "wine_focus_autostart.h"

/* kernel32: Wine-only, cdecl; returned buffer belongs to Wine process heap.
 * ntdll: Wine-only stdcall, launches an ELF child without waiting. */
typedef char * (__cdecl *wine_to_unix_fn)(const WCHAR *);
typedef LONG (WINAPI *wine_spawn_fn)(char * const argv[], int wait);

int SpiderWineFocusAutostart_TryStart(void (*logger)(const char *)) {
  HMODULE kernel=GetModuleHandleA("kernel32.dll");
  HMODULE ntdll=GetModuleHandleA("ntdll.dll");
  wine_to_unix_fn to_unix;
  wine_spawn_fn spawn;
  WCHAR game_path[MAX_PATH];
  WCHAR helper_path[MAX_PATH];
  WCHAR *slash;
  DWORD n;
  DWORD requested;
  char existing[4]={0};
  char *game_unix=NULL,*helper_unix=NULL;
  char *argv[3];
  LONG status;
  int result=0;
  requested=GetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE",
                                      existing,sizeof(existing));
  /* =0: disable integrated bridge. =1: preserve validated manual helper. */
  if(requested || GetLastError()!=ERROR_ENVVAR_NOT_FOUND)return 0;
  if(!kernel || !ntdll) return 0;
  /* Fail closed on Windows and on Wine versions without the exact API. */
  if(!GetProcAddress(ntdll,"wine_get_version"))return 0;
  to_unix=(wine_to_unix_fn)GetProcAddress(kernel,"wine_get_unix_file_name");
  spawn=(wine_spawn_fn)GetProcAddress(ntdll,"__wine_unix_spawnvp");
  if(!to_unix || !spawn) {
    if(logger)logger("FOCUS-X11-AUTO: Wine Unix path/spawn API absent; using native Win32 focus");
    return 0;
  }
  n=GetModuleFileNameW(NULL,game_path,MAX_PATH);
  if(!n || n>=MAX_PATH) return 0;
  slash=wcsrchr(game_path,L'\\');
  if(!slash) return 0;
  /* The installation layout matches the existing patch DLL folder. */
  *slash=0;
  {
    int size=swprintf(helper_path,MAX_PATH,
                      L"%ls\\SpiderMan2k1Fix\\linux\\spiderman-focus-helper",
                      game_path);
    if(size<0 || size>=MAX_PATH)return 0;
  }
  if(GetFileAttributesW(helper_path)==INVALID_FILE_ATTRIBUTES) {
    if(logger)logger("FOCUS-X11-AUTO: Linux helper not packaged; Win32 focus remains active");
    return 0;
  }
  /* Wine conversion requires an existing host-side game EXE and ELF. */
  *slash=L'\\';
  helper_unix=to_unix(helper_path);
  game_unix=to_unix(game_path);
  if(!helper_unix || !game_unix) {
    if(logger)logger("FOCUS-X11-AUTO: Unix path conversion failed; Win32 fallback");
    goto cleanup;
  }
  argv[0]=helper_unix;
  argv[1]=game_unix;
  argv[2]=NULL;
  /* Wine ntdll double-forks for wait=0; no subprocess is left as our
   * Wine message-pump child. Pass exact argv, never a shell command. */
  status=spawn(argv,0);
  if(status==0) {
    /* Set BEFORE pause_at_flip_if_background() reads the environment
     * later on this same game Flip thread. No Windows env changes. */
    if(SetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE","1")) {
      result=1;
      if(logger)logger("FOCUS-X11-AUTO: native X11 helper spawned automatically; checking fresh state on Flip");
    } else if(logger)
      logger("FOCUS-X11-AUTO: helper started but bridge environment not set; Win32 focus fallback");
  } else if(logger) {
    char line[160];
    snprintf(line,sizeof(line),
             "FOCUS-X11-AUTO: helper spawn failed NTSTATUS=0x%08lX; Win32 fallback",
             (unsigned long)(DWORD)status);
    logger(line);
  }
cleanup:
  if(game_unix)HeapFree(GetProcessHeap(),0,game_unix);
  if(helper_unix)HeapFree(GetProcessHeap(),0,helper_unix);
  return result;
}
