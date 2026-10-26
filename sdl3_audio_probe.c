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
typedef void (__cdecl *DestroyFn)(SDL3Stream*);
typedef void (__cdecl *FreeFn)(void*);
typedef const char* (__cdecl *ErrorFn)(void);

static void (*logfn)(const char*)=NULL;
static int on=0, attempted=0, was_f7=0;
static HMODULE module=NULL;
static char directory[MAX_PATH];
static SDL3Stream *stream=NULL;
static InitFn init=NULL;
static LoadFn load=NULL;
static OpenFn open_device=NULL;
static PutFn put=NULL;
static ResumeFn resume=NULL;
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
  destroy=(DestroyFn)GetProcAddress(module,"SDL_DestroyAudioStream");
  sdl_free=(FreeFn)GetProcAddress(module,"SDL_free");
  error_text=(ErrorFn)GetProcAddress(module,"SDL_GetError");
  if(!init||!load||!open_device||!put||!resume||!destroy||!sdl_free||!error_text){
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
void SpiderSDL3Probe_Init(void (*logger)(const char*),int enabled){
  char exe[MAX_PATH],*slash;
  DWORD n;
  logfn=logger;
  if(enabled!=1)return;
  n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(!n||n>=sizeof(exe)||(slash=strrchr(exe,'\\'))==NULL)return;
  slash[1]=0;
  if(strlen(exe)+strlen("SpiderMan2k1Fix-SDL3-test.wav")+1>=MAX_PATH)return;
  strcpy(directory,exe);
  on=1;
  if(logfn)logfn("SDL3-PROBE: ARMED; F7 plays original SpiderMan2k1Fix-SDL3-test.wav via SDL3, does NOT replace DirectSound");
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
