/*
 * Experimental exact-hash workaround for one original Spider-Man PC
 * helicopter loop with a 2048-byte-boundary corrupted PCM tail.
 * Not a fix for the original loader/read-order defect.
 * OFF by default; Lock/Unlock may affect timing; no Play/Stop changes.
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
#include "audio_loop_tail_repair.h"

#define LOOP_BYTES 60480u
#define LOOP_HZ 12000u
#define BAD_OFFSET (29u*2048u-44u)
#define BAD_SAMPLE (BAD_OFFSET/2u)
#define FADE_SAMPLES 240u
#define MAX_ATTEMPTS 128
static const char bad_sha256[]="1a4fdeaf4ba4a60fb43c4bd35ebd2946561cb17c41767e72c8e7559559979cdc";
static void (*log_fn)(const char *)=NULL;
static int armed=0;
static volatile LONG attempts=0;
static BYTE getbyte(const BYTE *a,DWORD na,const BYTE *b,DWORD i){
  return i<na?a[i]:b[i-na];
}
static void putbyte(BYTE *a,DWORD na,BYTE *b,DWORD i,BYTE v){
  if(i<na)a[i]=v;else b[i-na]=v;
}
static int get16(const BYTE *a,DWORD na,const BYTE *b,DWORD index){
  unsigned lo=getbyte(a,na,b,index*2u),hi=getbyte(a,na,b,index*2u+1u);
  return (int)(int16_t)(uint16_t)(lo|(hi<<8));
}
static void put16(BYTE *a,DWORD na,BYTE *b,DWORD index,int value){
  uint16_t v=(uint16_t)(int16_t)value;
  putbyte(a,na,b,index*2u,(BYTE)(v&255u));
  putbyte(a,na,b,index*2u+1u,(BYTE)(v>>8));
}
void SpiderLoopTailRepair_Init(void (*logger)(const char *),int enabled){
  log_fn=logger;
  armed=(enabled==1);
  InterlockedExchange(&attempts,0);
  if(armed && log_fn)log_fn("AUDIO-LOOP-TAIL: ARMED exact SHA256; 240-sample fade + 566-sample silence at 2048-byte WAV boundary; OFF by default");
}
void SpiderLoopTailRepair_Probe(int slot,IDirectSoundBuffer *buf,DWORD bytes,DWORD hz,int looping){
  WAVEFORMATEX fmt;
  DSBCAPS caps;
  DWORD got=0,na=0,nb=0,digestlen=32;
  void *a=NULL,*b=NULL;
  HCRYPTPROV provider=0;
  HCRYPTHASH hash=0;
  BYTE digest[32];
  char digesthex[65],line[360];
  HRESULT hr;
  int ok=1,patched=0;
  unsigned i;
  if(!armed || !buf || !looping || slot<0 || slot>=32 ||
     bytes!=LOOP_BYTES || hz!=LOOP_HZ)return;
  if(InterlockedIncrement(&attempts)>MAX_ATTEMPTS)return;
  memset(&fmt,0,sizeof(fmt));
  memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
  hr=IDirectSoundBuffer_GetFormat(buf,&fmt,sizeof(fmt),&got);
  if(FAILED(hr)||got<16u||fmt.wFormatTag!=WAVE_FORMAT_PCM||
     fmt.nChannels!=1u||fmt.wBitsPerSample!=16u||
     fmt.nSamplesPerSec!=LOOP_HZ||fmt.nBlockAlign!=2u||
     FAILED(IDirectSoundBuffer_GetCaps(buf,&caps))||
     caps.dwBufferBytes!=LOOP_BYTES)return;
  hr=IDirectSoundBuffer_Lock(buf,0u,LOOP_BYTES,&a,&na,&b,&nb,0u);
  if(FAILED(hr))return;
  if(!a||na+nb!=LOOP_BYTES||(nb && !b))ok=0;
  if(ok)ok=CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0;
  if(ok)ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
  if(ok)ok=CryptHashData(hash,(const BYTE*)a,na,0)!=0;
  if(ok && nb)ok=CryptHashData(hash,(const BYTE*)b,nb,0)!=0;
  if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&digestlen,0)!=0&&digestlen==32u;
  if(hash)CryptDestroyHash(hash);
  if(provider)CryptReleaseContext(provider,0);
  if(ok){
    for(i=0;i<32u;i++)sprintf(digesthex+2u*i,"%02x",(unsigned)digest[i]);
    digesthex[64]=0;
    if(strcmp(digesthex,bad_sha256)==0){
      BYTE *p=(BYTE*)a,*q=(BYTE*)b;
      for(i=BAD_SAMPLE-FADE_SAMPLES;i<BAD_SAMPLE;i++){
        int v=get16(p,na,q,i);
        int gain=(int)(BAD_SAMPLE-i);
        put16(p,na,q,i,(v*gain)/(int)(FADE_SAMPLES+1u));
      }
      for(i=BAD_OFFSET;i<LOOP_BYTES;i++)putbyte(p,na,q,i,0u);
      patched=1;
    }
  }
  hr=IDirectSoundBuffer_Unlock(buf,a,na,b,nb);
  if(FAILED(hr)){
    if(log_fn)log_fn("AUDIO-LOOP-TAIL: WARNING DirectSound Unlock failed");
    return;
  }
  if(patched && log_fn){
    snprintf(line,sizeof(line),
      "AUDIO-LOOP-TAIL: REPAIRED slot=%d from_byte=%u silenced_bytes=%u fade_samples=%u original_sha256=%s (no source asset modified)",
      slot,(unsigned)BAD_OFFSET,(unsigned)(LOOP_BYTES-BAD_OFFSET),
      (unsigned)FADE_SAMPLES,bad_sha256);
    log_fn(line);
  }
}
