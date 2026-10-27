/*
 * Spider-Man (2001 PC) NATIVE WAV LOADER partial-sector correction.
 *
 * Strictly OFF by default, supported SHA256-verified SpideyPC.exe only.
 * Disassembly proved 0x503CD3 truncates WAV byte length / 2048, and
 * 0x5051D0 multiplies that count by 2048 before copying game PKR data.
 *
 * Two exact original CALLs in the WAV loader only:
 *    0x503CB4 -> native 0x505170 (query bytes in real allocation)
 *    0x503CD8 -> native 0x5051D0 (read floor(bytes / 2048) sectors).
 * The wrappers preserve both native calls and result conventions.
 *
 * With independently CRC-verified RIFF/PCM source proof:
 *   - read full blocks as before;
 *   - if the byte length has a remainder, ask the native PKR reader for
 *     exactly ONE additional 2048-byte sector into a PRIVATE 2048 buffer;
 *   - accept ONLY if the copied remainder matches the full known-original
 *     PCM from data.pkr; copy EXACTLY the remainder to the game's already
 *     allocated len-byte staging buffer. No ceil-rounding, extra allocation
 *     for game data, DirectSound intercept, or writes to data.pkr.
 *
 * No modification is performed unless the complete RIFF source matches
 * CRC32-verified originals, with any matching aliases required to agree
 * byte-for-byte on the entire unread remainder. Optional RIFF chunks after PCM
 * (observed 44 and 190 bytes) are preserved, not inferred or synthesized.
 * The obsolete Play-time DirectSound tail fallback has been retired;
 * only the independent, opt-in Heli1 two-sample declick remains.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pkr_audio_index.h"
#include "pkr_audio_loader.h"

#define GAME_BASE ((uintptr_t)0x00400000u)
#define LOAD_LENGTH_CALL ((BYTE*)(uintptr_t)0x00503CB4u)
#define LOAD_SECTOR_CALL ((BYTE*)(uintptr_t)0x00503CD8u)
#define ORIGINAL_GET_LENGTH ((int (__cdecl *)(int,uint32_t*))(uintptr_t)0x00505170u)
#define ORIGINAL_SECTOR_READ ((int (__cdecl *)(int,uint32_t,void*))(uintptr_t)0x005051D0u)
#define MAX_WAV_BYTES (16u*1024u*1024u)
#define MAX_LOADER_DIAG 96u
#define MAX_ALIAS_DIAG 24u /* separate rare-alias trace, even after first 96 restores */

static const BYTE expected_length_call[5]={0xE8,0xB7,0x14,0x00,0x00};
static const BYTE expected_sector_call[5]={0xE8,0xF3,0x14,0x00,0x00};
static DWORD tls_len=TLS_OUT_OF_INDEXES,tls_handle=TLS_OUT_OF_INDEXES;
static int installed=0;
static unsigned repaired_count=0,skipped_count=0,alias_repaired_count=0;
static void (*diag)(const char *)=NULL;

static void loader_message(const char *type,int handle,unsigned bytes,const char *name){
  char msg[270];
  if(!diag)return;
  snprintf(msg,sizeof(msg),
    "PKR-AUDIO: LOADER-%s resource=0x%08X remainder=%u source=%s (native PKR and DirectSound preserved)",
    type,(unsigned)handle,bytes,name?name:"-");
  diag(msg);
}

/* Bounded, read-only evidence for a skipped native WAV; this does NOT
 * change the original read result, file cursor, staging allocation,
 * DirectSound buffer, or any source validation / repair decision.
 * All inspected bytes lie within the original successful full-sector
 * read (complete_bytes >= 2048; dst is non-NULL). */
static uint32_t loader_le32(const unsigned char *p){
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
         ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static void loader_skip_detail(int handle,const void *dst,
                               uint32_t total,uint32_t complete){
  const unsigned char *p=(const unsigned char*)dst;
  int riff0, riff44, data0;
  uint32_t riff_size=0u,data_size=0u;
  long excess=0;
  char msg[390];
  if(!diag||!p||complete<2048u||complete>total)return;
  riff0=(memcmp(p,"RIFF",4u)==0 && memcmp(p+8u,"WAVE",4u)==0);
  riff44=(memcmp(p+44u,"RIFF",4u)==0 &&
          memcmp(p+52u,"WAVE",4u)==0);
  data0=(riff0 && memcmp(p+36u,"data",4u)==0);
  if(riff0){
    riff_size=loader_le32(p+4u);
    if(riff_size<=MAX_WAV_BYTES)
      excess=(long)total-(long)(riff_size+8u);
  }
  if(data0)data_size=loader_le32(p+40u);
  snprintf(msg,sizeof(msg),
    "PKR-AUDIO: LOADER-SKIP-DETAIL resource=0x%08X total_bytes=%lu complete_bytes=%lu remainder=%lu riff_at_0=%d riff_at_44=%d riff_declared_bytes=%lu data_chunk_bytes=%lu riff_excess_bytes=%ld (READ-ONLY)",
    (unsigned)handle,(unsigned long)total,(unsigned long)complete,
    (unsigned long)(total-complete),riff0,riff44,
    (unsigned long)(riff0 && riff_size<=MAX_WAV_BYTES?riff_size+8u:0u),
    (unsigned long)data_size,excess);
  diag(msg);
  /* Read-only identification of UNKNOWN native WAV resources. This is
   * executed ONLY after the safe full-source matcher declined to write.
   * The two 2048-byte FNV-1a fingerprints make later offline asset
   * comparisons reproducible without logging raw audio or touching
   * any live DirectSound/PKR state. */
  {
    char candidates[240],forensic[530];
    uint32_t head_hash=2166136261u,tail_hash=2166136261u;
    uint32_t pcm_end=0u,pcm_missing=0u,hz=0u;
    unsigned i,ch=0u,bits=0u,format=0u;
    int standard_fmt=riff0 &&
      memcmp(p+12u,"fmt ",4u)==0 && loader_le32(p+16u)>=16u;
    for(i=0u;i<2048u;i++){
      head_hash=(head_hash^(uint32_t)p[i])*16777619u;
      tail_hash=(tail_hash^(uint32_t)p[complete-2048u+i])*16777619u;
    }
    if(standard_fmt){
      format=(unsigned)p[20u]|((unsigned)p[21u]<<8);
      ch=(unsigned)p[22u]|((unsigned)p[23u]<<8);
      hz=loader_le32(p+24u);
      bits=(unsigned)p[34u]|((unsigned)p[35u]<<8);
    }
    if(data0 && data_size<=total-44u){
      pcm_end=44u+data_size;
      if(pcm_end>complete)pcm_missing=pcm_end-complete;
    }
    SpiderPKRAudioIndex_DescribeLoaderSkip(
      p,complete,total,candidates,(unsigned)sizeof(candidates));
    snprintf(forensic,sizeof(forensic),
      "PKR-AUDIO: LOADER-SKIP-FORENSIC pcm_tail_bytes=%lu "
      "wav_format=%u channels=%u hz=%lu bits=%u "
      "head2048_fnv1a=%08lX last_loaded2048_fnv1a=%08lX "
      "%s (READ-ONLY; no repair)",
      (unsigned long)pcm_missing,format,ch,(unsigned long)hz,bits,
      (unsigned long)head_hash,(unsigned long)tail_hash,candidates);
    diag(forensic);
  }
}

/* The loader immediately calls this original function and allocates the
 * returned WAV byte length before invoking the sector reader. Store its
 * result per-thread; NEVER infer capacity from untrusted RIFF bytes. */
static int __cdecl native_length_with_tracking(int handle,uint32_t *length){
  int result=ORIGINAL_GET_LENGTH(handle,length);
  if(installed && tls_len!=TLS_OUT_OF_INDEXES &&
     tls_handle!=TLS_OUT_OF_INDEXES){
    uint32_t n=(result==1 && length)?*length:0u;
    /* Store length first; zero it on any unsuccessful second TLS write. */
    if(n<2048u||n>MAX_WAV_BYTES||
       !TlsSetValue(tls_len,(LPVOID)(uintptr_t)n)||
       !TlsSetValue(tls_handle,(LPVOID)(uintptr_t)(uint32_t)handle))
      TlsSetValue(tls_len,NULL);
  }
  return result;
}

static int __cdecl native_sector_read_with_tail(int handle,uint32_t blocks,void *dst){
  uint32_t length=0u,staged_handle=0u,complete,remainder;
  const unsigned char *expected;
  const char *source=NULL;
  unsigned char scratch[2048];
  int result,extra;
  if(installed && tls_len!=TLS_OUT_OF_INDEXES &&
     tls_handle!=TLS_OUT_OF_INDEXES){
    length=(uint32_t)(uintptr_t)TlsGetValue(tls_len);
    staged_handle=(uint32_t)(uintptr_t)TlsGetValue(tls_handle);
    TlsSetValue(tls_len,NULL);
    TlsSetValue(tls_handle,NULL);
  }
  /* First use native behavior unconditionally. Its return is zero on
   * success; this wrapper does not interfere with the original cursor. */
  result=ORIGINAL_SECTOR_READ(handle,blocks,dst);
  if(!installed||result!=0||!dst||length<2048u||length>MAX_WAV_BYTES||
     staged_handle!=(uint32_t)handle||blocks!=(length>>11)||
     blocks>MAX_WAV_BYTES/2048u)return result;
  complete=blocks*2048u;
  if(complete>=length)return result;
  remainder=length-complete;
  if(!remainder||remainder>=2048u)return result;
  expected=SpiderPKRAudioIndex_ExpectedLoaderWavTail(
    (const unsigned char*)dst,complete,length,&source);
  if(!expected){
    if(++skipped_count<=MAX_LOADER_DIAG){
      loader_message("SKIP-NO-UNIQUE-SOURCE",handle,remainder,NULL);
      loader_skip_detail(handle,dst,length,complete);
    }
    return result;
  }
  /* The original reader clamps to the resource's actual end. A scratch
   * buffer prevents any overwrite if the native callee requests 2048 bytes.
   * Requiring exact independent CRC-backed source identity protects
   * against short reads, bad handles, and unexpected archive semantics. */
  memset(scratch,0xA5,sizeof(scratch));
  extra=ORIGINAL_SECTOR_READ(handle,1u,scratch);
  if(extra!=0||memcmp(scratch,expected,remainder)!=0){
    if(++skipped_count<=MAX_LOADER_DIAG)
      loader_message("SKIP-REMAINDER-MISMATCH",handle,remainder,source);
    return result;
  }
  memcpy((unsigned char*)dst+complete,scratch,remainder);
  ++repaired_count;
  /* Aliases are rare and may load after the standard 96-event log cap.
   * Record only independently CRC-verified, byte-identical aliases
   * AFTER successful fresh-native-sector validation and the exact
   * original-length copy. This is logging only, not a repair change. */
  if(source && strcmp(source,"CRC-identical WAV aliases")==0 &&
     ++alias_repaired_count<=MAX_ALIAS_DIAG)
    loader_message("ALIAS-TAIL-RESTORED",handle,remainder,source);
  if(repaired_count<=MAX_LOADER_DIAG)
    loader_message("TAIL-RESTORED",handle,remainder,source);
  return result;
}

void SpiderPKRAudioLoader_Init(void (*logger)(const char *),int enabled){
  BYTE *a=LOAD_LENGTH_CALL,*b=LOAD_SECTOR_CALL;
  DWORD old=0,ignored=0;
  uint32_t rel_a,rel_b;
  diag=logger;
  if(enabled!=1)return;
  if((uintptr_t)GetModuleHandleA(NULL)!=GAME_BASE||
     memcmp(a,expected_length_call,5u)!=0||
     memcmp(b,expected_sector_call,5u)!=0){
    if(diag)diag("PKR-AUDIO: LOADER-HOOK REFUSED original loader call signatures differ");
    return;
  }
  tls_len=TlsAlloc();
  if(tls_len==TLS_OUT_OF_INDEXES){
    if(diag)diag("PKR-AUDIO: LOADER-HOOK REFUSED TLS unavailable");
    return;
  }
  tls_handle=TlsAlloc();
  if(tls_handle==TLS_OUT_OF_INDEXES){
    TlsFree(tls_len);tls_len=TLS_OUT_OF_INDEXES;
    if(diag)diag("PKR-AUDIO: LOADER-HOOK REFUSED second TLS slot unavailable");
    return;
  }
  rel_a=(uint32_t)(uintptr_t)&native_length_with_tracking-
        ((uint32_t)(uintptr_t)a+5u);
  rel_b=(uint32_t)(uintptr_t)&native_sector_read_with_tail-
        ((uint32_t)(uintptr_t)b+5u);
  /* Both callsites are in the same protected code-page region. Patch as
   * one guarded unit, not two independent installed hooks. */
  if(!VirtualProtect(a,(SIZE_T)(b+5u-a),PAGE_EXECUTE_READWRITE,&old)){
    TlsFree(tls_handle);TlsFree(tls_len);
    tls_handle=tls_len=TLS_OUT_OF_INDEXES;
    if(diag)diag("PKR-AUDIO: LOADER-HOOK REFUSED VirtualProtect failed");
    return;
  }
  memcpy(a+1u,&rel_a,4u);
  memcpy(b+1u,&rel_b,4u);
  if(!FlushInstructionCache(GetCurrentProcess(),a,(SIZE_T)(b+5u-a))){
    memcpy(a,expected_length_call,5u);
    memcpy(b,expected_sector_call,5u);
    FlushInstructionCache(GetCurrentProcess(),a,(SIZE_T)(b+5u-a));
    VirtualProtect(a,(SIZE_T)(b+5u-a),old,&ignored);
    TlsFree(tls_handle);TlsFree(tls_len);
    tls_handle=tls_len=TLS_OUT_OF_INDEXES;
    if(diag)diag("PKR-AUDIO: LOADER-HOOK REFUSED FlushInstructionCache failed; restored both calls");
    return;
  }
  if(!VirtualProtect(a,(SIZE_T)(b+5u-a),old,&ignored)&&diag)
    diag("PKR-AUDIO: LOADER-HOOK WARNING cannot restore original code-page protection");
  installed=1;
  if(diag)diag("PKR-AUDIO: LOADER-HOOK ARMED experimental native WAV final-sector read; original sources CRC-verified; OFF by default");
}
