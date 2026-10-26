/*
 * Opt-in native helicopter loop investigation: capture only the first two
 * genuinely looping 60480-byte 12000-Hz PCM16 mono DirectSound buffers.
 * The program NEVER modifies game PCM or invokes Play, Stop or Set*.
 * Lock/Unlock can influence scheduling: opt-in experiments ONLY.
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
#include "audio_loop_pcm.h"

#define LOOP_BYTES 60480u
#define LOOP_HZ 12000u
#define MAX_LOOPS 2u
static void (*emit_line)(const char *)=NULL;
static int armed=0,save_wav=0;
static volatile LONG next_probe=0;
static BYTE pcm[MAX_LOOPS][LOOP_BYTES];

static void emit(const char *s){if(emit_line)emit_line(s);}
static int get_sample(const BYTE *p,unsigned index){
  return (int)(int16_t)(uint16_t)
         ((unsigned)p[index*2u]|((unsigned)p[index*2u+1u]<<8));
}
static unsigned diff(int a,int b){int d=a-b;return (unsigned)(d<0?-d:d);}
static DWORD WINAPI save_pcm(LPVOID arg){
  unsigned n=(unsigned)(uintptr_t)arg;
  char filename[MAX_PATH],base[64],message[320];
  BYTE header[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',
                   16,0,0,0,1,0,1,0,0,0,0,0,0,0,2,0,16,0,
                   'd','a','t','a',0,0,0,0};
  DWORD len=36u+LOOP_BYTES,rate=LOOP_HZ,byterate=LOOP_HZ*2u,data=LOOP_BYTES;
  DWORD plen,written=0;
  HANDLE file;
  BOOL ok;
  if(n>=MAX_LOOPS)return 0;
  snprintf(base,sizeof(base),"SpiderMan2k1Fix-Loop8-%u.wav",n+1u);
  plen=GetTempPathA(MAX_PATH,filename);
  if(!plen || plen>=MAX_PATH || plen+strlen(base)>=MAX_PATH){
    emit("AUDIO-LOOP-PCM: FAILED TEMP path");
    return 0;
  }
  strcpy(filename+plen,base);
  memcpy(header+4,&len,4u);
  memcpy(header+24,&rate,4u);
  memcpy(header+28,&byterate,4u);
  memcpy(header+40,&data,4u);
  file=CreateFileA(filename,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,
                   FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE){
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: WRITE-REFUSED capture=%u win32=%lu; existing capture NOT overwritten",
             n+1u,(unsigned long)GetLastError());
    emit(message);
    return 0;
  }
  ok=WriteFile(file,header,sizeof(header),&written,NULL) && written==sizeof(header);
  if(ok)ok=WriteFile(file,pcm[n],LOOP_BYTES,&written,NULL) && written==LOOP_BYTES;
  if(!CloseHandle(file))ok=FALSE;
  if(!ok){
    DeleteFileA(filename);
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: WRITE-FAILED capture=%u",n+1u);
  }else{
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: WRITTEN capture=%u path=%s game_PCM_unchanged=1",
             n+1u,filename);
  }
  emit(message);
  return 0;
}
void SpiderLoopPCM_Init(void (*logger)(const char *),int enabled,int capture){
  emit_line=logger;
  armed=(enabled==1);
  save_wav=armed && (capture==1);
  InterlockedExchange(&next_probe,0);
  if(capture && !armed)emit("AUDIO-LOOP-PCM: REFUSED capture requires ExperimentalAudioLoopPCMProbe=1");
  if(armed)emit("AUDIO-LOOP-PCM: ARMED 2 x 60480-byte @12000Hz looping PCM16; read-only fingerprint, seam/jump analysis; Lock/Unlock may affect timing");
}
void SpiderLoopPCM_Probe(int slot,IDirectSoundBuffer *buf,DWORD bytes,DWORD hz,int loop_flag){
  unsigned index,i,seam,maxjump=0,maxat=0,large=0;
  DWORD returned=0,a=0,b=0,hashlen=32;
  WAVEFORMATEX fmt;
  void *part1=NULL,*part2=NULL;
  HCRYPTPROV provider=0;
  HCRYPTHASH hash=0;
  BYTE digest[32];
  char hex[65],message[380];
  HRESULT hr;
  int ok,unlock_ok;
  HANDLE thread;
  if(!armed || !buf || !loop_flag || slot<0 || slot>=32 ||
     bytes!=LOOP_BYTES || hz!=LOOP_HZ)return;
  if(InterlockedCompareExchange(&next_probe,0,0)>=MAX_LOOPS)return;
  index=(unsigned)(InterlockedIncrement(&next_probe)-1);
  if(index>=MAX_LOOPS)return;
  memset(&fmt,0,sizeof(fmt));
  hr=IDirectSoundBuffer_GetFormat(buf,&fmt,sizeof(fmt),&returned);
  if(FAILED(hr)||returned<16u||fmt.wFormatTag!=WAVE_FORMAT_PCM ||
     fmt.nChannels!=1u||fmt.wBitsPerSample!=16u||
     fmt.nSamplesPerSec!=LOOP_HZ||fmt.nBlockAlign!=2u){
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: FORMAT-REFUSED capture=%u slot=%d hr=0x%08lX tag=%u channels=%u bits=%u rate=%lu",
             index+1u,slot,(unsigned long)hr,(unsigned)fmt.wFormatTag,
             (unsigned)fmt.nChannels,(unsigned)fmt.wBitsPerSample,
             (unsigned long)fmt.nSamplesPerSec);
    emit(message);
    return;
  }
  hr=IDirectSoundBuffer_Lock(buf,0,LOOP_BYTES,&part1,&a,&part2,&b,0);
  if(FAILED(hr)){
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: LOCK-FAILED capture=%u slot=%d hr=0x%08lX",
             index+1u,slot,(unsigned long)hr);
    emit(message);
    return;
  }
  ok=(part1!=NULL && a+b==LOOP_BYTES && (!b||part2!=NULL));
  if(ok){
    memcpy(pcm[index],part1,a);
    if(b)memcpy(pcm[index]+a,part2,b);
  }
  unlock_ok=SUCCEEDED(IDirectSoundBuffer_Unlock(buf,part1,a,part2,b));
  if(!ok || !unlock_ok){
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: COPY-FAILED capture=%u slot=%d complete=%d unlocked=%d",
             index+1u,slot,ok,unlock_ok);
    emit(message);
    return;
  }
  ok=CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0;
  if(ok)ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
  if(ok)ok=CryptHashData(hash,pcm[index],LOOP_BYTES,0)!=0;
  if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&hashlen,0)!=0 && hashlen==32u;
  if(hash)CryptDestroyHash(hash);
  if(provider)CryptReleaseContext(provider,0);
  if(!ok){
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: HASH-FAILED capture=%u slot=%d",index+1u,slot);
    emit(message);
    return;
  }
  for(i=0;i<32u;i++)sprintf(hex+i*2u,"%02x",(unsigned)digest[i]);
  hex[64]=0;
  seam=diff(get_sample(pcm[index],0),get_sample(pcm[index],LOOP_BYTES/2u-1u));
  for(i=1;i<LOOP_BYTES/2u;i++){
    unsigned d=diff(get_sample(pcm[index],i),get_sample(pcm[index],i-1u));
    if(d>maxjump){maxjump=d;maxat=i;}
    if(d>=12000u)large++;
  }
  snprintf(message,sizeof(message),
    "AUDIO-LOOP-PCM: CAPTURE=%u slot=%d sha256=%s first=%d last=%d seam_jump=%u max_internal_jump=%u max_sample=%u jumps_ge_12000=%u",
    index+1u,slot,hex,get_sample(pcm[index],0),
    get_sample(pcm[index],LOOP_BYTES/2u-1u),seam,maxjump,maxat,large);
  emit(message);
  if(!save_wav)return;
  /* Thread only touches immutable copy AFTER the DirectSound Unlock. */
  thread=CreateThread(NULL,0,save_pcm,(LPVOID)(uintptr_t)index,0,NULL);
  if(thread){
    CloseHandle(thread);
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: WRITING capture=%u after Unlock",index+1u);
    emit(message);
  }else{
    snprintf(message,sizeof(message),"AUDIO-LOOP-PCM: THREAD-FAILED capture=%u",index+1u);
    emit(message);
  }
}
