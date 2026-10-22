/* Optional F12 quick exit using the game's normal WM_CLOSE path.
 * A small polling thread is used instead of a DirectDraw Flip callback:
 * comics and opening stills can stop flipping for an arbitrary duration,
 * and DirectInput gameplay may not dispatch WM_KEYDOWN.
 * No TerminateProcess, ExitProcess, forced WM_QUIT, keyboard hooks, or
 * subclassing of the game's WndProc. No exit is triggered in the background.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "quick_exit_f12.h"

static volatile LONG enabled=0;
static volatile LONG closing=0;
static PVOID volatile game_hwnd=NULL;
static void (*quick_exit_log)(const char *)=NULL;

static void say(const char *msg) {
  if(quick_exit_log)quick_exit_log(msg);
}

int SpiderQuickExitF12_Enabled(void) {
  return InterlockedCompareExchange(&enabled,0,0)==1;
}

void SpiderQuickExitF12_AttachWindow(HWND hwnd) {
  InterlockedExchangePointer(&game_hwnd,(PVOID)hwnd);
}

static DWORD WINAPI quick_exit_f12_thread(LPVOID unused) {
  BOOL previous_down=TRUE; /* require a released F12 after startup */
  (void)unused;
  for(;;) {
    HWND hwnd=(HWND)InterlockedCompareExchangePointer(&game_hwnd,NULL,NULL);
    BOOL focused=hwnd && IsWindow(hwnd) && GetForegroundWindow()==hwnd;
    BOOL down=(GetAsyncKeyState(VK_F12)&0x8000)!=0;
    if(!focused) {
      /* Never trigger from Alt+Tab or on regaining focus while held. */
      previous_down=TRUE;
    } else if(!down) {
      previous_down=FALSE;
    } else if(!previous_down &&
              InterlockedCompareExchange(&closing,1,0)==0) {
      /* WM_CLOSE queues a request on the window's owning thread. This
       * preserves the game's ordinary destruction, DirectDraw/DirectSound
       * releases, and CRT cleanup instead of forcibly killing it. */
      if(PostMessageA(hwnd,WM_CLOSE,0,0)) {
        unsigned attempts;
        say("QUICK-EXIT-F12: F12 pressed in foreground; WM_CLOSE posted (normal shutdown)");
        /* Read-only check: did the game's own WndProc destroy the window?
         * If not, report it instead of killing the process or bypassing
         * a possible native prompt. Ending normally is the game's job. */
        for(attempts=0;attempts<100u;attempts++) {
          if(!IsWindow(hwnd)) {
            say("QUICK-EXIT-F12: game window destroyed after WM_CLOSE (normal path)");
            return 0;
          }
          Sleep(50);
        }
        say("QUICK-EXIT-F12: WARNING WM_CLOSE posted, but game window still exists after 5s; no forced termination");
        return 0; /* one request only, no repeated exit messages */
      }
      InterlockedExchange(&closing,0);
      say("QUICK-EXIT-F12: ERROR could not post WM_CLOSE; game left running");
      previous_down=TRUE;
    }
    Sleep(25);
  }
}

void SpiderQuickExitF12_Init(void (*log_fn)(const char *)) {
  char exe[MAX_PATH],ini[MAX_PATH],value[24],*slash;
  DWORD len;
  HANDLE thread;
  quick_exit_log=log_fn;
  len=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(!len || len>=sizeof(exe) || !(slash=strrchr(exe,'\\'))) {
    say("QUICK-EXIT-F12: disabled (cannot locate game INI)");
    return;
  }
  if((size_t)(slash-exe)+strlen("SpiderMan2k1Fix.ini")+2>sizeof(ini)) {
    say("QUICK-EXIT-F12: disabled (INI path too long)");
    return;
  }
  memcpy(ini,exe,(size_t)(slash-exe)+1);
  strcpy(ini+(slash-exe)+1,"SpiderMan2k1Fix.ini");
  /* Strict value: only the literal 1 enables this optional exit path.
   * Missing [System] or QuickExitF12, including older INIs, means off. */
  GetPrivateProfileStringA("System","QuickExitF12","0",value,sizeof(value),ini);
  if(strcmp(value,"1")!=0) {
    say("QUICK-EXIT-F12: disabled by default ([System] QuickExitF12=0)");
    return;
  }
  InterlockedExchange(&enabled,1);
  thread=CreateThread(NULL,0,quick_exit_f12_thread,NULL,0,NULL);
  if(!thread) {
    InterlockedExchange(&enabled,0);
    say("QUICK-EXIT-F12: ERROR thread creation failed; feature disabled");
    return;
  }
  CloseHandle(thread);
  say("QUICK-EXIT-F12: enabled; foreground F12 will post WM_CLOSE once");
}
