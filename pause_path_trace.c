/* Optional, passive graphics-path trace for Win11/Wine Alt+Tab comparisons.
 * Logs only calls already intercepted by the existing graphics integration.
 * No blocking, audio/video hooks, game memory modifications or input changes.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pause_path_trace.h"

typedef struct {
  const char *name;
  volatile LONG last_tick;
  volatile LONG total;
} TraceCounter;

static TraceCounter counters[] = {
  {"Blt",0,0}, {"DrawPrimitive",0,0}, {"DrawIndexedPrimitive",0,0}
};
static void (*trace_log)(const char *)=NULL;
static volatile LONG trace_enabled=0;

static int tracing(void) {
  return InterlockedCompareExchange(&trace_enabled,0,0)==1;
}
static const char *stage_name(int menu) {return menu ? "menu/video" : "3D";}
static void emit(const char *message) {
  if(trace_log) trace_log(message);
}


/* A separate low-frequency observer measures *actual* foreground changes,
 * including intervals when the game does not call Flip at all.
 * It does not dispatch messages, modify HWNDs, touch DirectInput or render.
 * Started after DirectDraw SetCooperativeLevel, never in DllMain.
 */
static PVOID volatile observed_window=NULL;
static volatile LONG observer_started=0;
static volatile LONG observer_background=0;
static volatile LONG observer_loss_tick=0;
static volatile LONG latest_background_flip=0;

static DWORD WINAPI observe_foreground(LPVOID unused) {
  HWND previous_window=NULL;
  int previously_foreground=-1,ever_foreground=0;
  DWORD loss=0;
  unsigned milestone=0;
  static const DWORD milestones[]={1000,5000,10000,20000};
  char line[246];
  (void)unused;
  while(tracing()) {
    HWND hwnd=(HWND)InterlockedCompareExchangePointer(&observed_window,NULL,NULL);
    DWORD now=GetTickCount();
    int foreground;
    if(!hwnd || !IsWindow(hwnd)) break;
    if(hwnd!=previous_window) {
      previous_window=hwnd;
      previously_foreground=-1;
      ever_foreground=0;
      loss=0;
      InterlockedExchange(&observer_background,0);
    }
    foreground=GetForegroundWindow()==hwnd;
    if(previously_foreground==-1) {
      previously_foreground=foreground;
      if(foreground)ever_foreground=1;
      snprintf(line,sizeof(line),
        "PAUSE-PATH: FOCUS OBSERVER ready tick=%lu thread=%lu foreground=%d",
        (unsigned long)now,(unsigned long)GetCurrentThreadId(),foreground);
      emit(line);
    } else if(foreground!=previously_foreground) {
      previously_foreground=foreground;
      if(foreground) {
        ever_foreground=1;
        InterlockedExchange(&observer_background,0);
        snprintf(line,sizeof(line),
          "PAUSE-PATH: FOCUS RESTORED tick=%lu background-ms=%lu thread=%lu last-background-flip=%ld",
          (unsigned long)now,(unsigned long)(loss?now-loss:0),
          (unsigned long)GetCurrentThreadId(),
          (long)InterlockedCompareExchange(&latest_background_flip,0,0));
      } else if(ever_foreground) {
        loss=now;
        milestone=0;
        InterlockedExchange(&observer_loss_tick,(LONG)now);
        InterlockedExchange(&observer_background,1);
        snprintf(line,sizeof(line),
          "PAUSE-PATH: FOCUS LOST tick=%lu thread=%lu last-background-flip=%ld",
          (unsigned long)now,(unsigned long)GetCurrentThreadId(),
          (long)InterlockedCompareExchange(&latest_background_flip,0,0));
      } else {
        line[0]=0;
      }
      if(line[0])emit(line);
    }
    if(!foreground && ever_foreground && loss &&
       milestone<sizeof(milestones)/sizeof(milestones[0]) &&
       (DWORD)(now-loss)>=milestones[milestone]) {
      snprintf(line,sizeof(line),
        "PAUSE-PATH: FOCUS BACKGROUND elapsed-ms=%lu tick=%lu thread=%lu last-background-flip=%ld blt=%ld draw=%ld indexed=%ld",
        (unsigned long)(now-loss),(unsigned long)now,
        (unsigned long)GetCurrentThreadId(),
        (long)InterlockedCompareExchange(&latest_background_flip,0,0),
        (long)InterlockedCompareExchange(&counters[0].total,0,0),
        (long)InterlockedCompareExchange(&counters[1].total,0,0),
        (long)InterlockedCompareExchange(&counters[2].total,0,0));
      emit(line);
      milestone++;
    }
    /* Diagnostic-only polling, on a dedicated thread, not the game thread. */
    Sleep(25);
  }
  InterlockedExchange(&observer_started,0);
  return 0;
}

void SpiderPausePath_AttachWindow(HWND hwnd) {
  HANDLE thread;
  DWORD error;
  char line[128];
  if(!tracing() || !hwnd || !IsWindow(hwnd))return;
  InterlockedExchangePointer(&observed_window,hwnd);
  if(InterlockedCompareExchange(&observer_started,1,0)!=0)return;
  thread=CreateThread(NULL,0,observe_foreground,NULL,0,NULL);
  if(!thread) {
    error=GetLastError();
    InterlockedExchange(&observer_started,0);
    snprintf(line,sizeof(line),"PAUSE-PATH: WARNING focus observer failed error=%lu",
             (unsigned long)error);
    emit(line);
  } else CloseHandle(thread);
}

void SpiderPausePath_Init(void (*logger)(const char *),int debug_enabled) {
  char env[8]={0};
  DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_PAUSE_PATH_TRACE",env,sizeof(env));
  trace_log=logger;
  if(!debug_enabled || n!=1 || env[0]!='1')return;
  InterlockedExchange(&trace_enabled,1);
  emit("PAUSE-PATH: passive background Blt/DrawPrimitive/DrawIndexedPrimitive/Flip tracing ENABLED; rate <=1 line/s per draw API");
}

void SpiderPausePath_Record(const char *api,HWND hwnd,LONG flip,int menu,
                            const char *surface_role) {
  DWORD now;
  LONG old,total;
  size_t i;
  char line[224];
  TraceCounter *counter=NULL;
  if(!tracing() || !hwnd || GetForegroundWindow()==hwnd)return;
  for(i=0;i<sizeof(counters)/sizeof(counters[0]);i++) {
    if(strcmp(api,counters[i].name)==0) {counter=&counters[i];break;}
  }
  if(!counter)return;
  total=InterlockedIncrement(&counter->total);
  now=GetTickCount();
  old=InterlockedCompareExchange(&counter->last_tick,0,0);
  if(old && (DWORD)(now-(DWORD)old)<1000)return;
  if(InterlockedCompareExchange(&counter->last_tick,(LONG)now,old)!=old)return;
  snprintf(line,sizeof(line),
    "PAUSE-PATH: background %s count=%ld tick=%lu thread=%lu flip=%ld stage=%s target=%s",
    api,(long)total,(unsigned long)now,(unsigned long)GetCurrentThreadId(),
    (long)flip,stage_name(menu),surface_role?surface_role:"n/a");
  emit(line);
}

DWORD SpiderPausePath_FlipEnter(HWND hwnd,LONG flip,int menu) {
  DWORD now;
  char line[180];
  if(!tracing() || !hwnd || GetForegroundWindow()==hwnd)return 0;
  now=GetTickCount();
  InterlockedExchange(&latest_background_flip,flip);
  {
    LONG observed=InterlockedCompareExchange(&observer_loss_tick,0,0);
    LONG since=InterlockedCompareExchange(&observer_background,0,0)==1 && observed ?
      (LONG)(now-(DWORD)observed) : -1;
    snprintf(line,sizeof(line),
      "PAUSE-PATH: background Flip pause-check ENTER tick=%lu thread=%lu flip=%ld stage=%s focus-loss-to-gate-ms=%ld",
      (unsigned long)now,(unsigned long)GetCurrentThreadId(),
      (long)flip,stage_name(menu),(long)since);
  }
  emit(line);
  return now?now:1;
}

void SpiderPausePath_FlipLeave(HWND hwnd,DWORD entered,LONG flip,int menu) {
  DWORD now;
  char line[220];
  if(!entered || !tracing())return;
  now=GetTickCount();
  snprintf(line,sizeof(line),
    "PAUSE-PATH: Flip pause-check EXIT elapsed-ms=%lu tick=%lu thread=%lu flip=%ld foreground=%d stage=%s",
    (unsigned long)(now-entered),(unsigned long)now,
    (unsigned long)GetCurrentThreadId(),(long)flip,
    (hwnd && GetForegroundWindow()==hwnd)?1:0,stage_name(menu));
  emit(line);
}
