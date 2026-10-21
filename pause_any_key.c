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
#include "pause_any_key.h"

static HHOOK hook=NULL;
static HWND target=NULL;
static BYTE held[256];
static volatile LONG received_press=0;
static volatile LONG completed=0;
static DWORD first_key=0;

static LRESULT CALLBACK keyboard_proc(int code,WPARAM wp,LPARAM lp) {
  if(code==HC_ACTION && hook && target && GetForegroundWindow()==target) {
    const KBDLLHOOKSTRUCT *info=(const KBDLLHOOKSTRUCT*)lp;
    DWORD vk=info ? info->vkCode : 256u;
    const int down=(wp==WM_KEYDOWN || wp==WM_SYSKEYDOWN);
    const int up=(wp==WM_KEYUP || wp==WM_SYSKEYUP);
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
  hook=SetWindowsHookExA(WH_KEYBOARD_LL,keyboard_proc,module,0);
  if(!hook) target=NULL;
  return hook!=NULL;
}
BOOL SpiderPauseAny_Completed(void) {
  if(!hook) return FALSE;
  /* If focus changes again while awaiting the key release, re-arm.
   * Do not allow a stale partial press to trigger resumption later. */
  if(GetForegroundWindow()!=target) {
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
}
