/*
 * Three user-auditioned Spider-Man (2001) one-shot PCM suffix anomalies.
 *
 * Experimental, disabled by default; only buffers matching an exact PCM
 * SHA-256, byte count and sample rate can be modified. This is NOT a
 * general file loader fix; no source files, looping sounds, or voice
 * playback controls are touched. In-place Lock/Unlock may affect timing.
 *
 * Original PCM was captured from DirectSound game-owned buffers in
 * two separate F8 diagnostic sessions, 2026-10-26.
 * Every bad suffix begins at an observed 2048-byte RIFF boundary:
 *     PCM offset = N*2048 - 44 (ordinary 44-byte WAV header).
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <wincrypt.h>
#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "audio_combat_tail_repair.h"

typedef struct {
  const char *name;
  DWORD bytes,rate,bad_byte,fade_samples;
  const char *sha256;
} BadTail;
static const BadTail tails[]={
  {"R2-03",29924u,16000u,14u*2048u-44u,240u,
   "910fbf35919a16880f6b7783828b767ed92f28c1a8c37e5bef8200ff297fc892"},
  {"R2-07",15140u,16000u,7u*2048u-44u,240u,
   "f61d96ce26df86781b7dc8793db99dc9dcc834135a1b079052a56710cc18980c"},
  {"R1-08",10128u,22050u,4u*2048u-44u,331u,
   "16d320a6c7a48f99d74abb62ec92cc668f080220db1b7b4b708d0088f5e3bf07"}
};
#define TAIL_COUNT (sizeof(tails)/sizeof(tails[0]))
#define MAX_CANDIDATES 512u
static void (*emit)(const char *)=NULL;
static int armed=0;
static volatile LONG checked=0;

static BYTE read_byte(const BYTE *a,DWORD a_bytes,
                      const BYTE *b,DWORD index){
  return index<a_bytes?a[index]:b[index-a_bytes];
}
static void write_byte(BYTE *a,DWORD a_bytes,BYTE *b,DWORD index,BYTE value){
  if(index<a_bytes)a[index]=value;
  else b[index-a_bytes]=value;
}
static int read_s16(const BYTE *a,DWORD a_bytes,
                    const BYTE *b,DWORD index){
  DWORD byte=index*2u;
  unsigned low=read_byte(a,a_bytes,b,byte);
  unsigned high=read_byte(a,a_bytes,b,byte+1u);
  return (int)(int16_t)(uint16_t)(low|(high<<8));
}
static void write_s16(BYTE *a,DWORD a_bytes,BYTE *b,DWORD index,int value){
  uint16_t word=(uint16_t)(int16_t)value;
  write_byte(a,a_bytes,b,index*2u,(BYTE)(word&255u));
  write_byte(a,a_bytes,b,index*2u+1u,(BYTE)(word>>8));
}
void SpiderCombatTailRepair_Init(void (*log_fn)(const char *),int enabled){
  emit=log_fn;
  armed=(enabled==1);
  InterlockedExchange(&checked,0);
  if(armed && emit)
    emit("AUDIO-COMBAT-TAIL: ARMED exact-SHA256 only; 3 user-confirmed one-shot tails; 15ms fade + silence; ORIGINAL ASSETS untouched; diagnostic OFF by default");
}
void SpiderCombatTailRepair_Probe(int slot,IDirectSoundBuffer *buffer,
                                  DWORD bytes,DWORD rate,int looping){
  const BadTail *target=NULL;
  unsigned i;
  DWORD got=0,part1_bytes=0,part2_bytes=0,digestlen=32;
  DSBCAPS caps;
  WAVEFORMATEX fmt;
  void *part1=NULL,*part2=NULL;
  HCRYPTPROV provider=0;
  HCRYPTHASH hash=0;
  BYTE digest[32];
  char actual[65],msg[380];
  HRESULT hr;
  int hashed=1,mutated=0;
  BYTE *a,*b;
  DWORD seam_sample;

  if(!armed || !buffer || looping || slot<0 || slot>=32)return;
  for(i=0;i<TAIL_COUNT;i++)
    if(tails[i].bytes==bytes && tails[i].rate==rate){
      target=&tails[i];
      break;
    }
  if(!target)return;
  if((unsigned)InterlockedIncrement(&checked)>MAX_CANDIDATES)return;
  memset(&fmt,0,sizeof(fmt));
  memset(&caps,0,sizeof(caps));
  caps.dwSize=sizeof(caps);
  hr=IDirectSoundBuffer_GetFormat(buffer,&fmt,sizeof(fmt),&got);
  if(FAILED(hr)||got<16u||fmt.wFormatTag!=WAVE_FORMAT_PCM||
     fmt.nChannels!=1u||fmt.wBitsPerSample!=16u||
     fmt.nSamplesPerSec!=rate||fmt.nBlockAlign!=2u||
     FAILED(IDirectSoundBuffer_GetCaps(buffer,&caps))||
     caps.dwBufferBytes!=bytes)return;
  hr=IDirectSoundBuffer_Lock(buffer,0u,bytes,
                            &part1,&part1_bytes,&part2,&part2_bytes,0u);
  if(FAILED(hr))return;
  if(!part1 || part1_bytes+part2_bytes!=bytes ||
     (part2_bytes && !part2)){
    IDirectSoundBuffer_Unlock(buffer,part1,part1_bytes,part2,part2_bytes);
    return;
  }
  a=(BYTE*)part1;b=(BYTE*)part2;
  hashed=CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,
                             CRYPT_VERIFYCONTEXT)!=0;
  if(hashed)hashed=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
  if(hashed)hashed=CryptHashData(hash,a,part1_bytes,0)!=0;
  if(hashed && part2_bytes)hashed=CryptHashData(hash,b,part2_bytes,0)!=0;
  if(hashed)hashed=CryptGetHashParam(hash,HP_HASHVAL,digest,
                                    &digestlen,0)!=0 && digestlen==32u;
  if(hash)CryptDestroyHash(hash);
  if(provider)CryptReleaseContext(provider,0);
  if(hashed){
    for(i=0;i<32u;i++)
      sprintf(actual+i*2u,"%02x",(unsigned)digest[i]);
    actual[64]=0;
    if(strcmp(actual,target->sha256)==0 &&
       target->bad_byte%2u==0u &&
       target->bad_byte<bytes &&
       target->fade_samples<target->bad_byte/2u){
      seam_sample=target->bad_byte/2u;
      /* A brief linear fade before the known bad 2048-byte boundary,
       * followed by silence for its corrupt suffix. */
      for(i=seam_sample-target->fade_samples;i<seam_sample;i++){
        int original=read_s16(a,part1_bytes,b,i);
        int remaining=(int)(seam_sample-i);
        int faded=(original*remaining)/(int)(target->fade_samples+1u);
        write_s16(a,part1_bytes,b,i,faded);
      }
      for(i=target->bad_byte;i<bytes;i++)
        write_byte(a,part1_bytes,b,i,0u);
      mutated=1;
    }
  }
  hr=IDirectSoundBuffer_Unlock(buffer,part1,part1_bytes,part2,part2_bytes);
  if(FAILED(hr)){
    if(emit)emit("AUDIO-COMBAT-TAIL: WARNING DirectSound Unlock failed");
    return;
  }
  if(mutated && emit){
    snprintf(msg,sizeof(msg),
      "AUDIO-COMBAT-TAIL: REPAIRED sample=%s slot=%d bytes=%lu hz=%lu from_pcm_byte=%lu fade_samples=%lu original_sha256=%s",
      target->name,slot,(unsigned long)bytes,(unsigned long)rate,
      (unsigned long)target->bad_byte,
      (unsigned long)target->fade_samples,target->sha256);
    emit(msg);
  }
}
