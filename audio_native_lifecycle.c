/*
 * Spider-Man (2001) experimental native audio lifecycle telemetry.
 * Exact supported image SHA-256 and validated INI are guaranteed by caller.
 *
 * This probes ONLY eleven original CALL instructions into game-owned voice
 * Play/Stop functions. It does not detour DirectSound COM, change sample
 * content, or alter the original function's arguments/return path.
 *
 * Source disassembly (exact SpideyPC.exe):
 * 0x4722F3: call 0x504230 -- looping play
 * 0x472870: call 0x504230 -- one-shot play
 * 0x470F6E: call 0x5042A0 -- cleanup path A
 * 0x4712FC: call 0x5042A0 -- cleanup path B
 * 0x471858: call 0x5042A0 -- cleanup path C
 * 0x471B7C: call 0x5042A0 -- cleanup path D
 * 0x47236C: call 0x5042A0 -- cleanup path E
 * 0x4723F2: call 0x5042A0 -- actor voice stop
 * 0x4725EE: call 0x5042A0 -- first allocator reuse
 * 0x47261F: call 0x5042A0 -- second allocator reuse
 * 0x4729C4: call 0x5042A0 -- scene teardown
 *
 * Experimental / OFF by default. This is NOT a fix for audio crackles.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "audio_native_lifecycle.h"
#include "audio_swing5_fingerprint.h"

#define SPIDER_NATIVE_BASE ((uintptr_t)0x00400000u)
#define SPIDER_VOICE_COUNT 32u
#define SPIDER_VOICE_TABLE ((BYTE *)(uintptr_t)0x006BBD50u)
#define SPIDER_PRIMARY (*(IDirectSoundBuffer * const *)(uintptr_t)0x006BBF1Cu)
#define SPIDER_NATIVE_PLAY ((void (__cdecl *)(int,int))(uintptr_t)0x00504230u)
#define SPIDER_NATIVE_STOP ((void (__cdecl *)(int))(uintptr_t)0x005042A0u)

static void (*native_log)(const char *)=NULL;
static volatile LONG native_armed=0;
static volatile LONG native_in_3d=0,native_flip=0;
static DWORD native_epoch=0;
static unsigned native_events=0;
static BYTE native_f8_was_down=0;
static DWORD native_play_ms[SPIDER_VOICE_COUNT];
static IDirectSoundBuffer *native_play_buffer[SPIDER_VOICE_COUNT];
static BYTE native_play_valid[SPIDER_VOICE_COUNT];

/* The full native trace is bounded (1600 lines) to avoid enormous logs.
 * Keep a small, passive, opt-in rolling history of EVERY native transition,
 * including those after that limit, and flush it only on human F8 markers.
 * No additional DirectSound calls or changes to the game's native audio.
 * The ring uses a dedicated CRITICAL_SECTION: a game audio event may arrive
 * on a different thread from the DirectDraw Flip/F8 observer. */
#define NATIVE_HISTORY_CAP 256u
#define NATIVE_HISTORY_WINDOW_MS 3000u
#define NATIVE_HISTORY_DUMP_MAX 96u
typedef struct {
  DWORD time_ms;
  char line[320];
} NativeHistoryEntry;
static NativeHistoryEntry native_history[NATIVE_HISTORY_CAP];
static CRITICAL_SECTION native_history_cs;
static int native_history_ready=0;
static unsigned native_history_next=0,native_history_count=0;

static void native_history_reset(void){
  if(!native_history_ready)return;
  EnterCriticalSection(&native_history_cs);
  native_history_next=0;
  native_history_count=0;
  LeaveCriticalSection(&native_history_cs);
}
static void native_history_add(DWORD now,const char *line){
  NativeHistoryEntry *entry;
  if(!native_history_ready)return;
  EnterCriticalSection(&native_history_cs);
  entry=&native_history[native_history_next];
  entry->time_ms=now;
  /* The native event already fits a 320-character bounded buffer. */
  memcpy(entry->line,line,sizeof(entry->line));
  native_history_next=(native_history_next+1u)%NATIVE_HISTORY_CAP;
  if(native_history_count<NATIVE_HISTORY_CAP)native_history_count++;
  LeaveCriticalSection(&native_history_cs);
}
/* Snapshot inside the lock, emit outside it: the logging callback must
 * never be invoked while holding the ring lock. Events appear oldest first
 * so that stop/play order is preserved around the audible event. */
static void native_history_dump(DWORD now){
  NativeHistoryEntry recent[NATIVE_HISTORY_DUMP_MAX];
  unsigned found=0,kept=0,i;
  char msg[420];
  if(!native_history_ready || !native_log)return;
  EnterCriticalSection(&native_history_cs);
  for(i=0;i<native_history_count;i++){
    unsigned idx=(native_history_next+NATIVE_HISTORY_CAP-1u-i)%NATIVE_HISTORY_CAP;
    const NativeHistoryEntry *entry=&native_history[idx];
    if(now-entry->time_ms>NATIVE_HISTORY_WINDOW_MS)break;
    found++;
    if(kept<NATIVE_HISTORY_DUMP_MAX)recent[kept++]=*entry;
  }
  LeaveCriticalSection(&native_history_cs);
  snprintf(msg,sizeof(msg),
    "AUDIO-NATIVE-HISTORY: F8 PRECEDING %lu ms kept=%u total=%u truncated=%u (rolling history, active even after normal 1600-event log limit)",
    (unsigned long)NATIVE_HISTORY_WINDOW_MS,kept,found,found-kept);
  native_log(msg);
  while(kept>0u){
    const NativeHistoryEntry *event=&recent[--kept];
    snprintf(msg,sizeof(msg),"AUDIO-NATIVE-HISTORY: before_f8_ms=%lu %s",
      (unsigned long)(now-event->time_ms),event->line);
    native_log(msg);
  }
}

typedef struct {
  IDirectSoundBuffer *buffer;
  DWORD status,bytes,hz,read,write;
  unsigned has_status,has_pos;
} VoiceInfo;

static void capture_voice(int slot,VoiceInfo *out){
  IDirectSoundBuffer *buf;
  DSBCAPS caps;
  memset(out,0,sizeof(*out));
  if(slot<0 || (unsigned)slot>=SPIDER_VOICE_COUNT)return;
  buf=*(IDirectSoundBuffer * const *)(SPIDER_VOICE_TABLE+(unsigned)slot*12u);
  out->buffer=buf;
  if(!buf || buf==SPIDER_PRIMARY)return;
  if(SUCCEEDED(IDirectSoundBuffer_GetStatus(buf,&out->status)))
    out->has_status=1;
  memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
  if(SUCCEEDED(IDirectSoundBuffer_GetCaps(buf,&caps)))
    out->bytes=caps.dwBufferBytes;
  if(SUCCEEDED(IDirectSoundBuffer_GetFrequency(buf,&out->hz))){}
  if(out->has_status && (out->status&DSBSTATUS_PLAYING) &&
     SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(buf,&out->read,&out->write)))
    out->has_pos=1;
}
static void trace_event(const char *kind,int slot,int looping,
                        const VoiceInfo *info){
  char line[320];
  DWORD now;
  LONG frame;
  long age=-1;
  if(InterlockedCompareExchange(&native_in_3d,0,0)!=1)return;
  now=GetTickCount()-native_epoch;
  frame=InterlockedCompareExchange(&native_flip,0,0);
  if(slot>=0 && (unsigned)slot<SPIDER_VOICE_COUNT &&
     native_play_valid[slot] && native_play_buffer[slot]==info->buffer)
    age=(long)(now-native_play_ms[slot]);
  snprintf(line,sizeof(line),
    "AUDIO-NATIVE: %s t=%lums flip~=%ld slot=%d voice=%p status=%s0x%08lX bytes=%lu hz=%lu read=%lu write=%lu play_age_ms=%ld looping_arg=%d",
    kind,(unsigned long)now,(long)frame,slot,(void *)info->buffer,
    info->has_status?"":"UNKNOWN:",(unsigned long)info->status,
    (unsigned long)info->bytes,(unsigned long)info->hz,
    (unsigned long)info->read,(unsigned long)info->write,
    age,looping);
  native_history_add(now,line);
  if(native_events<1600u){
    native_events++;
    if(native_log)native_log(line);
  }else{
    if(native_events==1600u && native_log)
      native_log("AUDIO-NATIVE: LIMIT 1600 events; further voice transition events suppressed (F8 ring remains active)");
    native_events++;
  }
}
static void __cdecl native_play_loop(int slot,int looping){
  VoiceInfo info;
  SPIDER_NATIVE_PLAY(slot,looping);
  if(InterlockedCompareExchange(&native_armed,0,0)!=1 ||
     InterlockedCompareExchange(&native_in_3d,0,0)!=1)return;
  capture_voice(slot,&info);
  if(slot>=0 && (unsigned)slot<SPIDER_VOICE_COUNT){
    native_play_ms[slot]=GetTickCount()-native_epoch;
    native_play_buffer[slot]=info.buffer;
    native_play_valid[slot]=1;
  }
  trace_event("PLAY-LOOP",slot,looping,&info);
}
static void __cdecl native_play_oneshot(int slot,int looping){
  VoiceInfo info;
  SPIDER_NATIVE_PLAY(slot,looping);
  if(InterlockedCompareExchange(&native_armed,0,0)!=1 ||
     InterlockedCompareExchange(&native_in_3d,0,0)!=1)return;
  capture_voice(slot,&info);
  if(slot>=0 && (unsigned)slot<SPIDER_VOICE_COUNT){
    native_play_ms[slot]=GetTickCount()-native_epoch;
    native_play_buffer[slot]=info.buffer;
    native_play_valid[slot]=1;
  }
  trace_event("PLAY-ONESHOT",slot,looping,&info);
  /* Exact-size/frequency candidates only, separately gated, max 12. */
  SpiderSwing5Fingerprint_Probe(slot,info.buffer,info.bytes,info.hz);
}
static void stop_and_trace(int slot,const char *kind){
  VoiceInfo info;
  if(InterlockedCompareExchange(&native_armed,0,0)==1 &&
     InterlockedCompareExchange(&native_in_3d,0,0)==1){
    capture_voice(slot,&info);
    trace_event(kind,slot,-1,&info);
    if(slot>=0 && (unsigned)slot<SPIDER_VOICE_COUNT)
      native_play_valid[slot]=0;
  }
  /* Forward to precisely the original game code; never suppress Stop. */
  SPIDER_NATIVE_STOP(slot);
}
/* Other five direct native stop sites were missing from the first
 * six-callsite diagnostic. Their names describe addresses only:
 * game-level intent has NOT been reverse engineered yet. */
static void __cdecl native_stop_site_470f6e(int slot){
  stop_and_trace(slot,"STOP-SITE-470F6E");
}
static void __cdecl native_stop_site_4712fc(int slot){
  stop_and_trace(slot,"STOP-SITE-4712FC");
}
static void __cdecl native_stop_site_471858(int slot){
  stop_and_trace(slot,"STOP-SITE-471858");
}
static void __cdecl native_stop_site_471b7c(int slot){
  stop_and_trace(slot,"STOP-SITE-471B7C");
}
static void __cdecl native_stop_site_47236c(int slot){
  stop_and_trace(slot,"STOP-SITE-47236C");
}
static void __cdecl native_stop_actor(int slot){
  stop_and_trace(slot,"STOP-ACTOR");
}
static void __cdecl native_stop_recycle_first(int slot){
  stop_and_trace(slot,"STOP-RECYCLE-FIRST");
}
static void __cdecl native_stop_recycle_second(int slot){
  stop_and_trace(slot,"STOP-RECYCLE-SECOND");
}
static void __cdecl native_stop_teardown(int slot){
  stop_and_trace(slot,"STOP-TEARDOWN");
}

typedef struct {
  BYTE *call_site;
  BYTE expected[5];
  void *wrapper;
} NativeHookSite;
static NativeHookSite native_sites[]={
  {(BYTE *)(uintptr_t)0x00470F6Eu,{0xE8,0x2D,0x33,0x09,0x00},(void *)native_stop_site_470f6e},
  {(BYTE *)(uintptr_t)0x004712FCu,{0xE8,0x9F,0x2F,0x09,0x00},(void *)native_stop_site_4712fc},
  {(BYTE *)(uintptr_t)0x00471858u,{0xE8,0x43,0x2A,0x09,0x00},(void *)native_stop_site_471858},
  {(BYTE *)(uintptr_t)0x00471B7Cu,{0xE8,0x1F,0x27,0x09,0x00},(void *)native_stop_site_471b7c},
  {(BYTE *)(uintptr_t)0x0047236Cu,{0xE8,0x2F,0x1F,0x09,0x00},(void *)native_stop_site_47236c},
  {(BYTE *)(uintptr_t)0x004722F3u,{0xE8,0x38,0x1F,0x09,0x00},(void *)native_play_loop},
  {(BYTE *)(uintptr_t)0x00472870u,{0xE8,0xBB,0x19,0x09,0x00},(void *)native_play_oneshot},
  {(BYTE *)(uintptr_t)0x004723F2u,{0xE8,0xA9,0x1E,0x09,0x00},(void *)native_stop_actor},
  {(BYTE *)(uintptr_t)0x004725EEu,{0xE8,0xAD,0x1C,0x09,0x00},(void *)native_stop_recycle_first},
  {(BYTE *)(uintptr_t)0x0047261Fu,{0xE8,0x7C,0x1C,0x09,0x00},(void *)native_stop_recycle_second},
  {(BYTE *)(uintptr_t)0x004729C4u,{0xE8,0xD7,0x18,0x09,0x00},(void *)native_stop_teardown}
};
#define NATIVE_HOOK_COUNT (sizeof(native_sites)/sizeof(native_sites[0]))

void SpiderNativeAudioTrace_Init(void (*log_fn)(const char *)){
  char path[MAX_PATH],*slash;
  DWORD n,oldprotection[3]={0,0,0},ignored=0;
  unsigned i,j;
  BYTE *const pages[3]={
    (BYTE *)(uintptr_t)0x00470000u,
    (BYTE *)(uintptr_t)0x00471000u,
    (BYTE *)(uintptr_t)0x00472000u
  };
  BOOL flushok,restoreok=TRUE;
  native_log=log_fn;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(!n || n>=sizeof(path))return;
  slash=strrchr(path,'\\');
  if(!slash || (size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1>=sizeof(path))return;
  strcpy(slash+1,"SpiderMan2k1Fix.ini");
  if(GetPrivateProfileIntA("Debug","ExperimentalAudioNativeLifecycleTrace",0,path)!=1){
    if(GetPrivateProfileIntA("Debug","ExperimentalAudioSwing5Fingerprint",0,path)==1 && native_log)
      native_log("AUDIO-SWING5-FP: REFUSED; requires ExperimentalAudioNativeLifecycleTrace=1");
    return;
  }
  if(native_log)native_log("AUDIO-NATIVE: requested optional all 11 direct native voice Play/Stop callsites; F8 click marker enabled");
  /* Caller verified the known SHA256 and parsed INI before getting here. */
  if((uintptr_t)GetModuleHandleA(NULL)!=SPIDER_NATIVE_BASE){
    if(native_log)native_log("AUDIO-NATIVE: REFUSED relocated executable");
    return;
  }
  for(i=0;i<NATIVE_HOOK_COUNT;i++)
    if(memcmp(native_sites[i].call_site,native_sites[i].expected,5u)!=0){
      if(native_log)native_log("AUDIO-NATIVE: REFUSED unexpected callsite bytes");
      return;
    }

  /* The 11 calls occupy three 4KB pages. Acquire write permissions for
   * all pages BEFORE changing any instruction. Fail closed on partial
   * protection acquisition; restore every previous page on the way out. */
  for(i=0;i<3u;i++){
    if(!VirtualProtect(pages[i],4096u,PAGE_EXECUTE_READWRITE,&oldprotection[i])){
      while(i>0u){--i;VirtualProtect(pages[i],4096u,oldprotection[i],&ignored);}
      if(native_log)native_log("AUDIO-NATIVE: REFUSED VirtualProtect failed; no instruction modified");
      return;
    }
  }
  for(i=0;i<NATIVE_HOOK_COUNT;i++){
    uint32_t delta=(uint32_t)(uintptr_t)native_sites[i].wrapper-
                   ((uint32_t)(uintptr_t)native_sites[i].call_site+5u);
    memcpy(native_sites[i].call_site+1,&delta,sizeof(delta));
  }
  flushok=FlushInstructionCache(GetCurrentProcess(),pages[0],3u*4096u);
  if(!flushok){
    for(i=0;i<NATIVE_HOOK_COUNT;i++)
      memcpy(native_sites[i].call_site,native_sites[i].expected,5u);
    FlushInstructionCache(GetCurrentProcess(),pages[0],3u*4096u);
  }
  for(j=0;j<3u;j++)
    if(!VirtualProtect(pages[j],4096u,oldprotection[j],&ignored))
      restoreok=FALSE;
  if(!flushok){
    if(native_log)native_log("AUDIO-NATIVE: FAILED instruction cache flush; original calls restored");
    return;
  }
  if(!restoreok){
    /* Instrumentation still installed; report protection restore failure. */
    if(native_log)native_log("AUDIO-NATIVE: WARNING code-page protection restore failed");
  }
  SpiderSwing5Fingerprint_Init(native_log,
    GetPrivateProfileIntA("Debug","ExperimentalAudioSwing5Fingerprint",0,path)==1,
    GetPrivateProfileIntA("Debug","ExperimentalAudioSwing5Capture",0,path)==1,
    GetPrivateProfileIntA("Debug","ExperimentalAudioSwing5TailMute",0,path)==1);
  InitializeCriticalSection(&native_history_cs);
  native_history_ready=1;
  InterlockedExchange(&native_armed,1);
  if(native_log)native_log("AUDIO-NATIVE-HISTORY: ARMED 256 events rolling, 3000 ms F8 retrospective, max 96 logged events per marker, no audio mutation");
  if(native_log)native_log("AUDIO-NATIVE: ARMED 11 exact original game callsites (2 Play, 9 Stop), unchanged DirectSound and all effects; F8 click marker ready");
}
void SpiderNativeAudioTrace_OnFlip(LONG flip,int in_3d){
  LONG previous;
  BYTE f8_down;
  if(InterlockedCompareExchange(&native_armed,0,0)!=1)return;
  InterlockedExchange(&native_flip,flip);
  previous=InterlockedExchange(&native_in_3d,in_3d?1:0);
  if(in_3d && !previous){
    native_epoch=GetTickCount();
    native_events=0;
    native_history_reset();
    native_f8_was_down=0;
    memset(native_play_valid,0,sizeof(native_play_valid));
    if(native_log)native_log("AUDIO-NATIVE: 3D START t=0ms; on native sound events timestamps are relative to this Flip");
  }
  if(!in_3d){
    native_f8_was_down=0;
    return;
  }
  /* Human time marker, sampled once per rendered frame. F8 is NOT
   * swallowed: game input is unchanged. Press once just AFTER hearing
   * the click. Natural reaction latency must be considered. */
  f8_down=(BYTE)!!(GetAsyncKeyState(VK_F8)&0x8000);
  if(f8_down && !native_f8_was_down && native_log){
    char msg[160];
    snprintf(msg,sizeof(msg),
      "AUDIO-NATIVE-MARK: F8 AFTER-CLICK t=%lums flip=%ld (human reaction delay; correlate preceding events)",
      (unsigned long)(GetTickCount()-native_epoch),(long)flip);
    native_log(msg);
    native_history_dump(GetTickCount()-native_epoch);
  }
  native_f8_was_down=f8_down;
}
