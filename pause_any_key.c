/* Spider-Man 2001: focus-pause wake exclusively through Any key.
 *
 * The WH_KEYBOARD_LL hook is temporary. Outside the game foreground we
 * ALWAYS chain, leaving Alt+Tab, the desktop and other apps unaffected.
 * The game Flip loop continues pumping messages on the installing thread.
 * The hook swallows keydown/repeat/keyup; resume waits until the chosen
 * sequence is fully released so held keys cannot fire in the next frame.
 *
 * IMPORTANT: Windows low-level keyboard hook suppresses Win32 key messages,
 * but independent DirectInput/Raw Input behavior requires Windows 11 tests.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "pause_any_key.h"

static HHOOK hook=NULL;
static HWND target=NULL;
static BYTE held[256];
static volatile LONG received_press=0;
static volatile LONG completed=0;
static DWORD first_key=0;
static volatile LONG host_focus_blocked=0;
static DWORD host_bad_since=0;
static DWORD host_probe_tick=0;
static int host_probe_bad=0;

/* PAUSED normally ignores keyboard input while X11 says OTHER.
 * If the native helper dies, or endlessly says UNKNOWN, release PAUSED
 * after a bounded grace period rather than leaving the user trapped.
 * This *does not* allow text typed in Firefox to resume a valid pause. */
static BOOL host_bridge_unavailable_for_long(void) {
  DWORD tick=GetTickCount();
  char env[4]={0};
  DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE",env,sizeof(env));
  if(n!=1 || env[0]!='1') {
    host_bad_since=0;
    return FALSE;
  }
  if(!host_probe_tick || (DWORD)(tick-host_probe_tick)>=250u) {
    char path[MAX_PATH],line[64]={0},*end;
    DWORD len,read=0;
    HANDLE f;
    WIN32_FILE_ATTRIBUTE_DATA attr;
    FILETIME now_ft;
    ULARGE_INTEGER now,modified;
    host_probe_tick=tick;
    host_probe_bad=1;
    len=GetModuleFileNameA(NULL,path,sizeof(path));
    end=len && len<sizeof(path)?strrchr(path,'\\'):NULL;
    if(end && (size_t)(end-path)+sizeof("SpiderMan2k1Fix.x11-focus")<sizeof(path)) {
      strcpy(end+1,"SpiderMan2k1Fix.x11-focus");
      if(GetFileAttributesExA(path,GetFileExInfoStandard,&attr)) {
        GetSystemTimeAsFileTime(&now_ft);
        now.LowPart=now_ft.dwLowDateTime;
        now.HighPart=now_ft.dwHighDateTime;
        modified.LowPart=attr.ftLastWriteTime.dwLowDateTime;
        modified.HighPart=attr.ftLastWriteTime.dwHighDateTime;
        if(now.QuadPart>=modified.QuadPart &&
           now.QuadPart-modified.QuadPart<=1200ull*10000ull) {
          f=CreateFileA(path,GENERIC_READ,
                        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                        NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
          if(f!=INVALID_HANDLE_VALUE) {
            if(ReadFile(f,line,sizeof(line)-1,&read,NULL) && read) {
              line[read]=0;
              if(!strcmp(line,"SPIDEY_X11_FOCUS_V1 OTHER\n") ||
                 !strcmp(line,"SPIDEY_X11_FOCUS_V1 GAME\n"))
                host_probe_bad=0;
            }
            CloseHandle(f);
          }
        }
      }
    }
  }
  if(host_probe_bad) {
    if(!host_bad_since)host_bad_since=tick?tick:1;
    return (DWORD)(tick-host_bad_since)>=2200u;
  }
  host_bad_since=0;
  return FALSE;
}
void SpiderPauseAny_SetHostFocusBlocked(BOOL blocked) {
  InterlockedExchange(&host_focus_blocked,blocked?1:0);
}

static LRESULT CALLBACK keyboard_proc(int code,WPARAM wp,LPARAM lp) {
  if(code==HC_ACTION && hook && target &&
     InterlockedCompareExchange(&host_focus_blocked,0,0)==0 &&
     GetForegroundWindow()==target) {
    const KBDLLHOOKSTRUCT *info=(const KBDLLHOOKSTRUCT*)lp;
    DWORD vk=info ? info->vkCode : 256u;
    const int down=(wp==WM_KEYDOWN || wp==WM_SYSKEYDOWN);
    const int up=(wp==WM_KEYUP || wp==WM_SYSKEYUP);
    /* Alt+Tab, Win+Tab and desktop switching must keep working even
     * when Spider-Man is foreground with the pause indicator visible.
     * OS navigation modifiers are NOT a game wake key; a subsequent
     * ordinary key still resumes and is consumed on release. */
    if(vk==VK_MENU || vk==VK_LMENU || vk==VK_RMENU ||
       vk==VK_LWIN || vk==VK_RWIN ||
       (vk==VK_TAB && (info->flags&LLKHF_ALTDOWN)))
      return CallNextHookEx(hook,code,wp,lp);
    if(vk<256u && down) {
      if(!InterlockedCompareExchange(&received_press,0,0)) first_key=vk;
      held[vk]=1;
      InterlockedExchange(&received_press,1);
    } else if(vk<256u && up) {
      unsigned i;int any_held=0;
      held[vk]=0;
      for(i=0;i<256;i++) if(held[i]) {any_held=1;break;}
      if(!any_held && InterlockedCompareExchange(&received_press,0,0)==1)
        InterlockedExchange(&completed,1);
    }
    if(down || up) return 1;
  }
  return CallNextHookEx(hook,code,wp,lp);
}
BOOL SpiderPauseAny_Begin(HWND game_hwnd) {
  HMODULE module;
  if(hook || !game_hwnd || !IsWindow(game_hwnd)) return FALSE;
  module=GetModuleHandleA("SpiderMan2k1Fix.dll");
  if(!module) return FALSE;
  target=game_hwnd;
  memset(held,0,sizeof(held));
  first_key=0;
  InterlockedExchange(&received_press,0);
  InterlockedExchange(&completed,0);
  host_bad_since=0;
  host_probe_tick=0;
  host_probe_bad=0;
  hook=SetWindowsHookExA(WH_KEYBOARD_LL,keyboard_proc,module,0);
  if(!hook) target=NULL;
  return hook!=NULL;
}
BOOL SpiderPauseAny_Completed(void) {
  if(!hook) return FALSE;
  /* Native observer failure must not strand us in a PAUSED loop with
   * the keyboard hook permanently blocked. Release the hook via the
   * regular pause-exit path after a verified absence >2.2 seconds. */
  if(InterlockedCompareExchange(&host_focus_blocked,0,0)!=0 &&
     host_bridge_unavailable_for_long())return TRUE;
  /* If focus changes again while awaiting the key release, re-arm.
   * Do not allow a stale partial press to trigger resumption later. */
  if(GetForegroundWindow()!=target ||
     InterlockedCompareExchange(&host_focus_blocked,0,0)!=0) {
    memset(held,0,sizeof(held));
    first_key=0;
    InterlockedExchange(&received_press,0);
    InterlockedExchange(&completed,0);
    return FALSE;
  }
  return InterlockedCompareExchange(&completed,0,0)==1;
}
BOOL SpiderPauseAny_DiscardGameKeyMessage(const MSG *msg,HWND game_hwnd) {
  if(!hook || !msg || msg->hwnd!=game_hwnd) return FALSE;
  switch(msg->message) {
    case WM_KEYDOWN:case WM_KEYUP:case WM_SYSKEYDOWN:case WM_SYSKEYUP:
    case WM_CHAR:case WM_SYSCHAR:return TRUE;
    default:return FALSE;
  }
}
DWORD SpiderPauseAny_FirstKey(void) {return first_key;}
void SpiderPauseAny_End(void) {
  HHOOK old=hook;
  hook=NULL;
  target=NULL;
  if(old) UnhookWindowsHookEx(old);
  memset(held,0,sizeof(held));
  first_key=0;
  InterlockedExchange(&received_press,0);
  InterlockedExchange(&completed,0);
  InterlockedExchange(&host_focus_blocked,0);
  host_bad_since=0;
  host_probe_tick=0;
  host_probe_bad=0;
}
