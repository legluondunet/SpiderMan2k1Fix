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
  snprintf(line,sizeof(line),
    "PAUSE-PATH: background Flip pause-check ENTER tick=%lu thread=%lu flip=%ld stage=%s",
    (unsigned long)now,(unsigned long)GetCurrentThreadId(),
    (long)flip,stage_name(menu));
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
