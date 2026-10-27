/*
 * DirectSound-native PKR original PCM tail repair.
 *
 * Valid ONLY for the already SHA256-checked supported SpideyPC.exe.
 * The single original x86 CALL at 0x00472870 points to native
 * DXSOUND_Play (0x00504230): E8 BB 19 09 00. We change its rel32
 * destination, and pass ALL calls through to the original function.
 * No native Stop/IsPlaying API hooks, SDL3, or rewritten game WAV files.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdint.h>
#include <string.h>
#include "pkr_audio_index.h"
#include "pkr_audio_play_hook.h"

#define GAME_BASE ((uintptr_t)0x00400000u)
#define VOICE_TABLE ((BYTE*)(uintptr_t)0x006BBD50u)
#define VOICE_STRIDE 12u
#define ORIGINAL_CALL ((BYTE*)(uintptr_t)0x00472870u)
#define NATIVE_PLAY ((void (__cdecl *)(int,int))(uintptr_t)0x00504230u)
static const BYTE expected_call[5]={0xE8,0xBB,0x19,0x09,0x00};
static void (*diag)(const char *)=NULL;
static int installed=0;

static void __cdecl native_play_with_pkr(int slot,int looping){
  if(installed && slot>=0 && slot<32){
    const BYTE *entry=VOICE_TABLE+(unsigned)slot*VOICE_STRIDE;
    IDirectSoundBuffer *buffer=*(IDirectSoundBuffer *const*)entry;
    if(buffer){
      SpiderPKRAudioIndex_Probe(slot,buffer,looping);
      SpiderPKRAudioIndex_TryTailRestore(slot,buffer,looping);
    }
  }
  /* ALWAYS use the game's original DirectSound Play, even if no source
   * matched, archive validation failed, or DS Lock/Unlock failed. */
  NATIVE_PLAY(slot,looping);
}

void SpiderPKRAudioPlayHook_Init(void (*logger)(const char*),int enabled){
  DWORD old=0,ignored=0;
  BYTE *call=ORIGINAL_CALL;
  uint32_t rel;
  diag=logger;
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
}
