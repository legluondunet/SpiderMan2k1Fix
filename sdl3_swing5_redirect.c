/*
 * Opt-in exact-SHA Swing5 native callsite replacement, game EXE SHA
 * verified by parent before this is installed. All other sounds use
 * native DirectSound without alteration.
 *
 * Original one-shot CALL instruction: SpideyPC.exe 0x00472870
 * (E8 BB 19 09 00 -> 0x00504230, DXSOUND_Play).
 * Caller-tested EXE: 0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <wincrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_audio_probe.h"
#include "sdl3_swing5_redirect.h"

#define GAME_BASE ((uintptr_t)0x00400000u)
#define VOICE_TABLE ((BYTE*)(uintptr_t)0x006BBD50u)
#define VOICE_STRIDE 12u
#define ONE_SHOT_CALL ((BYTE*)(uintptr_t)0x00472870u)
#define GAME_PLAY ((void (__cdecl *)(int,int))(uintptr_t)0x00504230u)
#define GAME_STOP_ADDRESS ((BYTE*)(uintptr_t)0x005042A0u)
#define NATIVE_STOP_PATCH_BYTES 8u
static const BYTE expected_stop[NATIVE_STOP_PATCH_BYTES]={
  0x8B,0x44,0x24,0x04,0x56,0x8D,0x04,0x40
};
static void (__cdecl *original_stop)(int)=NULL;
static BYTE *stop_trampoline=NULL;
#define SWING_BYTES 32632u
static const BYTE expected_call[5]={0xE8,0xBB,0x19,0x09,0x00};
static const BYTE corrupt_swing_sha[32]={
  0x5b,0x56,0x23,0x48,0x07,0x7d,0xdb,0x8c,
  0xb7,0x7e,0xda,0xbc,0xd5,0x22,0xe1,0xb1,
  0x78,0x9a,0x74,0xc6,0xb7,0x85,0x2c,0x86,
  0x34,0x4c,0x1e,0x88,0x7f,0x62,0x09,0xad
};
static void (*log_fn)(const char*)=NULL;
static int installed=0;
static volatile LONG matches=0,fallbacks=0;
static void __cdecl maybe_stop_sdl3(int slot){
  SpiderSDL3Probe_StopSlot(slot,"native-DXSOUND_Stop");
  if(original_stop)original_stop(slot);
}

static int exact_corrupt_buffer(IDirectSoundBuffer *buf){
  DSBCAPS caps;
  WAVEFORMATEX format;
  DWORD got=0,length1=0,length2=0,digest_length=32;
  void *p1=NULL,*p2=NULL;
  HCRYPTPROV provider=0;
  HCRYPTHASH hash=0;
  BYTE digest[32];
  int ok=0,unlock_ok;
  HRESULT hr;
  if(!buf)return 0;
  memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
  memset(&format,0,sizeof(format));
  if(FAILED(IDirectSoundBuffer_GetCaps(buf,&caps)) ||
     caps.dwBufferBytes!=SWING_BYTES ||
     FAILED(IDirectSoundBuffer_GetFormat(buf,&format,sizeof(format),&got)) ||
     got<16u || format.wFormatTag!=WAVE_FORMAT_PCM ||
     format.nChannels!=1 || format.nSamplesPerSec!=16000u ||
     format.wBitsPerSample!=16 || format.nBlockAlign!=2)
    return 0;
  hr=IDirectSoundBuffer_Lock(buf,0,SWING_BYTES,
                             &p1,&length1,&p2,&length2,0);
  if(FAILED(hr))return 0;
  if(p1 && length1+length2==SWING_BYTES && (!length2||p2) &&
     CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){
    if(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) &&
       CryptHashData(hash,(BYTE*)p1,length1,0) &&
       (!length2||CryptHashData(hash,(BYTE*)p2,length2,0)) &&
       CryptGetHashParam(hash,HP_HASHVAL,digest,&digest_length,0) &&
       digest_length==sizeof(digest) &&
       memcmp(digest,corrupt_swing_sha,sizeof(digest))==0)
      ok=1;
  }
  if(hash)CryptDestroyHash(hash);
  if(provider)CryptReleaseContext(provider,0);
  unlock_ok=SUCCEEDED(IDirectSoundBuffer_Unlock(buf,p1,length1,p2,length2));
  return unlock_ok && ok;
}
static void __cdecl maybe_play_sdl3(int slot,int looping){
  BYTE *entry;
  IDirectSoundBuffer *buf;
  LONG volume=0,pan=0;
  DWORD hz=0;
  char line[220];
  if(!installed || looping!=0 || slot<0 || slot>=32){
    GAME_PLAY(slot,looping);
    return;
  }
  /* A slot can be reused with another sound without an intervening Stop.
   * Never let the earlier SDL3 stream continue on this newly owned slot. */
  SpiderSDL3Probe_StopSlot(slot,"native-voice-replay");
  entry=VOICE_TABLE+(unsigned)slot*VOICE_STRIDE;
  buf=*(IDirectSoundBuffer**)entry;
  if(buf && exact_corrupt_buffer(buf)){
    if(SUCCEEDED(IDirectSoundBuffer_GetVolume(buf,&volume)) &&
       SUCCEEDED(IDirectSoundBuffer_GetPan(buf,&pan)) &&
       SUCCEEDED(IDirectSoundBuffer_GetFrequency(buf,&hz)) &&
       SpiderSDL3Probe_PlaySwing5(slot,buf,volume,pan,hz)){
      /* Only the two metadata writes performed by original DXSOUND_Play:
       * field_8=0, field_9=(looping!=0); the native COM Play is skipped.
       * The original DirectSound voice remains stopped and reusable.
       * Prototype limitation: SDL3 playback isn't owned by game Stop(). */
      entry[8]=0;
      entry[9]=0;
      InterlockedIncrement(&matches);
      return;
    }
    if(InterlockedIncrement(&fallbacks)<=4 && log_fn){
      snprintf(line,sizeof(line),
        "SDL3-SWING5: FALLBACK original DirectSound Play slot=%d (SDL3 source/device unavailable)",
        slot);
      log_fn(line);
    }
  }
  GAME_PLAY(slot,looping); /* Fail OPEN for all unmatched/failed cases. */
}
void SpiderSDL3Swing5_Init(void (*logger)(const char*),int enabled){
  DWORD old=0,ignored=0;
  uint32_t rel;
  BYTE *page=(BYTE*)(uintptr_t)0x00472000u;
  log_fn=logger;
  if(enabled!=1)return;
  if((uintptr_t)GetModuleHandleA(NULL)!=GAME_BASE ||
     memcmp(ONE_SHOT_CALL,expected_call,sizeof(expected_call))!=0){
    if(log_fn)log_fn("SDL3-SWING5: REFUSED unexpected executable relocation/opcodes; original DirectSound untouched");
    return;
  }
  if(!VirtualProtect(page,4096u,PAGE_EXECUTE_READWRITE,&old)){
    if(log_fn)log_fn("SDL3-SWING5: REFUSED VirtualProtect failed");
    return;
  }
  rel=(uint32_t)(uintptr_t)&maybe_play_sdl3-
      ((uint32_t)(uintptr_t)ONE_SHOT_CALL+5u);
  memcpy(ONE_SHOT_CALL+1u,&rel,4u);
  if(!FlushInstructionCache(GetCurrentProcess(),ONE_SHOT_CALL,5u)){
    memcpy(ONE_SHOT_CALL,expected_call,sizeof(expected_call));
    FlushInstructionCache(GetCurrentProcess(),ONE_SHOT_CALL,5u);
    VirtualProtect(page,4096u,old,&ignored);
    if(log_fn)log_fn("SDL3-SWING5: REFUSED instruction-cache flush; original call restored");
    return;
  }
  if(!VirtualProtect(page,4096u,old,&ignored) && log_fn)
    log_fn("SDL3-SWING5: WARNING code-page protection restore failed");
  installed=1;
  if(log_fn)log_fn("SDL3-SWING5: INSTALLED exact game CALL 0x472870; corrupt PCM SHA256 gated; fallback original DirectSound enabled; experimental");
}
