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

/* Activated only after a fresh, well-formed state from the launched ELF.
 * A successful POSIX spawn alone is NOT proof that X11 focus works:
 * on Wayland, or if libX11/glibc is absent, the helper can exit instantly. */
static int helper_waiting_for_state=0;
static DWORD helper_spawn_tick=0;
static DWORD helper_probe_tick=0;
static FILETIME helper_spawn_filetime;
static WCHAR helper_state_path[MAX_PATH];

int SpiderWineFocusAutostart_PollReady(void (*logger)(const char *)) {
  DWORD tick=GetTickCount(),read=0;
  WIN32_FILE_ATTRIBUTE_DATA attr;
  FILETIME now_ft;
  ULARGE_INTEGER started,modified,now;
  HANDLE file;
  char state[64]={0};
  int valid=0;
  if(!helper_waiting_for_state)return 0;
  if((DWORD)(tick-helper_spawn_tick)>4000u) {
    helper_waiting_for_state=0;
    if(logger)logger("FOCUS-X11-AUTO: no verified X11 state in 4s; Win32 fallback");
    return 0;
  }
  if(helper_probe_tick && (DWORD)(tick-helper_probe_tick)<100u)return 0;
  helper_probe_tick=tick;
  if(!GetFileAttributesExW(helper_state_path,GetFileExInfoStandard,&attr))return 0;
  GetSystemTimeAsFileTime(&now_ft);
  started.LowPart=helper_spawn_filetime.dwLowDateTime;
  started.HighPart=helper_spawn_filetime.dwHighDateTime;
  modified.LowPart=attr.ftLastWriteTime.dwLowDateTime;
  modified.HighPart=attr.ftLastWriteTime.dwHighDateTime;
  now.LowPart=now_ft.dwLowDateTime;
  now.HighPart=now_ft.dwHighDateTime;
  if(modified.QuadPart<started.QuadPart || now.QuadPart<modified.QuadPart ||
     now.QuadPart-modified.QuadPart>1200ull*10000ull)return 0;
  file=CreateFileW(helper_state_path,GENERIC_READ,
                   FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                   NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE)return 0;
  if(ReadFile(file,state,sizeof(state)-1,&read,NULL) && read) {
    state[read]=0;
    valid=!strcmp(state,"SPIDEY_X11_FOCUS_V1 GAME\n") ||
          !strcmp(state,"SPIDEY_X11_FOCUS_V1 OTHER\n");
  }
  CloseHandle(file);
  if(!valid)return 0;
  helper_waiting_for_state=0;
  if(!SetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE","1")) {
    if(logger)logger("FOCUS-X11-AUTO: could not activate verified X11 bridge; Win32 fallback");
    return 0;
  }
  if(logger)logger("FOCUS-X11-AUTO: fresh native X11 state verified; automatic bridge active");
  return 1;
}

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
  DWORD session_len;
  char session[32]={0};
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
  /* XWayland focus is not authoritative for native Wayland windows.
   * Conservatively disable the X11 backend for Wayland sessions. */
  session_len=GetEnvironmentVariableA("XDG_SESSION_TYPE",session,sizeof(session));
  if((session_len && session_len<sizeof(session) &&
      lstrcmpiA(session,"wayland")==0) ||
     ((!session_len || session_len>=sizeof(session)) &&
      GetEnvironmentVariableA("WAYLAND_DISPLAY",NULL,0)>0)) {
    if(logger)logger("FOCUS-X11-AUTO: Wayland session; X11 backend disabled, Win32 fallback");
    return 0;
  }
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
    int state_size=swprintf(helper_state_path,MAX_PATH,
                           L"%ls\\SpiderMan2k1Fix.x11-focus",game_path);
    if(size<0 || size>=MAX_PATH || state_size<0 || state_size>=MAX_PATH)
      return 0;
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
  /* GitHub Actions artifacts strip executable file permissions.
   * Restore u+x on the shipped ELF using argv (NO shell injection);
   * a read-only/game directory on which chmod fails is a safe fallback. */
  {
    char *chmod_argv[5]={"/usr/bin/chmod","u+x","--",helper_unix,NULL};
    status=spawn(chmod_argv,1);
    if(status!=0) {
      if(logger)logger("FOCUS-X11-AUTO: helper chmod failed; Win32 focus fallback");
      goto cleanup;
    }
  }
  argv[0]=helper_unix;
  argv[1]=game_unix;
  argv[2]=NULL;
  /* Wine ntdll double-forks for wait=0; no subprocess is left as our
   * Wine message-pump child. Pass exact argv, never a shell command. */
  GetSystemTimeAsFileTime(&helper_spawn_filetime);
  status=spawn(argv,0);
  if(status==0) {
    helper_waiting_for_state=1;
    helper_spawn_tick=GetTickCount();
    helper_probe_tick=0;
    result=1;
    if(logger)logger("FOCUS-X11-AUTO: native X11 helper spawned automatically; checking fresh state on Flip");
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
