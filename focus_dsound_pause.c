/* Spider-Man 2001 opt-in native DirectSound secondary-voice pause.
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
 * Disabled by default, restricted to the supported executable.
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
static void (*logger)(const char *)=NULL;

static void diag(const char *msg){if(logger)logger(msg);}
static IDirectSoundBuffer *voice_at(unsigned slot){
  /* Game's own voice table: first pointer of each 12-byte record. */
  return *(IDirectSoundBuffer * const *)(SPIDER_VOICES+slot*12u);
}
void SpiderFocusDSound_Init(void (*log_fn)(const char *)){
  char path[MAX_PATH],*slash;
  DWORD n;
  logger=log_fn;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(!n || n>=sizeof(path) || !(slash=strrchr(path,'\\')) ||
    (size_t)(slash-path)+sizeof("SpiderMan2k1Fix.ini")>sizeof(path))return;
  strcpy(slash+1,"SpiderMan2k1Fix.ini");
  if(GetPrivateProfileIntA("Input","PauseDirectSoundOnFocusLoss",0,path)!=1)return;
  /* The calling code has already verified the complete supported EXE hash
   * before any fixed-address access. Reject any unexpected site layout. */
  if(memcmp(SPIDER_VOICE_INIT,voice_init_signature,sizeof(voice_init_signature)) ||
     memcmp(SPIDER_DUPLICATE,duplicate_signature,sizeof(duplicate_signature)) ||
     memcmp(SPIDER_VOICE_STOP,voice_stop_signature,sizeof(voice_stop_signature))){
    diag("FOCUS-DSOUND: DISABLED (native 32-voice table signatures differ)");
    return;
  }
  InterlockedExchange(&enabled,1);
  diag("FOCUS-DSOUND: experimental secondary-voice pause enabled; 32 game-owned slots, primary excluded");
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
