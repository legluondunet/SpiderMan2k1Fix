/*
 * Read-only experimental probe for brief foreign sound fragments
 * at the END of Spider-Man 2001 punches.
 * Captures up to 24 distinct PCM16 mono one-shots for user-side listening,
 * without changing DirectSound playback or sample bytes.
 * Lock/Unlock itself may influence timing: disabled by default.
 * Capture begins after a configurable native 3D delay or, if selected,
 * after an explicit F8 trigger plus that delay. No fighting F8 required.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "audio_combat_pcm.h"

#define LIMIT 24u
#define MANUAL_SESSIONS 2u
#define MANUAL_SESSION_LIMIT 12u
#define F8_DEBOUNCE_MS 1200u
#define PCM_MIN 4096u
#define PCM_MAX 32768u
#define SPACING_MS 250u
typedef struct {
  BYTE data[PCM_MAX];
  DWORD size,rate,at_ms;
  unsigned slot,session,ordinal;
  uint64_t hash;
} Capture;
static Capture samples[LIMIT];
static CRITICAL_SECTION cs;
static int on=0;
static DWORD start_delay_ms=22000u;
static int manual_mode=0;
static volatile LONG f8_triggered=0;
static volatile LONG f8_uses=0,prev_f8_time=-1;
static volatile LONG ready_logged=0;
static volatile LONG capture_from_ms=22000;
static unsigned count=0;
static unsigned session_counts[MANUAL_SESSIONS]={0u,0u};
static volatile LONG prev_ms=0;
static void (*emit)(const char *)=NULL;

static uint64_t fingerprint(const BYTE *p,DWORD n){
  uint64_t h=UINT64_C(14695981039346656037);
  DWORD i;
  for(i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}
  return h;
}
static int pcm16(const BYTE *p,unsigned index){
  unsigned i=index*2u;
  return (int)(int16_t)(uint16_t)((unsigned)p[i]|((unsigned)p[i+1u]<<8));
}
static unsigned delta(int a,int b){
  int d=a-b;return (unsigned)(d<0?-d:d);
}
/* At conventional WAV file byte 2048*N, PCM byte 2048*N-44 begins.
 * Output is only a signal-discontinuity heuristic; compressed/stretched
 * samples, natural transients, and non-44-byte source headers are possible. */
static void inspect_seam(const Capture *c,unsigned *offset,
                         unsigned *before,unsigned *after){
  unsigned boundary=0,k,n=(unsigned)(c->size/2u);
  uint64_t left=0,right=0;
  for(k=1;2048u*k-44u<c->size;k++){
    unsigned s=(2048u*k-44u)/2u;
    if(s>128u && s+128u<n)boundary=s;
  }
  *offset=0;*before=0;*after=0;
  if(!boundary)return;
  for(k=boundary-127u;k<=boundary;k++)
    left+=delta(pcm16(c->data,k),pcm16(c->data,k-1u));
  for(k=boundary+1u;k<=boundary+128u;k++)
    right+=delta(pcm16(c->data,k),pcm16(c->data,k-1u));
  *offset=44u+2u*boundary;
  *before=(unsigned)(left/128u);
  *after=(unsigned)(right/128u);
}
static DWORD WINAPI save_wav(LPVOID argument){
  unsigned idx=(unsigned)(uintptr_t)argument,before,after,offset;
  Capture *c;
  BYTE header[44]={0};
  const DWORD fmt_size=16u;
  const WORD pcm_format=1u,ch=1u,align=2u,bits=16u;
  DWORD riff,rate,bps,sz,n,wrote=0;
  HANDLE file;
  BOOL ok;
  char dir[MAX_PATH],name[80],line[450];
  if(idx>=LIMIT)return 0;
  c=&samples[idx];
  inspect_seam(c,&offset,&before,&after);
  snprintf(line,sizeof(line),
    "AUDIO-COMBAT-PCM: CAPTURE=%u session=%u ordinal=%u t=%lums slot=%u bytes=%lu rate=%lu fnv64=%08lX%08lX last_boundary_wav_byte=%u abs_jump_before=%u abs_jump_after=%u (heuristic only)",
    idx+1u,c->session,c->ordinal,(unsigned long)c->at_ms,c->slot,(unsigned long)c->size,
    (unsigned long)c->rate,(unsigned long)(c->hash>>32),
    (unsigned long)(c->hash&0xFFFFFFFFu),offset,before,after);
  if(emit)emit(line);
  if(manual_mode)
    snprintf(name,sizeof(name),"SpiderMan2k1Fix-CombatPCM-R%u-%02u.wav",c->session,c->ordinal);
  else
    snprintf(name,sizeof(name),"SpiderMan2k1Fix-CombatPCM-%02u.wav",idx+1u);
  n=GetTempPathA(MAX_PATH,dir);
  if(!n || n>=MAX_PATH || n+strlen(name)>=MAX_PATH){
    if(emit)emit("AUDIO-COMBAT-PCM: TEMP-PATH-FAILED");
    return 0;
  }
  strcpy(dir+n,name);
  sz=c->size;riff=36u+sz;rate=c->rate;bps=rate*2u;
  memcpy(header,"RIFF",4);
  memcpy(header+4,&riff,4);
  memcpy(header+8,"WAVEfmt ",8);
  memcpy(header+16,&fmt_size,4);
  memcpy(header+20,&pcm_format,2);
  memcpy(header+22,&ch,2);
  memcpy(header+24,&rate,4);
  memcpy(header+28,&bps,4);
  memcpy(header+32,&align,2);
  memcpy(header+34,&bits,2);
  memcpy(header+36,"data",4);
  memcpy(header+40,&sz,4);
  file=CreateFileA(dir,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,
                   FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE){
    snprintf(line,sizeof(line),
      "AUDIO-COMBAT-PCM: WRITE-REFUSED capture=%u win32=%lu; old WAV never overwritten",
      idx+1u,(unsigned long)GetLastError());
    if(emit)emit(line);
    return 0;
  }
  ok=WriteFile(file,header,sizeof(header),&wrote,NULL)&&wrote==sizeof(header);
  if(ok)ok=WriteFile(file,c->data,sz,&wrote,NULL)&&wrote==sz;
  if(!CloseHandle(file))ok=FALSE;
  if(!ok){
    DeleteFileA(dir);
    snprintf(line,sizeof(line),"AUDIO-COMBAT-PCM: WRITE-FAILED capture=%u",idx+1u);
  }else{
    snprintf(line,sizeof(line),
      "AUDIO-COMBAT-PCM: WRITTEN capture=%u path=%s original_PCM_unchanged=1",
      idx+1u,dir);
  }
  if(emit)emit(line);
  return 0;
}
void SpiderCombatPCM_Init(void (*log_fn)(const char *),int enabled,
                           DWORD start_ms,int manual_f8){
  char line[260];
  emit=log_fn;
  on=enabled==1;
  manual_mode=(manual_f8==1);
  start_delay_ms=start_ms<=300000u?start_ms:22000u;
  count=0;
  InterlockedExchange(&prev_ms,0);
  InterlockedExchange(&f8_triggered,0);
  InterlockedExchange(&f8_uses,0);
  InterlockedExchange(&prev_f8_time,-1);
  memset(session_counts,0,sizeof(session_counts));
  InterlockedExchange(&ready_logged,0);
  InterlockedExchange(&capture_from_ms,(LONG)start_delay_ms);
  if(!on)return;
  InitializeCriticalSection(&cs);
  if(manual_mode)
    snprintf(line,sizeof(line),
      "AUDIO-COMBAT-PCM: ARMED MANUAL-F8; press F8 twice near fights; delay_after_f8=%lums; 12 distinct samples per press; TEMP WAV",
      (unsigned long)start_delay_ms);
  else
    snprintf(line,sizeof(line),
      "AUDIO-COMBAT-PCM: ARMED AUTO first_3d_t=%lums; 24 unique one-shots; TEMP WAV",
      (unsigned long)start_delay_ms);
  if(emit)emit(line);
}
/* A transition into 3D starts a new elapsed-time epoch. Preserve already
 * captured WAV snapshots; reset only the one-shot F8 arm state. */
void SpiderCombatPCM_On3DStart(void){
  if(!on)return;
  InterlockedExchange(&f8_triggered,0);
  InterlockedExchange(&prev_f8_time,-1);
  InterlockedExchange(&ready_logged,0);
  InterlockedExchange(&prev_ms,0);
  InterlockedExchange(&capture_from_ms,(LONG)start_delay_ms);
}
int SpiderCombatPCM_IsManualF8(void){
  return on && manual_mode;
}
/* Called only for an F8 rising edge while the game is in 3D.
 * Further presses deliberately do not restart/extend the capture window. */
void SpiderCombatPCM_OnF8(DWORD native_ms){
  char line[260];
  if(!SpiderCombatPCM_IsManualF8())return;
  if(InterlockedCompareExchange(&f8_triggered,1,0)!=0)return;
  InterlockedExchange(&capture_from_ms,(LONG)(native_ms+start_delay_ms));
  snprintf(line,sizeof(line),
    "AUDIO-COMBAT-PCM: F8-TRIGGER t=%lums capture_start_t=%lums delay=%lums (one-shot; repeated F8 ignored)",
    (unsigned long)native_ms,(unsigned long)(native_ms+start_delay_ms),
    (unsigned long)start_delay_ms);
  if(emit)emit(line);
}
void SpiderCombatPCM_OnFlip(DWORD native_ms){
  char line[220];
  DWORD first;
  if(!on || (manual_mode && InterlockedCompareExchange(&f8_triggered,0,0)==0))
    return;
  first=(DWORD)InterlockedCompareExchange(&capture_from_ms,0,0);
  if(native_ms<first)return;
  if(InterlockedCompareExchange(&ready_logged,1,0)!=0)return;
  snprintf(line,sizeof(line),
    "AUDIO-COMBAT-PCM: CAPTURE-WINDOW-OPEN t=%lums trigger=%s; listening for one-shot PCM",
    (unsigned long)native_ms,manual_mode?"F8":"automatic");
  if(emit)emit(line);
}
void SpiderCombatPCM_Probe(int slot,IDirectSoundBuffer *buf,DWORD size,
                           DWORD now,int looping){
  BYTE copy[PCM_MAX];
  WAVEFORMATEX format;
  DSBCAPS caps;
  DWORD returned=0,len1=0,len2=0;
  void *p=NULL,*q=NULL;
  HRESULT hr;
  LONG last;
  uint64_t hash;
  unsigned i,idx;
  HANDLE thread;
  char msg[140];
  if(!on || !buf || looping || slot<0 || slot>=32 ||
     size<PCM_MIN || size>PCM_MAX || (size&1u))return;
  /* Fail closed until F8 first triggers the manual capture window. */
  if(manual_mode && InterlockedCompareExchange(&f8_triggered,0,0)==0)return;
  if(now<(DWORD)InterlockedCompareExchange(&capture_from_ms,0,0))return;
  last=InterlockedCompareExchange(&prev_ms,0,0);
  if(now-(DWORD)last<SPACING_MS)return;
  if(InterlockedCompareExchange(&prev_ms,(LONG)now,last)!=last)return;
  memset(&format,0,sizeof(format));
  memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
  hr=IDirectSoundBuffer_GetFormat(buf,&format,sizeof(format),&returned);
  if(FAILED(hr)||returned<16u||format.wFormatTag!=WAVE_FORMAT_PCM||
     format.nChannels!=1u||format.wBitsPerSample!=16u||
     format.nSamplesPerSec<8000u||format.nSamplesPerSec>48000u||
     format.nBlockAlign!=2u||FAILED(IDirectSoundBuffer_GetCaps(buf,&caps))||
     caps.dwBufferBytes!=size)return;
  hr=IDirectSoundBuffer_Lock(buf,0,size,&p,&len1,&q,&len2,0);
  if(FAILED(hr))return;
  if(!p||len1+len2!=size||(len2 && !q)){
    IDirectSoundBuffer_Unlock(buf,p,len1,q,len2);
    return;
  }
  memcpy(copy,p,len1);
  if(len2)memcpy(copy+len1,q,len2);
  if(FAILED(IDirectSoundBuffer_Unlock(buf,p,len1,q,len2))){
    if(emit)emit("AUDIO-COMBAT-PCM: UNLOCK-FAILED, dropped copied sample");
    return;
  }
  hash=fingerprint(copy,size);
  EnterCriticalSection(&cs);
  for(i=0;i<count;i++)
    if(samples[i].size==size && samples[i].hash==hash &&
       memcmp(samples[i].data,copy,size)==0)break;
  if(i<count || count>=LIMIT){
    LeaveCriticalSection(&cs);
    return;
  }
  idx=count++;
  memcpy(samples[idx].data,copy,size);
  samples[idx].size=size;
  samples[idx].rate=format.nSamplesPerSec;
  samples[idx].at_ms=now;
  samples[idx].slot=(unsigned)slot;
  samples[idx].hash=hash;
  LeaveCriticalSection(&cs);
  thread=CreateThread(NULL,0,save_wav,(LPVOID)(uintptr_t)idx,0,NULL);
  if(thread)CloseHandle(thread);
  else{
    snprintf(msg,sizeof(msg),"AUDIO-COMBAT-PCM: THREAD-FAILED capture=%u",idx+1u);
    if(emit)emit(msg);
  }
  if(idx==LIMIT-1u && emit)
    emit("AUDIO-COMBAT-PCM: CAPTURE-LIMIT=24 reached; no further WAVs");
}
