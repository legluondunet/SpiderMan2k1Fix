/*
 * DirectSound-native optional Heli1 declick and read-only audio trace.
 *
 * Valid ONLY for the already SHA256-checked supported SpideyPC.exe.
 * The single original x86 CALL at 0x00472870 points to native
 * DXSOUND_Play (0x00504230): E8 BB 19 09 00. We change its rel32
 * destination, and pass ALL calls through to the original function.
 * No native Stop/IsPlaying API hooks, SDL3, or rewritten game WAV files.
 *
 * Optional bounded PLAY-TRACE is read-only. It measures the existing
 * DirectSound secondary voice before and after native Play to investigate
 * short residual noises. It never changes the DS play cursor or voice.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pkr_audio_index.h"
#include "pkr_audio_play_hook.h"

#define GAME_BASE ((uintptr_t)0x00400000u)
#define VOICE_TABLE ((BYTE*)(uintptr_t)0x006BBD50u)
#define VOICE_STRIDE 12u
#define ORIGINAL_CALL ((BYTE*)(uintptr_t)0x00472870u)
#define NATIVE_PLAY ((void (__cdecl *)(int,int))(uintptr_t)0x00504230u)
#define MAX_PLAY_TRACE 256u
static const BYTE expected_call[5]={0xE8,0xBB,0x19,0x09,0x00};
static void (*diag)(const char *)=NULL;
static int installed=0,trace_enabled=0;
static unsigned trace_count=0;

/* Snapshot only: no Stop, Lock, SetCurrentPosition, Play, or changes to
 * existing game state. Each original Play gets at most one before/after
 * pair; max 256 pairs per process, even when effects are spammed. */
static void trace_voice(unsigned n,const char *phase,int slot,int looping,
                        int repaired,IDirectSoundBuffer *buf){
  DWORD play=0xFFFFFFFFu,write=0xFFFFFFFFu,status=0xFFFFFFFFu,got=0;
  DSBCAPS caps;
  WAVEFORMATEX fmt;
  HRESULT hr_caps=E_POINTER,hr_fmt=E_POINTER;
  HRESULT hr_status=E_POINTER,hr_pos=E_POINTER;
  char line[370];
  memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
  memset(&fmt,0,sizeof(fmt));
  if(buf){
    hr_caps=IDirectSoundBuffer_GetCaps(buf,&caps);
    hr_fmt=IDirectSoundBuffer_GetFormat(buf,&fmt,sizeof(fmt),&got);
    hr_status=IDirectSoundBuffer_GetStatus(buf,&status);
    hr_pos=IDirectSoundBuffer_GetCurrentPosition(buf,&play,&write);
  }
  if(diag){
    snprintf(line,sizeof(line),
      "PKR-AUDIO: PLAY-TRACE n=%u phase=%s ms=%lu slot=%d loop=%d repaired=%d buf=%p bytes=%lu hz=%lu status=0x%08lX play=%lu write=%lu caps_hr=0x%08lX fmt_hr=0x%08lX status_hr=0x%08lX pos_hr=0x%08lX",
      n,phase,(unsigned long)GetTickCount(),slot,!!looping,repaired,
      (void*)buf,
      (unsigned long)(SUCCEEDED(hr_caps)?caps.dwBufferBytes:0u),
      (unsigned long)(SUCCEEDED(hr_fmt) && got>=16u?fmt.nSamplesPerSec:0u),
      (unsigned long)status,(unsigned long)play,(unsigned long)write,
      (unsigned long)(DWORD)hr_caps,(unsigned long)(DWORD)hr_fmt,
      (unsigned long)(DWORD)hr_status,(unsigned long)(DWORD)hr_pos);
    diag(line);
  }
}
static IDirectSoundBuffer *slot_buffer(int slot){
  const BYTE *entry;
  if(slot<0||slot>=32)return NULL;
  entry=VOICE_TABLE+(unsigned)slot*VOICE_STRIDE;
  return *(IDirectSoundBuffer *const*)entry;
}
static void __cdecl native_play_with_pkr(int slot,int looping){
  IDirectSoundBuffer *buffer=slot_buffer(slot);
  int repaired=0;
  int trace_now=trace_enabled && trace_count<MAX_PLAY_TRACE;
  unsigned n=0;
  if(installed && buffer){
    SpiderPKRAudioIndex_Probe(slot,buffer,looping); /* optional read-only */
    /* Retain the independent, opt-in Heli1 two-sample declick.
     * The native loader already restores missing WAV tail sectors;
     * no late DirectSound PCM tail repair is attempted here. */
    if(SpiderPKRAudioIndex_TryHeli1Declick(slot,buffer,looping))repaired=1;
  }
  if(trace_now){
    n=++trace_count;
    trace_voice(n,"before",slot,looping,repaired,buffer);
  }
  /* ALWAYS use the game's original DirectSound Play, even if no source
   * matched, archive validation failed, or DS Lock/Unlock failed. */
  NATIVE_PLAY(slot,looping);
  /* The native call may replace a slot's buffer. Re-read it; never touch
   * the previously captured pointer after returning from native Play. */
  if(trace_now)trace_voice(n,"after",slot,looping,repaired,slot_buffer(slot));
}

void SpiderPKRAudioPlayHook_Init(void (*logger)(const char*),int enabled,int trace){
  DWORD old=0,ignored=0;
  BYTE *call=ORIGINAL_CALL;
  uint32_t rel;
  diag=logger;
  trace_enabled=(trace==1);
  if(enabled!=1)return;
  if((uintptr_t)GetModuleHandleA(NULL)!=GAME_BASE ||
     memcmp(call,expected_call,sizeof(expected_call))!=0){
    if(diag)diag("PKR-AUDIO: PLAY-HOOK REFUSED unexpected original x86 CALL signature; game DirectSound untouched");
    return;
  }
  /* x86 near-call rel32 is modulo 2^32, including DLLs above 2GiB. */
  rel=(uint32_t)(uintptr_t)&native_play_with_pkr-
      ((uint32_t)(uintptr_t)call+5u);
  if(!VirtualProtect(call,sizeof(expected_call),PAGE_EXECUTE_READWRITE,&old)){
    if(diag)diag("PKR-AUDIO: PLAY-HOOK REFUSED cannot change page protection");
    return;
  }
  memcpy(call+1u,&rel,4u);
  if(!FlushInstructionCache(GetCurrentProcess(),call,sizeof(expected_call))){
    memcpy(call,expected_call,sizeof(expected_call));
    FlushInstructionCache(GetCurrentProcess(),call,sizeof(expected_call));
    VirtualProtect(call,sizeof(expected_call),old,&ignored);
    if(diag)diag("PKR-AUDIO: PLAY-HOOK REFUSED cache flush failed, original CALL restored");
    return;
  }
  if(!VirtualProtect(call,sizeof(expected_call),old,&ignored) && diag)
    diag("PKR-AUDIO: PLAY-HOOK WARNING original code-page protection not restored");
  installed=1;
  if(diag)diag("PKR-AUDIO: PLAY-HOOK INSTALLED exact original DXSOUND_Play CALL 0x472870 (native playback retained; no SDL3)");
  if(trace_enabled && diag)
    diag("PKR-AUDIO: PLAY-TRACE ARMED read-only before/after native Play; max 256 pairs; no cursor or Stop modifications");
}
