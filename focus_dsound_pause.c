/* Spider-Man 2001 permanent native DirectSound secondary-voice focus pause.
 *
 * Original SHA-256-verified SpideyPC.exe:
 * 0x005039F0 initializes 32 game-owned 12-byte voice slots at 0x006BBD50.
 * 0x00504110 duplicates secondary buffers into voice slots.
 * 0x00504230 / 0x005042A0 play / stop the game-owned voices.
 * 0x006BBF1C identifies the excluded primary playback buffer.
 *
 * Stop only currently playing secondary voices; DirectSound preserves
 * their playback cursor. Resume an unchanged slot with its captured loop
 * flag, never a replaced or already-playing buffer. Retain a COM reference
 * across the pause. No COM vtable or import-table modification.
 * Permanent with the fix, restricted to the SHA-verified game executable.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h> /* defines WAVEFORMATEX needed by dsound.h on MinGW-w64 */
#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "focus_dsound_pause.h"

#define VOICE_COUNT 32u
#define SPIDER_VOICES ((BYTE *)(uintptr_t)0x006BBD50u)
#define SPIDER_PRIMARY (*(IDirectSoundBuffer * const *)(uintptr_t)0x006BBF1Cu)
#define SPIDER_VOICE_INIT ((const BYTE *)(uintptr_t)0x005039F0u)
#define SPIDER_DUPLICATE ((const BYTE *)(uintptr_t)0x00504110u)
#define SPIDER_VOICE_STOP ((const BYTE *)(uintptr_t)0x005042A0u)

static const BYTE voice_init_signature[]={0x83,0xEC,0x24,0x56,0x57,0xB9,0x80,0x00,0x00,0x00};
static const BYTE duplicate_signature[]={0x8B,0x0D,0x20,0x79,0x6B,0x00,0x53,0x56,0x57};
static const BYTE voice_stop_signature[]={0x8B,0x44,0x24,0x04,0x56,0x8D,0x04,0x40};
static IDirectSoundBuffer *paused_voice[VOICE_COUNT];
static DWORD paused_play_flags[VOICE_COUNT];
static volatile LONG enabled=0,armed=0;
/* Opt-in passive debugging only. Never call Play, Stop, Lock, or Set* here. */
static volatile LONG audio_trace_enabled=0;
/* Mutating experiment is separately opt-in and never runs in menus. */
static volatile LONG audio_loop_stop_enabled=0;
static unsigned audio_loop_stop_flips=0;
static unsigned audio_loop_stop_ok=0;
static unsigned audio_loop_stop_attempts=0;
static int audio_loop_stop_no_match_logged=0;
static IDirectSoundBuffer *audio_trace_previous[VOICE_COUNT];
static BYTE audio_trace_was_playing[VOICE_COUNT];
/* Compare successive positions for an unchanged, playing buffer. A backward
 * cursor may mean a loop boundary OR a game-initiated rewind/restart. */
static DWORD audio_trace_last_read[VOICE_COUNT];
static BYTE audio_trace_have_read[VOICE_COUNT];
static DWORD audio_trace_epoch=0;
static unsigned audio_trace_events=0;
static int audio_trace_in_3d=0;
static void (*logger)(const char *)=NULL;

static void diag(const char *msg){if(logger)logger(msg);}
static IDirectSoundBuffer *voice_at(unsigned slot){
  /* Game's own voice table: first pointer of each 12-byte record. */
  return *(IDirectSoundBuffer * const *)(SPIDER_VOICES+slot*12u);
}
void SpiderFocusDSound_Init(void (*log_fn)(const char *)){
  logger=log_fn;
  /* Native sound focus pause is permanent, with original-site validation. */
  /* The calling code has already verified the complete supported EXE hash
   * before any fixed-address access. Reject any unexpected site layout. */
  if(memcmp(SPIDER_VOICE_INIT,voice_init_signature,sizeof(voice_init_signature)) ||
     memcmp(SPIDER_DUPLICATE,duplicate_signature,sizeof(duplicate_signature)) ||
     memcmp(SPIDER_VOICE_STOP,voice_stop_signature,sizeof(voice_stop_signature))){
    diag("FOCUS-DSOUND: DISABLED (native 32-voice table signatures differ)");
    return;
  }
  InterlockedExchange(&enabled,1);
  diag("FOCUS-DSOUND: guarded secondary-voice pause enabled; 32 game-owned slots, primary excluded");
  /* The game's supported hash and INI have already been verified by caller.
   * The trace is OFF unless explicitly opted in under [Debug]. */
  {
    char path[MAX_PATH],*slash;
    DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
    if(n && n<sizeof(path) && (slash=strrchr(path,'\\'))!=NULL &&
       (size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1<sizeof(path)) {
      strcpy(slash+1,"SpiderMan2k1Fix.ini");
      if(GetPrivateProfileIntA("Debug","ExperimentalAudioVoiceTrace",0,path)==1) {
        InterlockedExchange(&audio_trace_enabled,1);
        diag("AUDIO-VOICE-TRACE: ARMED (passive DirectSound voice snapshot at each 3D Flip; no audio changes)");
      }
      if(GetPrivateProfileIntA("Debug","ExperimentalAudioStopSuspectLoop",0,path)==1) {
        InterlockedExchange(&audio_loop_stop_enabled,1);
        diag("AUDIO-LOOP-STOP: ARMED (experimental stop of slot 3 ONLY if playing+looping, bytes=85176, hz=12000, 3D only)");
      }
    }
  }
}
/* Sample only once per presented frame, only in native 3D, on the game
 * thread. This does NOT intercept play calls. Detect new playback states
 * and read-cursor backwards jumps on unchanged playing buffers; the latter
 * may be an end-of-loop wrap OR a manual restart. Events between samples
 * may be missed. No buffer mutation, AddRef, Lock or playback changes. */
void SpiderFocusDSound_TraceOnFlip(LONG flip,int in_3d){
  unsigned i;
  DWORD elapsed;
  if(InterlockedCompareExchange(&audio_trace_enabled,0,0)!=1 ||
     InterlockedCompareExchange(&enabled,0,0)!=1)return;
  if(!in_3d){
    audio_trace_in_3d=0;
    return;
  }
  if(!audio_trace_in_3d){
    memset(audio_trace_previous,0,sizeof(audio_trace_previous));
    memset(audio_trace_was_playing,0,sizeof(audio_trace_was_playing));
    memset(audio_trace_have_read,0,sizeof(audio_trace_have_read));
    audio_trace_events=0;
    audio_trace_epoch=GetTickCount();
    audio_trace_in_3d=1;
    diag("AUDIO-VOICE-TRACE: 3D START t=0ms (elapsed relative to first 3D Flip)");
  }
  elapsed=GetTickCount()-audio_trace_epoch;
  for(i=0;i<VOICE_COUNT;i++){
    IDirectSoundBuffer *buf=voice_at(i);
    DWORD status=0,read_cursor=0,write_cursor=0;
    HRESULT hr;
    int playing,started,rewound=0,got_cursor=0;
    if(!buf || buf==SPIDER_PRIMARY){
      audio_trace_previous[i]=NULL;
      audio_trace_was_playing[i]=0;
      audio_trace_have_read[i]=0;
      continue;
    }
    hr=IDirectSoundBuffer_GetStatus(buf,&status);
    if(FAILED(hr)){
      audio_trace_previous[i]=buf;
      audio_trace_was_playing[i]=0;
      audio_trace_have_read[i]=0;
      continue;
    }
    playing=(status & DSBSTATUS_PLAYING) && !(status & DSBSTATUS_BUFFERLOST);
    started=playing && (!audio_trace_was_playing[i] || audio_trace_previous[i]!=buf);
    if(playing){
      got_cursor=SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(
                             buf,&read_cursor,&write_cursor));
      if(got_cursor && !started && audio_trace_have_read[i] &&
         read_cursor<audio_trace_last_read[i])
        rewound=1;
    }
    if(started || rewound){
      if(audio_trace_events<600u){
        DSBCAPS caps;
        DWORD frequency=0;
        char line[256];
        memset(&caps,0,sizeof(caps));
        caps.dwSize=sizeof(caps);
        if(FAILED(IDirectSoundBuffer_GetCaps(buf,&caps)))caps.dwBufferBytes=0;
        if(FAILED(IDirectSoundBuffer_GetFrequency(buf,&frequency)))frequency=0;
        snprintf(line,sizeof(line),
          "AUDIO-VOICE-TRACE: %s t=%lums flip=%ld slot=%u buffer=%p status=0x%08lX read=%lu prev=%lu write=%lu bytes=%lu hz=%lu",
          rewound?"CURSOR-REWIND":"PLAY",
          (unsigned long)elapsed,(long)flip,i,(void*)buf,
          (unsigned long)status,(unsigned long)read_cursor,
          (unsigned long)(audio_trace_have_read[i]?audio_trace_last_read[i]:0),
          (unsigned long)write_cursor,(unsigned long)caps.dwBufferBytes,
          (unsigned long)frequency);
        diag(line);
      }else if(audio_trace_events==600u){
        diag("AUDIO-VOICE-TRACE: LIMIT 600 PLAY/REWIND events reached; further lines suppressed");
      }
      audio_trace_events++;
    }
    audio_trace_previous[i]=buf;
    audio_trace_was_playing[i]=(BYTE)!!playing;
    audio_trace_have_read[i]=(BYTE)(playing && got_cursor);
    if(playing && got_cursor)audio_trace_last_read[i]=read_cursor;
  }
}
/* Targeted A/B experiment for the observed 3.549-second looping voice.
 * This is not an audio fix: it deliberately changes one secondary buffer.
 * Signature and supported-EXE guards are already enforced in Init.
 * No COM hooks, primary buffer changes, volume changes, or auto-resume.
 */
void SpiderFocusDSound_StopSuspectOnFlip(LONG flip,int in_3d){
  IDirectSoundBuffer *buf;
  DWORD status=0,frequency=0;
  DSBCAPS caps;
  HRESULT hr;
  if(InterlockedCompareExchange(&enabled,0,0)!=1 ||
     InterlockedCompareExchange(&audio_loop_stop_enabled,0,0)!=1)return;
  if(!in_3d){
    audio_loop_stop_flips=0;
    audio_loop_stop_ok=0;
    audio_loop_stop_attempts=0;
    audio_loop_stop_no_match_logged=0;
    return;
  }
  audio_loop_stop_flips++;
  buf=voice_at(3u);
  if(buf && buf!=SPIDER_PRIMARY &&
     SUCCEEDED(IDirectSoundBuffer_GetStatus(buf,&status)) &&
     (status & (DSBSTATUS_PLAYING|DSBSTATUS_LOOPING))==
               (DSBSTATUS_PLAYING|DSBSTATUS_LOOPING) &&
     !(status & DSBSTATUS_BUFFERLOST)){
    memset(&caps,0,sizeof(caps));
    caps.dwSize=sizeof(caps);
    if(SUCCEEDED(IDirectSoundBuffer_GetCaps(buf,&caps)) &&
       caps.dwBufferBytes==85176u &&
       SUCCEEDED(IDirectSoundBuffer_GetFrequency(buf,&frequency)) &&
       frequency==12000u){
      audio_loop_stop_attempts++;
      hr=IDirectSoundBuffer_Stop(buf);
      if(SUCCEEDED(hr))audio_loop_stop_ok++;
      if(audio_loop_stop_attempts<=12u){
        char line[220];
        snprintf(line,sizeof(line),
          "AUDIO-LOOP-STOP: %s flip=%ld slot=3 buffer=%p status-before=0x%08lX bytes=%lu hz=%lu hr=0x%08lX success=%u",
          SUCCEEDED(hr)?"STOP":"FAIL",(long)flip,(void *)buf,
          (unsigned long)status,(unsigned long)caps.dwBufferBytes,
          (unsigned long)frequency,(unsigned long)hr,audio_loop_stop_ok);
        diag(line);
      }else if(audio_loop_stop_attempts==13u)
        diag("AUDIO-LOOP-STOP: further stop attempts suppressed; still stopping matched voice");
      return;
    }
  }
  if(audio_loop_stop_flips==600u && audio_loop_stop_ok==0u &&
     !audio_loop_stop_no_match_logged){
    diag("AUDIO-LOOP-STOP: NO MATCH after 600 3D Flips; no buffer stopped; cannot infer audio cause");
    audio_loop_stop_no_match_logged=1;
  }
}
void SpiderFocusDSound_Pause(void){
  unsigned i,found=0,stopped=0,failed=0;
  if(InterlockedCompareExchange(&enabled,0,0)!=1 ||
     InterlockedCompareExchange(&armed,1,0)!=0)return;
  memset(paused_voice,0,sizeof(paused_voice));
  memset(paused_play_flags,0,sizeof(paused_play_flags));
  for(i=0;i<VOICE_COUNT;i++){
    IDirectSoundBuffer *buf=voice_at(i);
    DWORD status=0;
    HRESULT hr;
    if(!buf || buf==SPIDER_PRIMARY)continue;
    found++;
    hr=IDirectSoundBuffer_GetStatus(buf,&status);
    if(FAILED(hr)){failed++;continue;}
    if(!(status & DSBSTATUS_PLAYING) || (status & DSBSTATUS_BUFFERLOST))continue;
    /* Preserve a reference to a buffer stopped by us, not an unowned COM
     * pointer. This is also safe if the game releases/replaces its slot. */
    IDirectSoundBuffer_AddRef(buf);
    hr=IDirectSoundBuffer_Stop(buf);
    if(SUCCEEDED(hr)){
      paused_voice[i]=buf;
      paused_play_flags[i]=(status & DSBSTATUS_LOOPING)?DSBPLAY_LOOPING:0;
      stopped++;
    }else{
      IDirectSoundBuffer_Release(buf);
      failed++;
    }
  }
  if(logger){
    char line[160];
    snprintf(line,sizeof(line),
      "FOCUS-DSOUND: pause slots=%u occupied=%u playing-stopped=%u failed=%u",
      (unsigned)VOICE_COUNT,found,stopped,failed);
    diag(line);
  }
}
void SpiderFocusDSound_Resume(void){
  unsigned i,resumed=0,replaced=0,failed=0;
  if(InterlockedCompareExchange(&armed,0,1)!=1)return;
  for(i=0;i<VOICE_COUNT;i++){
    IDirectSoundBuffer *buf=paused_voice[i];
    if(buf){
      if(voice_at(i)==buf){
        DWORD status=0;
        HRESULT hr=IDirectSoundBuffer_GetStatus(buf,&status);
        if(SUCCEEDED(hr) && !(status & DSBSTATUS_PLAYING) &&
           !(status & DSBSTATUS_BUFFERLOST)){
          hr=IDirectSoundBuffer_Play(buf,0,0,paused_play_flags[i]);
          if(SUCCEEDED(hr))resumed++;
          else failed++;
        }else if(FAILED(hr))failed++;
        else if(status & DSBSTATUS_BUFFERLOST)failed++;
        /* If native game code restarted the voice itself, leave it alone. */
      }else replaced++;
      IDirectSoundBuffer_Release(buf);
      paused_voice[i]=NULL;
      paused_play_flags[i]=0;
    }
  }
  if(logger){
    char line[160];
    snprintf(line,sizeof(line),
      "FOCUS-DSOUND: resume playing-restored=%u slot-replaced=%u failed=%u",
      resumed,replaced,failed);
    diag(line);
  }
}
