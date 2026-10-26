/*
 * Experimental exact PCM fingerprint of a suspected Spider-Man 2001 WAV.
 * No buffer modifications, volume/rate changes or Stop calls. Lock/Unlock
 * still touches the audio synchronization path; use only in short tests.
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
#include "audio_swing5_fingerprint.h"

#define SUSPECT_BYTES 32632u
#define SUSPECT_HZ 16000u
#define MAX_PROBES 12u
/* SHA-256 of the decoded 16-bit mono PCM bytes, not the RIFF header. */
static const char expected_pcm_sha256[] =
 "abb6d1b0863ea9c44fc0b718115f7e0821383d90a58f7cd22d9cf6e447fb8639";
static void (*probe_log)(const char *)=NULL;
static int probe_enabled=0;
static unsigned probe_count=0;
/* Exactly one copied PCM snapshot. Worker writes after DirectSound Unlock.
 * This is a diagnostic side effect, explicitly gated by the user.
 * No game buffer or game resource is modified. */
static int capture_enabled=0;
static volatile LONG capture_state=0; /* 0=none, 1=copy/write, 2=done */
static BYTE captured_pcm[SUSPECT_BYTES];

static DWORD WINAPI write_captured_wav(LPVOID unused){
  char path[MAX_PATH],message[240],*slash;
  DWORD n,got=0;
  HANDLE file;
  BOOL success;
  DWORD riff_size=36u+SUSPECT_BYTES;
  DWORD sample_rate=SUSPECT_HZ,byte_rate=2u*SUSPECT_HZ,data_size=SUSPECT_BYTES;
  BYTE header[44]={
    'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',
    16,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,2,0,16,0,
    'd','a','t','a',0,0,0,0
  };
  (void)unused;
  memcpy(header+4,&riff_size,4u);
  memcpy(header+24,&sample_rate,4u);
  memcpy(header+28,&byte_rate,4u);
  memcpy(header+40,&data_size,4u);
  n=GetModuleFileNameA(NULL,path,MAX_PATH);
  slash=n>0 && n<MAX_PATH?strrchr(path,'\\'):NULL;
  if(!slash || (size_t)(slash-path)+strlen("\\SpiderMan2k1Fix-AudioCandidate.wav")>=MAX_PATH){
    if(probe_log)probe_log("AUDIO-SWING5-CAPTURE: REFUSED game executable path too long");
    InterlockedExchange(&capture_state,2);
    return 0;
  }
  strcpy(slash+1,"SpiderMan2k1Fix-AudioCandidate.wav");
  /* CREATE_NEW protects previous captures and unrelated game data. */
  file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE){
    DWORD err=GetLastError();
    snprintf(message,sizeof(message),
      "AUDIO-SWING5-CAPTURE: CREATE-FAILED win32=%lu (file exists? delete or rename old capture first)",
      (unsigned long)err);
    if(probe_log)probe_log(message);
    InterlockedExchange(&capture_state,2);
    return 0;
  }
  success=WriteFile(file,header,sizeof(header),&got,NULL) && got==sizeof(header);
  if(success)success=WriteFile(file,captured_pcm,SUSPECT_BYTES,&got,NULL) && got==SUSPECT_BYTES;
  if(!CloseHandle(file))success=FALSE;
  if(!success){
    DeleteFileA(path);
    if(probe_log)probe_log("AUDIO-SWING5-CAPTURE: WRITE-FAILED; incomplete output removed");
  }else{
    if(probe_log)probe_log("AUDIO-SWING5-CAPTURE: WRITTEN SpiderMan2k1Fix-AudioCandidate.wav (44-byte WAV header + 32632 PCM bytes), no game PCM modified");
  }
  InterlockedExchange(&capture_state,2);
  return 0;
}


static unsigned char byte_at(const unsigned char *a,DWORD na,
                             const unsigned char *b,DWORD offset){
  return offset<na?a[offset]:b[offset-na];
}
static int sample_at(const unsigned char *a,DWORD na,
                     const unsigned char *b,DWORD index){
  unsigned lo=byte_at(a,na,b,index*2u);
  unsigned hi=byte_at(a,na,b,index*2u+1u);
  return (int)(int16_t)(uint16_t)(lo|(hi<<8));
}
static void log_message(const char *message){
  if(probe_log)probe_log(message);
}
void SpiderSwing5Fingerprint_Init(void (*log_fn)(const char *),int enabled,
                                   int capture_wav){
  probe_log=log_fn;
  probe_enabled=enabled==1;
  capture_enabled=probe_enabled && capture_wav==1;
  InterlockedExchange(&capture_state,0);
  probe_count=0;
  if(capture_wav && !probe_enabled)
    log_message("AUDIO-SWING5-CAPTURE: REFUSED; enable ExperimentalAudioSwing5Fingerprint=1 too");
  if(capture_enabled)
    log_message("AUDIO-SWING5-CAPTURE: ARMED, one WAV snapshot, original buffer unchanged; local file I/O performed on worker thread");
  if(probe_enabled)
    log_message("AUDIO-SWING5-FP: ARMED optional PCM SHA-256; 32632 bytes/16000 Hz mono16 only; max 12 probes; Lock/Unlock may affect timing");
}
void SpiderSwing5Fingerprint_Probe(int slot,IDirectSoundBuffer *buffer,
                                    DWORD bytes,DWORD frequency){
  WAVEFORMATEX fmt;
  DWORD got=0,part1_len=0,part2_len=0,digest_len=32;
  void *part1=NULL,*part2=NULL;
  HCRYPTPROV provider=0;
  HCRYPTHASH hash=0;
  BYTE digest[32];
  char actual[65],message[320];
  unsigned i,trailing_zeros=0,tail_peak=0;
  DWORD total_samples=SUSPECT_BYTES/2u;
  HRESULT hr;
  int ok=1,unlock_ok,capture_ready=0;

  if(!probe_enabled || !buffer || slot<0 || slot>=32 ||
     bytes!=SUSPECT_BYTES || frequency!=SUSPECT_HZ)return;
  if(probe_count>=MAX_PROBES)return;
  probe_count++;

  memset(&fmt,0,sizeof(fmt));
  hr=IDirectSoundBuffer_GetFormat(buffer,&fmt,sizeof(fmt),&got);
  if(FAILED(hr) || got<16u || fmt.wFormatTag!=WAVE_FORMAT_PCM ||
     fmt.nChannels!=1u || fmt.wBitsPerSample!=16u ||
     fmt.nSamplesPerSec!=SUSPECT_HZ){
    snprintf(message,sizeof(message),
      "AUDIO-SWING5-FP: FORMAT-REFUSED slot=%d hr=0x%08lX returned=%lu tag=%u channels=%u bits=%u source_hz=%lu",
      slot,(unsigned long)hr,(unsigned long)got,(unsigned)fmt.wFormatTag,
      (unsigned)fmt.nChannels,(unsigned)fmt.wBitsPerSample,
      (unsigned long)fmt.nSamplesPerSec);
    log_message(message);
    return;
  }
  /* An entire-buffer Lock may return two fragments. Do not Restore()
   * after an error or attempt to write to either returned address. */
  hr=IDirectSoundBuffer_Lock(buffer,0u,SUSPECT_BYTES,
           &part1,&part1_len,&part2,&part2_len,0u);
  if(FAILED(hr)){
    snprintf(message,sizeof(message),
      "AUDIO-SWING5-FP: LOCK-FAILED slot=%d hr=0x%08lX",
      slot,(unsigned long)hr);
    log_message(message);
    return;
  }
  if(part1==NULL || part1_len+part2_len!=SUSPECT_BYTES ||
     (part2_len>0u && part2==NULL))ok=0;

  if(ok){
    ok=CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0;
    if(ok)ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
    if(ok)ok=CryptHashData(hash,(const BYTE*)part1,part1_len,0)!=0;
    if(ok && part2_len)
      ok=CryptHashData(hash,(const BYTE*)part2,part2_len,0)!=0;
    if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&digest_len,0)!=0 && digest_len==32u;
    if(ok){
      const unsigned char *p1=(const unsigned char *)part1;
      const unsigned char *p2=(const unsigned char *)part2;
      for(i=0;i<32u;i++)sprintf(actual+2u*i,"%02x",(unsigned)digest[i]);
      actual[64]=0;
      for(i=total_samples;i>0u;i--){
        if(sample_at(p1,part1_len,p2,i-1u)!=0)break;
        trailing_zeros++;
      }
      for(i=total_samples-160u;i<total_samples;i++){
        int v=sample_at(p1,part1_len,p2,i);
        unsigned absolute=(unsigned)(v<0?-v:v);
        if(absolute>tail_peak)tail_peak=absolute;
      }
      if(capture_enabled && InterlockedCompareExchange(&capture_state,1,0)==0){
        memcpy(captured_pcm,p1,part1_len);
        if(part2_len)memcpy(captured_pcm+part1_len,p2,part2_len);
        capture_ready=1;
      }
      snprintf(message,sizeof(message),
        "AUDIO-SWING5-FP: %s slot=%d bytes=%lu hz=%lu sha256=%s tail_zero_samples=%u tail_peak_10ms=%u/32768",
        strcmp(actual,expected_pcm_sha256)==0?"MATCH":"DIFFERENT",
        slot,(unsigned long)bytes,(unsigned long)frequency,actual,
        trailing_zeros,tail_peak);
    }
  }
  if(hash)CryptDestroyHash(hash);
  if(provider)CryptReleaseContext(provider,0);
  unlock_ok=SUCCEEDED(IDirectSoundBuffer_Unlock(buffer,part1,part1_len,part2,part2_len));
  if(!ok){
    snprintf(message,sizeof(message),
      "AUDIO-SWING5-FP: HASH-FAILED slot=%d bytes1=%lu bytes2=%lu",
      slot,(unsigned long)part1_len,(unsigned long)part2_len);
  }
  if(!unlock_ok){
    char warning[120];
    snprintf(warning,sizeof(warning),"AUDIO-SWING5-FP: UNLOCK-FAILED slot=%d",slot);
    log_message(warning);
  }
  log_message(message);
  if(capture_ready){
    HANDLE thread=CreateThread(NULL,0,write_captured_wav,NULL,0,NULL);
    if(thread){
      CloseHandle(thread);
      log_message("AUDIO-SWING5-CAPTURE: SCHEDULED one PCM WAV snapshot after buffer Unlock");
    }else{
      log_message("AUDIO-SWING5-CAPTURE: THREAD-FAILED; no file written");
      InterlockedExchange(&capture_state,2);
    }
  }
}
