/*
 * Spider-Man (2001) experimental native audio lifecycle telemetry.
 * Exact supported image SHA-256 and validated INI are guaranteed by caller.
 *
 * This probes ONLY six original CALL instructions into game-owned voice
 * Play/Stop functions. It does not detour DirectSound COM, change sample
 * content, or alter the original function's arguments/return path.
 *
 * Source disassembly (exact SpideyPC.exe):
 * 0x4722F3: call 0x504230 -- looping play
 * 0x472870: call 0x504230 -- one-shot play
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
static DWORD native_play_ms[SPIDER_VOICE_COUNT];
static IDirectSoundBuffer *native_play_buffer[SPIDER_VOICE_COUNT];
static BYTE native_play_valid[SPIDER_VOICE_COUNT];

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
  if(native_events>=900u){
    if(native_events==900u && native_log)
      native_log("AUDIO-NATIVE: LIMIT 900 events; further voice transition events suppressed");
    native_events++;
    return;
  }
  native_events++;
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
  if(native_log)native_log(line);
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
  DWORD n,oldprotection=0,ignored=0;
  unsigned i;
  BYTE *page=(BYTE *)(uintptr_t)0x00472000u;
  BOOL flushok,restoreok;
  native_log=log_fn;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(!n || n>=sizeof(path))return;
  slash=strrchr(path,'\\');
  if(!slash || (size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1>=sizeof(path))return;
  strcpy(slash+1,"SpiderMan2k1Fix.ini");
  if(GetPrivateProfileIntA("Debug","ExperimentalAudioNativeLifecycleTrace",0,path)!=1)
    return;
  if(native_log)native_log("AUDIO-NATIVE: requested optional six-callsite Play/Stop observation");
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
  /* All 6 call instructions lie in one 4KB executable code page.
   * Change all before the audio game loop starts, and restore the page. */
  if(!VirtualProtect(page,4096u,PAGE_EXECUTE_READWRITE,&oldprotection)){
    if(native_log)native_log("AUDIO-NATIVE: REFUSED VirtualProtect failed");
    return;
  }
  for(i=0;i<NATIVE_HOOK_COUNT;i++){
    uint32_t delta=(uint32_t)(uintptr_t)native_sites[i].wrapper-
                   ((uint32_t)(uintptr_t)native_sites[i].call_site+5u);
    memcpy(native_sites[i].call_site+1,&delta,sizeof(delta));
  }
  flushok=FlushInstructionCache(GetCurrentProcess(),page,4096u);
  if(!flushok){
    for(i=0;i<NATIVE_HOOK_COUNT;i++)
      memcpy(native_sites[i].call_site,native_sites[i].expected,5u);
    FlushInstructionCache(GetCurrentProcess(),page,4096u);
  }
  restoreok=VirtualProtect(page,4096u,oldprotection,&ignored);
  if(!flushok){
    if(native_log)native_log("AUDIO-NATIVE: FAILED instruction cache flush; original calls restored");
    return;
  }
  if(!restoreok){
    /* Instrumentation still installed; report protection restore failure. */
    if(native_log)native_log("AUDIO-NATIVE: WARNING code-page protection restore failed");
  }
  InterlockedExchange(&native_armed,1);
  if(native_log)native_log("AUDIO-NATIVE: ARMED 6 exact original game callsites; unchanged DirectSound and all effects");
}
void SpiderNativeAudioTrace_OnFlip(LONG flip,int in_3d){
  LONG previous;
  if(InterlockedCompareExchange(&native_armed,0,0)!=1)return;
  InterlockedExchange(&native_flip,flip);
  previous=InterlockedExchange(&native_in_3d,in_3d?1:0);
  if(in_3d && !previous){
    native_epoch=GetTickCount();
    native_events=0;
    memset(native_play_valid,0,sizeof(native_play_valid));
    if(native_log)native_log("AUDIO-NATIVE: 3D START t=0ms; on native sound events timestamps are relative to this Flip");
  }
}
