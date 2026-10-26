/*
 * Optional F7 standalone SDL3 WAV audition, NOT gameplay engine takeover.
 * The original DirectSound, PKR, WAV loader and game voices are unchanged.
 * No SDL headers or import libraries required: dynamically-loaded x86 DLL.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <wincrypt.h>
#define COBJMACROS
#include <mmsystem.h>
#include <dsound.h>
#include "sdl3_audio_probe.h"

#define SDL_AUDIO_INIT_FLAG 0x10u
#define SDL_DEFAULT_PLAYBACK 0xFFFFFFFFu
#define MAX_PCM_BYTES (16u*1024u*1024u)
typedef struct {uint32_t format;int channels;int freq;} SDL3Spec;
typedef struct SDL3Stream SDL3Stream;
typedef bool (__cdecl *InitFn)(uint32_t);
typedef bool (__cdecl *LoadFn)(const char*,SDL3Spec*,uint8_t**,uint32_t*);
typedef SDL3Stream* (__cdecl *OpenFn)(uint32_t,const SDL3Spec*,void*,void*);
typedef bool (__cdecl *PutFn)(SDL3Stream*,const void*,int);
typedef bool (__cdecl *ResumeFn)(SDL3Stream*);
typedef bool (__cdecl *PauseFn)(SDL3Stream*);
typedef void (__cdecl *DestroyFn)(SDL3Stream*);
typedef void (__cdecl *FreeFn)(void*);
typedef const char* (__cdecl *ErrorFn)(void);

static void (*logfn)(const char*)=NULL;
static int on=0, attempted=0, was_f7=0, swing_mode=0;
static int swing_source_attempted=0,swing_source_ready=0;
static uint8_t swing_source_pcm[32632];
/* Each native slot owns at most one SDL3 stream and retains a COM
 * reference to the exact DS buffer whose Play was suppressed. */
typedef struct {
  SDL3Stream *audio;
  IDirectSoundBuffer *original;
  LONG volume,pan;
  DWORD hz,last_tick;
  uint64_t position_milli_frames; /* frame index *1000; no clock drift */
  int paused;
} SwingVoice;
static SwingVoice swing_voices[32];
static CRITICAL_SECTION voice_cs;
static int voice_cs_ready=0,menu_was_active=1;
#define GAME_VOICE_TABLE ((BYTE*)(uintptr_t)0x006BBD50u)
#define SWING_FRAMES (32632u/2u)
static HMODULE module=NULL;
static char directory[MAX_PATH];
static SDL3Stream *stream=NULL;
static InitFn init=NULL;
static LoadFn load=NULL;
static OpenFn open_device=NULL;
static PutFn put=NULL;
static ResumeFn resume=NULL;
static PauseFn pause_audio=NULL;
static DestroyFn destroy=NULL;
static FreeFn sdl_free=NULL;
static ErrorFn error_text=NULL;

static void error(const char *step){
  char line[430];
  const char *detail=error_text?error_text():"unknown SDL error";
  snprintf(line,sizeof(line),"SDL3-PROBE: %s: %.290s",step,detail?detail:"unknown");
  if(logfn)logfn(line);
}
static int initialize_sdl(void){
  char path[MAX_PATH];
  if(attempted)return module!=NULL;
  attempted=1;
  if(strlen(directory)+strlen("SDL3.dll")+1>=MAX_PATH)return 0;
  snprintf(path,sizeof(path),"%sSDL3.dll",directory);
  module=LoadLibraryA(path); /* Never search a global / untrusted path. */
  if(!module){
    if(logfn)logfn("SDL3-PROBE: missing or invalid 32-bit SDL3.dll beside SpideyPC.exe");
    return 0;
  }
  init=(InitFn)GetProcAddress(module,"SDL_InitSubSystem");
  load=(LoadFn)GetProcAddress(module,"SDL_LoadWAV");
  open_device=(OpenFn)GetProcAddress(module,"SDL_OpenAudioDeviceStream");
  put=(PutFn)GetProcAddress(module,"SDL_PutAudioStreamData");
  resume=(ResumeFn)GetProcAddress(module,"SDL_ResumeAudioStreamDevice");
  pause_audio=(PauseFn)GetProcAddress(module,"SDL_PauseAudioStreamDevice");
  destroy=(DestroyFn)GetProcAddress(module,"SDL_DestroyAudioStream");
  sdl_free=(FreeFn)GetProcAddress(module,"SDL_free");
  error_text=(ErrorFn)GetProcAddress(module,"SDL_GetError");
  if(!init||!load||!open_device||!put||!resume||!pause_audio||!destroy||!sdl_free||!error_text){
    if(logfn)logfn("SDL3-PROBE: required SDL3 entrypoint absent; no sound played");
    FreeLibrary(module);module=NULL;return 0;
  }
  if(!init(SDL_AUDIO_INIT_FLAG)){
    error("SDL_InitSubSystem audio failed");
    FreeLibrary(module);module=NULL;return 0;
  }
  if(logfn)logfn("SDL3-PROBE: audio initialized; DirectSound still controls every game voice");
  return 1;
}
void SpiderSDL3Probe_Init(void (*logger)(const char*),int enabled,int replace_swing5){
  char exe[MAX_PATH],*slash;
  DWORD n;
  logfn=logger;
  swing_mode=(replace_swing5==1);
  on=(enabled==1);
  if(swing_mode && !voice_cs_ready){
    InitializeCriticalSection(&voice_cs);
    voice_cs_ready=1;
  }
  if(!on && !swing_mode)return;
  n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(!n||n>=sizeof(exe)||(slash=strrchr(exe,'\\'))==NULL)return;
  slash[1]=0;
  if(strlen(exe)+strlen("SpiderMan2k1Fix-SDL3-test.wav")+1>=MAX_PATH)return;
  strcpy(directory,exe);
  if(on && logfn)logfn("SDL3-PROBE: ARMED; F7 plays original SpiderMan2k1Fix-SDL3-test.wav via SDL3");
  if(swing_mode && logfn)logfn("SDL3-SWING5: ARMED; exact game PCM + source WAV SHA256 required; only Swing5 may bypass native DirectSound Play");
}
static void play(void){
  SDL3Spec fmt={0};
  uint8_t *audio=NULL;
  uint32_t bytes=0;
  SDL3Stream *new_stream;
  char path[MAX_PATH],msg[260];
  if(!initialize_sdl())return;
  snprintf(path,sizeof(path),"%sSpiderMan2k1Fix-SDL3-test.wav",directory);
  if(!load(path,&fmt,&audio,&bytes)){
    error("SDL_LoadWAV failed; place original WAV in game directory");
    return;
  }
  if(!audio||bytes==0||bytes>MAX_PCM_BYTES||
     fmt.channels<1||fmt.channels>2||fmt.freq<8000||fmt.freq>96000){
    if(logfn)logfn("SDL3-PROBE: refused WAV outside size/format test limits");
    sdl_free(audio);return;
  }
  if(stream){destroy(stream);stream=NULL;}
  new_stream=open_device(SDL_DEFAULT_PLAYBACK,&fmt,NULL,NULL);
  if(!new_stream){
    error("SDL_OpenAudioDeviceStream failed");
    sdl_free(audio);return;
  }
  if(!put(new_stream,audio,(int)bytes)){
    error("SDL_PutAudioStreamData failed");
    destroy(new_stream);sdl_free(audio);return;
  }
  sdl_free(audio);
  if(!resume(new_stream)){
    error("SDL_ResumeAudioStreamDevice failed");
    destroy(new_stream);return;
  }
  stream=new_stream;
  snprintf(msg,sizeof(msg),
    "SDL3-PROBE: PLAY original WAV bytes=%lu hz=%d channels=%d format=0x%lX (game DirectSound unaffected)",
    (unsigned long)bytes,fmt.freq,fmt.channels,(unsigned long)fmt.format);
  if(logfn)logfn(msg);
}
void SpiderSDL3Probe_OnFlip(HWND game){
  int down;
  if(!on)return;
  if(!game||GetForegroundWindow()!=game){was_f7=0;return;}
  down=(GetAsyncKeyState(VK_F7)&0x8000)!=0;
  if(down&&!was_f7)play();
  was_f7=down;
}

/* Exact ORIGINAL PCM checksum from user-provided Swing5.wav (PCM only).
 * This is deliberately different from the game's damaged buffer checksum,
 * which is checked by the native hook before this code is ever called. */
static const BYTE clean_swing_sha[32]={
  0x73,0x0c,0xca,0x40,0x57,0xc9,0x22,0x5c,
  0x80,0xba,0x1a,0xc0,0x99,0x46,0xa3,0x8a,
  0xc9,0x33,0x1b,0x1a,0x7b,0x49,0x1f,0x43,
  0x7d,0x0b,0xdc,0xd0,0x0e,0x3d,0x17,0x00
};
static int clean_pcm_digest(const BYTE *src,DWORD size){
  HCRYPTPROV provider=0;
  HCRYPTHASH hash=0;
  DWORD count=32;
  BYTE digest[32];
  int success=0;
  if(CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){
    if(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) &&
       CryptHashData(hash,src,size,0) &&
       CryptGetHashParam(hash,HP_HASHVAL,digest,&count,0) &&
       count==sizeof(digest) &&
       memcmp(digest,clean_swing_sha,sizeof(digest))==0)
      success=1;
    if(hash)CryptDestroyHash(hash);
    CryptReleaseContext(provider,0);
  }
  return success;
}
static int load_verified_swing_source(void){
  SDL3Spec spec={0};
  uint8_t *decoded=NULL;
  uint32_t bytes=0;
  char file[MAX_PATH];
  if(swing_source_ready)return 1;
  if(swing_source_attempted)return 0; /* refuse repeated disk I/O in Play */
  swing_source_attempted=1;
  if(!initialize_sdl())return 0;
  snprintf(file,sizeof(file),"%sSpiderMan2k1Fix-SDL3-test.wav",directory);
  if(!load(file,&spec,&decoded,&bytes)){
    error("SDL3-SWING5: original source WAV not available; native Play fallback");
    return 0;
  }
  if(decoded && bytes==sizeof(swing_source_pcm) &&
     spec.freq==16000 && spec.channels==1 && spec.format==0x8010u &&
     clean_pcm_digest(decoded,bytes)){
    memcpy(swing_source_pcm,decoded,bytes);
    swing_source_ready=1;
  }else if(logfn){
    logfn("SDL3-SWING5: REFUSED source WAV not exact clean Swing5 PCM SHA256/size/16kHz mono16");
  }
  if(decoded)sdl_free(decoded);
  return swing_source_ready;
}
/* This is called only after the game-owned secondary DS buffer matches
 * the exact pre-observed live Swing5 PCM SHA-256, never by F7.
 *
 * SDL3 receives a separate stream per native voice slot so short sounds
 * can overlap. The game-owned buffer is never modified. If SDL3 fails,
 * the original native DXSOUND_Play is called instead. */
int SpiderSDL3Probe_PlaySwing5(int slot,IDirectSoundBuffer *original,LONG ds_volume,LONG ds_pan,DWORD actual_hz){
  SDL3Spec spec={0};
  SDL3Stream *new_stream=NULL;
  int16_t *stereo=NULL;
  DWORD i;
  double left,right;
  char message[300];
  const size_t frame_count=sizeof(swing_source_pcm)/2u;
  if(!swing_mode || !original || !voice_cs_ready || slot<0 || slot>=32 ||
     ds_volume>0 || ds_volume<-10000 ||
     ds_pan>10000 || ds_pan<-10000 ||
     actual_hz<100u || actual_hz>100000u)
    return 0;
  if(!load_verified_swing_source())return 0;

  /* DirectSound volume and pan are hundredths of dB. Translate the
   * mono original to independent left/right channels with matching
   * attenuation. Clip after scaling, never write to the source bytes. */
  left=pow(10.0,(double)(ds_volume-(ds_pan>0?ds_pan:0))/2000.0);
  right=pow(10.0,(double)(ds_volume+(ds_pan<0?ds_pan:0))/2000.0);
  stereo=(int16_t*)HeapAlloc(GetProcessHeap(),0,frame_count*4u);
  if(!stereo){
    if(logfn)logfn("SDL3-SWING5: allocation failed, native Play fallback");
    return 0;
  }
  for(i=0;i<(DWORD)frame_count;i++){
    int16_t value=(int16_t)((uint16_t)swing_source_pcm[i*2u] |
                             (uint16_t)(swing_source_pcm[i*2u+1u]<<8));
    stereo[i*2u]=(int16_t)((double)value*left);
    stereo[i*2u+1u]=(int16_t)((double)value*right);
  }
  spec.format=0x8010u; /* signed 16-bit little-endian */
  spec.channels=2;
  spec.freq=(int)actual_hz; /* preserve native pitch modulation */
  new_stream=open_device(SDL_DEFAULT_PLAYBACK,&spec,NULL,NULL);
  if(!new_stream){
    error("SDL3-SWING5: SDL_OpenAudioDeviceStream failed; native fallback");
    HeapFree(GetProcessHeap(),0,stereo);
    return 0;
  }
  if(!put(new_stream,stereo,(int)(frame_count*4u)) || !resume(new_stream)){
    error("SDL3-SWING5: audio queue/resume failed; native fallback");
    destroy(new_stream);
    HeapFree(GetProcessHeap(),0,stereo);
    return 0;
  }
  HeapFree(GetProcessHeap(),0,stereo);
  EnterCriticalSection(&voice_cs);
  if(swing_voices[slot].audio){
    destroy(swing_voices[slot].audio);
    IDirectSoundBuffer_Release(swing_voices[slot].original);
  }
  IDirectSoundBuffer_AddRef(original);
  memset(&swing_voices[slot],0,sizeof(swing_voices[slot]));
  swing_voices[slot].audio=new_stream;
  swing_voices[slot].original=original;
  swing_voices[slot].volume=ds_volume;
  swing_voices[slot].pan=ds_pan;
  swing_voices[slot].hz=actual_hz;
  swing_voices[slot].last_tick=GetTickCount();
  LeaveCriticalSection(&voice_cs);
  snprintf(message,sizeof(message),
    "SDL3-SWING5: REPLACED slot=%d original_pcm=32632 hz=%lu ds_volume=%ld ds_pan=%ld channels=2 (original native DirectSound Play skipped)",
    slot,(unsigned long)actual_hz,(long)ds_volume,(long)ds_pan);
  if(logfn)logfn(message);
  return 1;
}
