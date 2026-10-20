/* Experimental Windows/Wine focus-event pause.
 *
 * No DirectInput IAT/COM hooks, synthetic input, SetForegroundWindow,
 * SuspendThread, or window Z-order manipulation. WndProc only records focus
 * state. The waiting happens later at the existing, game-owned Flip call.
 * It is NOT a full simulation/movie/audio pause and cannot be instantaneous
 * unless the next Flip is reached promptly.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "focus_event_pause.h"

static void (*write_log)(const char *)=NULL;
static volatile LONG enabled=0;
static volatile LONG focus_lost=0;
static volatile LONG has_been_foreground=0;
static volatile LONG pause_busy=0;
static volatile LONG focus_loss_tick=0;
static HWND game_window=NULL;
static WNDPROC original_proc=NULL;

static void write_focus(const char *tag,DWORD duration) {
  char line[220];
  snprintf(line,sizeof(line),"FOCUS-EVENT: %s elapsed-ms=%lu foreground=%d",
           tag,(unsigned long)duration,
           game_window && GetForegroundWindow()==game_window);
  if(write_log) write_log(line);
}
static LONG read_flag(volatile LONG *v) {
  return InterlockedCompareExchange(v,0,0);
}
static void mark_lost(const char *reason) {
  DWORD now;
  if(!read_flag(&has_been_foreground)) return;
  if(InterlockedExchange(&focus_lost,1)!=0) return;
  now=GetTickCount();
  InterlockedExchange(&focus_loss_tick,(LONG)now);
  write_focus(reason,0);
}
static void mark_restored(const char *reason) {
  DWORD since,now;
  if(GetForegroundWindow()!=game_window) return;
  InterlockedExchange(&has_been_foreground,1);
  if(InterlockedExchange(&focus_lost,0)!=1) return;
  now=GetTickCount();
  since=(DWORD)read_flag(&focus_loss_tick);
  write_focus(reason,now-since);
}
static LRESULT CALLBACK focus_pause_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  WNDPROC chained=original_proc;
  switch(msg) {
    case WM_ACTIVATEAPP:
      if(!wp) mark_lost("WM_ACTIVATEAPP(FALSE)");
      else mark_restored("WM_ACTIVATEAPP(TRUE)");
      break;
    case WM_KILLFOCUS:
      mark_lost("WM_KILLFOCUS");
      break;
    case WM_SETFOCUS:
      mark_restored("WM_SETFOCUS");
      break;
    default:
      break;
  }
  if(msg==WM_NCDESTROY && game_window==hwnd) {
    if((WNDPROC)GetWindowLongPtrA(hwnd,GWLP_WNDPROC)==focus_pause_proc &&
       chained) {
      SetWindowLongPtrA(hwnd,GWLP_WNDPROC,(LONG_PTR)chained);
    }
    game_window=NULL;
    original_proc=NULL;
    InterlockedExchange(&focus_lost,0);
    InterlockedExchange(&pause_busy,0);
  }
  return chained ? CallWindowProcA(chained,hwnd,msg,wp,lp)
                 : DefWindowProcA(hwnd,msg,wp,lp);
}
void SpiderFocusEventPause_Init(void (*logger)(const char *)) {
  char path[MAX_PATH],*slash;
  DWORD n;
  int want=0;
  write_log=logger;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(!n || n>=sizeof(path) || !(slash=strrchr(path,'\\'))) return;
  if((size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1>=sizeof(path)) return;
  strcpy(slash+1,"SpiderMan2k1Fix.ini");
  want=GetPrivateProfileIntA("Input","ExperimentalFocusEventPause",0,path)==1;
  if(!want) return;
  if(GetPrivateProfileIntA("Input","ExperimentalFocusTrace",0,path)==1 ||
     GetPrivateProfileIntA("Input","ExperimentalAltTabReacquire",0,path)==1) {
    if(write_log) write_log("FOCUS-EVENT: DISABLED: incompatible with experimental DirectInput focus hooks");
    return;
  }
  InterlockedExchange(&enabled,1);
  if(write_log) write_log("FOCUS-EVENT: PROTOTYPE enabled (Win32 message observer, next-Flip gate, NOT an immediate engine pause)");
}
int SpiderFocusEventPause_IsEnabled(void) {
  return read_flag(&enabled)==1;
}
void SpiderFocusEventPause_AttachWindow(HWND hwnd) {
  WNDPROC old;
  DWORD error;
  if(!SpiderFocusEventPause_IsEnabled() || !hwnd || !IsWindow(hwnd)) return;
  if(game_window==hwnd && original_proc) return;
  /* Do not silently detach another layer's WndProc when the HWND changes. */
  if(game_window && original_proc && IsWindow(game_window)) {
    if((WNDPROC)GetWindowLongPtrA(game_window,GWLP_WNDPROC)!=focus_pause_proc) {
      if(write_log) write_log("FOCUS-EVENT: DISABLED: another WndProc replaced our subclass");
      InterlockedExchange(&enabled,0);
      return;
    }
    SetWindowLongPtrA(game_window,GWLP_WNDPROC,(LONG_PTR)original_proc);
  }
  game_window=hwnd;
  original_proc=NULL;
  InterlockedExchange(&focus_lost,0);
  InterlockedExchange(&has_been_foreground,GetForegroundWindow()==hwnd);
  InterlockedExchange(&pause_busy,0);
  SetLastError(0);
  old=(WNDPROC)SetWindowLongPtrA(hwnd,GWLP_WNDPROC,(LONG_PTR)focus_pause_proc);
  error=GetLastError();
  if(!old && error) {
    char line[120];
    snprintf(line,sizeof(line),"FOCUS-EVENT: DISABLED: WndProc subclass error=%lu",
             (unsigned long)error);
    if(write_log) write_log(line);
    game_window=NULL;
    InterlockedExchange(&enabled,0);
    return;
  }
  original_proc=old;
  if(write_log) write_log("FOCUS-EVENT: observer attached (no DirectInput interception)");
}
void SpiderFocusEventPause_OnFlip(void) {
  MSG msg;
  HWND hwnd=game_window;
  DWORD start,loss;
  if(!SpiderFocusEventPause_IsEnabled() || !hwnd || !IsWindow(hwnd)) return;
  if(GetForegroundWindow()==hwnd) {
    mark_restored("foreground-at-Flip");
    return;
  }
  if(!read_flag(&has_been_foreground)) return;
  if(!read_flag(&focus_lost)) mark_lost("foreground-lost-at-Flip (event not yet dispatched)");
  if(InterlockedCompareExchange(&pause_busy,1,0)!=0) return;
  start=GetTickCount();
  loss=(DWORD)read_flag(&focus_loss_tick);
  write_focus("GATE ENTER: time-from-focus-loss",start-loss);
  while(IsWindow(hwnd) && GetForegroundWindow()!=hwnd) {
    /* This is the game thread: deliver focus/quit messages or Alt+Tab
     * would become impossible. Avoid high-frequency logging in the loop. */
    while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) {
      if(msg.message==WM_QUIT) {
        PostQuitMessage((int)msg.wParam);
        write_focus("GATE EXIT: WM_QUIT",GetTickCount()-start);
        InterlockedExchange(&pause_busy,0);
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    if(GetForegroundWindow()==hwnd) break;
    MsgWaitForMultipleObjectsEx(0,NULL,50,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
  }
  write_focus("GATE EXIT: time-in-gate",GetTickCount()-start);
  if(IsWindow(hwnd)) mark_restored("foreground-at-gate-exit");
  InterlockedExchange(&pause_busy,0);
}
