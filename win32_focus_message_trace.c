/* Experimental PASSIVE Win32 focus-message observer for Wine/X11 comparison.
 *
 * Uses thread-specific WH_CALLWNDPROC / WH_CALLWNDPROCRET / WH_GETMESSAGE
 * hooks to observe
 * messages before and after the game's WndProc. Never subclasses the WndProc,
 * consumes messages, changes focus, touches input or enters the pause gate.
 *
 * Enabled only if [Debug] Log=1 and BOTH these env vars are exactly "1":
 *   SPIDERMAN2K1FIX_PAUSE_PATH_TRACE
 *   SPIDERMAN2K1FIX_WIN32_FOCUS_MESSAGE_TRACE
 * Hooks are installed only after a valid game HWND exists (not in DllMain).
 * This is an experimental diagnostic, not a pause implementation.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "win32_focus_message_trace.h"

static void (*output)(const char *)=NULL;
static volatile LONG enabled=0;
static HWND watched_hwnd=NULL;
static DWORD watched_thread=0;
static HHOOK before_hook=NULL,after_hook=NULL,retrieved_hook=NULL;
static DWORD last_heartbeat=0;
static volatile LONG retrieved_removed=0,retrieved_peeked=0,delivered=0;
static volatile LONG last_retrieved_tick=0,last_dispatched_tick=0;
static volatile LONG last_retrieved_message=0;
static volatile LONG retrieved_focus=0;

static int env_one(const char *name) {
  char value[8]={0};
  DWORD count=GetEnvironmentVariableA(name,value,sizeof(value));
  return count==1 && value[0]=='1';
}
static const char *message_name(UINT msg) {
  switch(msg) {
    case WM_ACTIVATEAPP: return "WM_ACTIVATEAPP";
    case WM_KILLFOCUS: return "WM_KILLFOCUS";
    case WM_SETFOCUS: return "WM_SETFOCUS";
    case WM_ACTIVATE: return "WM_ACTIVATE";
    case WM_NCACTIVATE: return "WM_NCACTIVATE";
    default: return NULL;
  }
}
static void wall_time(char *dst,size_t cap) {
  SYSTEMTIME st;
  GetLocalTime(&st);
  snprintf(dst,cap,"%02u:%02u:%02u.%03u",
           (unsigned)st.wHour,(unsigned)st.wMinute,
           (unsigned)st.wSecond,(unsigned)st.wMilliseconds);
}
/* This function logs only selected focus messages, always on the game thread.
 * GetActiveWindow()/GetFocus() are thread-relative and therefore meaningful
 * here; the independent observer uses GetGUIThreadInfo(game_thread) instead. */
static void event_line(const char *phase,HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  const char *name=message_name(msg);
  char line[330],wall[24];
  HWND fg;
  if(!name || !output)return;
  wall_time(wall,sizeof(wall));
  fg=GetForegroundWindow();
  snprintf(line,sizeof(line),
    "WIN32-FOCUS: EVENT %s %s wall=%s tick=%lu thread=%lu hwnd=%p wp=0x%08lX lp=0x%08lX fg=%p is-game=%d active=%p focus=%p",
    phase,name,wall,(unsigned long)GetTickCount(),
    (unsigned long)GetCurrentThreadId(),(void*)hwnd,
    (unsigned long)wp,(unsigned long)lp,(void*)fg,
    fg==watched_hwnd,(void*)GetActiveWindow(),(void*)GetFocus());
  output(line);
}
/* Posted-message retrieval, not a raw PeekMessage/GetMessage call counter:
 * WH_GETMESSAGE triggers only when an API actually retrieves a message.
 * A flat count does not prove that the game stopped polling an empty queue. */
static LRESULT CALLBACK focus_retrieved_hook(int code,WPARAM wp,LPARAM lp) {
  if(code>=0 && lp && GetCurrentThreadId()==watched_thread) {
    const MSG *msg=(const MSG *)(uintptr_t)lp;
    DWORD tick=GetTickCount();
    if(wp==PM_REMOVE)InterlockedIncrement(&retrieved_removed);
    else InterlockedIncrement(&retrieved_peeked);
    InterlockedExchange(&last_retrieved_tick,(LONG)tick);
    InterlockedExchange(&last_retrieved_message,(LONG)msg->message);
    if(message_name(msg->message)) {
      char line[230],wall[24];
      InterlockedIncrement(&retrieved_focus);
      wall_time(wall,sizeof(wall));
      snprintf(line,sizeof(line),
        "WIN32-FOCUS: DEQUEUE %s wall=%s tick=%lu thread=%lu msg=%p removed=%d",
        message_name(msg->message),wall,(unsigned long)tick,
        (unsigned long)GetCurrentThreadId(),(void*)msg->hwnd,wp==PM_REMOVE);
      if(output)output(line);
    }
  }
  return CallNextHookEx(retrieved_hook,code,wp,lp);
}
static LRESULT CALLBACK focus_before_hook(int code,WPARAM wp,LPARAM lp) {
  if(code>=0 && lp) {
    const CWPSTRUCT *data=(const CWPSTRUCT *)(uintptr_t)lp;
    if(data->hwnd==watched_hwnd) {
      InterlockedIncrement(&delivered);
      InterlockedExchange(&last_dispatched_tick,(LONG)GetTickCount());
      event_line("BEFORE",data->hwnd,data->message,data->wParam,data->lParam);
    }
  }
  return CallNextHookEx(before_hook,code,wp,lp);
}
static LRESULT CALLBACK focus_after_hook(int code,WPARAM wp,LPARAM lp) {
  if(code>=0 && lp) {
    const CWPRETSTRUCT *data=(const CWPRETSTRUCT *)(uintptr_t)lp;
    if(data->hwnd==watched_hwnd)
      event_line("AFTER",data->hwnd,data->message,data->wParam,data->lParam);
  }
  return CallNextHookEx(after_hook,code,wp,lp);
}
void SpiderWin32FocusTrace_Init(void (*logger)(const char *),int debug_enabled) {
  output=logger;
  if(!debug_enabled ||
     !env_one("SPIDERMAN2K1FIX_PAUSE_PATH_TRACE") ||
     !env_one("SPIDERMAN2K1FIX_WIN32_FOCUS_MESSAGE_TRACE")) return;
  InterlockedExchange(&enabled,1);
  if(output)output("WIN32-FOCUS: passive thread-local message hooks requested [experimental; no pause/input changes]");
}
void SpiderWin32FocusTrace_AttachWindow(HWND hwnd) {
  HMODULE module=NULL;
  DWORD thread,error;
  char line[200];
  if(InterlockedCompareExchange(&enabled,0,0)!=1 || !hwnd || !IsWindow(hwnd))
    return;
  thread=GetWindowThreadProcessId(hwnd,NULL);
  if(!thread)return;
  if(before_hook && after_hook && retrieved_hook && watched_thread==thread) {
    watched_hwnd=hwnd; /* Same GUI thread, but the game may recreate its HWND. */
    return;
  }
  /* A different GUI thread: release the old hooks before installing new ones. */
  if(before_hook) {UnhookWindowsHookEx(before_hook); before_hook=NULL;}
  if(after_hook) {UnhookWindowsHookEx(after_hook); after_hook=NULL;}
  if(retrieved_hook) {UnhookWindowsHookEx(retrieved_hook); retrieved_hook=NULL;}
  watched_hwnd=NULL;
  watched_thread=0;
  if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         (LPCSTR)(void *)focus_before_hook,&module)) {
    if(output)output("WIN32-FOCUS: WARNING cannot locate callback module");
    return;
  }
  watched_hwnd=hwnd;
  watched_thread=thread;
  last_heartbeat=0;
  InterlockedExchange(&retrieved_removed,0);
  InterlockedExchange(&retrieved_peeked,0);
  InterlockedExchange(&delivered,0);
  InterlockedExchange(&last_retrieved_tick,0);
  InterlockedExchange(&last_dispatched_tick,0);
  InterlockedExchange(&last_retrieved_message,0);
  InterlockedExchange(&retrieved_focus,0);
  before_hook=SetWindowsHookExA(WH_CALLWNDPROC,focus_before_hook,module,thread);
  if(before_hook)
    after_hook=SetWindowsHookExA(WH_CALLWNDPROCRET,focus_after_hook,module,thread);
  if(before_hook && after_hook)
    retrieved_hook=SetWindowsHookExA(WH_GETMESSAGE,focus_retrieved_hook,module,thread);
  if(!before_hook || !after_hook || !retrieved_hook) {
    error=GetLastError();
    if(before_hook) {UnhookWindowsHookEx(before_hook); before_hook=NULL;}
    if(after_hook) {UnhookWindowsHookEx(after_hook); after_hook=NULL;}
    if(retrieved_hook) {UnhookWindowsHookEx(retrieved_hook); retrieved_hook=NULL;}
    watched_hwnd=NULL;
    watched_thread=0;
    snprintf(line,sizeof(line),
      "WIN32-FOCUS: WARNING thread-local hooks failed Win32=%lu; tracing disabled",
      (unsigned long)error);
    if(output)output(line);
    InterlockedExchange(&enabled,0);
    return;
  }
  snprintf(line,sizeof(line),
    "WIN32-FOCUS: hook READY hwnd=%p thread=%lu before=1 after=1 dequeue=1",
    (void*)hwnd,(unsigned long)thread);
  if(output)output(line);
}
void SpiderWin32FocusTrace_Heartbeat(HWND hwnd,DWORD now) {
  GUITHREADINFO gui;
  DWORD game_thread;
  HWND foreground;
  BOOL gui_ok;
  char line[485],wall[24];
  if(InterlockedCompareExchange(&enabled,0,0)!=1 ||
     !hwnd || !IsWindow(hwnd) || !output) return;
  if(last_heartbeat && (DWORD)(now-last_heartbeat)<5000UL)return;
  last_heartbeat=now;
  foreground=GetForegroundWindow();
  game_thread=GetWindowThreadProcessId(hwnd,NULL);
  memset(&gui,0,sizeof(gui));
  gui.cbSize=sizeof(gui);
  gui_ok=game_thread!=0 && GetGUIThreadInfo(game_thread,&gui);
  wall_time(wall,sizeof(wall));
  snprintf(line,sizeof(line),
    "WIN32-FOCUS: HEARTBEAT wall=%s tick=%lu observer-thread=%lu game-thread=%lu hwnd=%p fg=%p is-game=%d gui-ok=%d gui-active=%p gui-focus=%p dequeue-remove=%ld dequeue-peek=%ld dequeue-focus=%ld window-delivery=%ld last-dequeue-tick=%lu last-dequeue-msg=0x%04lX last-delivery-tick=%lu",
    wall,(unsigned long)now,(unsigned long)GetCurrentThreadId(),
    (unsigned long)game_thread,(void*)hwnd,(void*)foreground,
    foreground==hwnd,!!gui_ok,(void*)gui.hwndActive,(void*)gui.hwndFocus,
    (long)InterlockedCompareExchange(&retrieved_removed,0,0),
    (long)InterlockedCompareExchange(&retrieved_peeked,0,0),
    (long)InterlockedCompareExchange(&retrieved_focus,0,0),
    (long)InterlockedCompareExchange(&delivered,0,0),
    (unsigned long)(DWORD)InterlockedCompareExchange(&last_retrieved_tick,0,0),
    (unsigned long)InterlockedCompareExchange(&last_retrieved_message,0,0),
    (unsigned long)(DWORD)InterlockedCompareExchange(&last_dispatched_tick,0,0));
  output(line);
}
