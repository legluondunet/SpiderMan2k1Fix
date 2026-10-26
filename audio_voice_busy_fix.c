/* Optional single-byte fix for the original game's broken audio-voice
 * occupancy test (Spider-Man Settings by r57zone inspired this experiment).
 *
 * The original SpideyPC.exe calls IDirectSoundBuffer::GetStatus for a
 * game-owned voice, then checks (status & 4), i.e. LOOPING, rather than
 * (status & 1), i.e. PLAYING. That wrongly marks a currently playing
 * one-shot as available for recycling.
 *
 * Source reference: https://github.com/r57zone/Spider-Man-Settings
 * Source/Unit1.pas: VoiceFixFileOffset = $10448F, $04 -> $01.
 *
 * This is a guarded experiment, OFF by default. Patches only mapped
 * in-process executable code. Never changes SpideyPC.exe on disk.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "audio_voice_busy_fix.h"

#define VOICE_TEST_VA ((BYTE *)(uintptr_t)0x00504488u)
#define VOICE_FLAG_VA ((BYTE *)(uintptr_t)0x0050448Fu)
static const BYTE original_status_tail[]={
  0x8B,0x44,0x24,0x04,0x5E,0x83,0xE0,0x04,0x59,0xC3
};
static void diag(void (*logger)(const char *),const char *message){
  if(logger)logger(message);
}
void SpiderVoiceBusyFix_Init(void (*logger)(const char *)){
  char path[MAX_PATH],*slash;
  DWORD n,oldprotect=0,restored=0;
  BOOL flushed,protected_back;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(!n || n>=sizeof(path))return;
  slash=strrchr(path,'\\');
  if(!slash || (size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1>=sizeof(path))return;
  strcpy(slash+1,"SpiderMan2k1Fix.ini");
  if(GetPrivateProfileIntA("Debug","ExperimentalAudioVoiceBusyFix",0,path)!=1)return;
  diag(logger,"AUDIO-VOICE-BUSY: ARMED (correct native DirectSound status test LOOPING(4) -> PLAYING(1); memory only)");
  /* The caller already verified the supported executable's full SHA-256.
   * Refuse relocated images, unexpected code or already-patched bytes. */
  if((uintptr_t)GetModuleHandleA(NULL)!=(uintptr_t)0x00400000u){
    diag(logger,"AUDIO-VOICE-BUSY: REFUSED (unsupported image base)");
    return;
  }
  if(memcmp(VOICE_TEST_VA,original_status_tail,sizeof(original_status_tail))!=0){
    diag(logger,"AUDIO-VOICE-BUSY: REFUSED (native code does not match original signature)");
    return;
  }
  if(!VirtualProtect(VOICE_TEST_VA,sizeof(original_status_tail),
                     PAGE_EXECUTE_READWRITE,&oldprotect)){
    diag(logger,"AUDIO-VOICE-BUSY: REFUSED (VirtualProtect failed)");
    return;
  }
  *VOICE_FLAG_VA=0x01u;
  flushed=FlushInstructionCache(GetCurrentProcess(),VOICE_TEST_VA,
                                 sizeof(original_status_tail));
  if(!flushed){
    *VOICE_FLAG_VA=0x04u; /* Fail closed: restore original executable code. */
    FlushInstructionCache(GetCurrentProcess(),VOICE_TEST_VA,
                          sizeof(original_status_tail));
  }
  protected_back=VirtualProtect(VOICE_TEST_VA,sizeof(original_status_tail),
                                oldprotect,&restored);
  if(flushed && *VOICE_FLAG_VA==0x01u){
    if(protected_back)
      diag(logger,"AUDIO-VOICE-BUSY: APPLIED VA=0x0050448F 04->01 (GetStatus now checks PLAYING, original game file untouched)");
    else
      diag(logger,"AUDIO-VOICE-BUSY: WARNING applied but code-page protection restore failed");
  }else{
    diag(logger,"AUDIO-VOICE-BUSY: FAILED (cache flush or patch verification failed; attempted rollback)");
  }
}
