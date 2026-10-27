/*
 * Spider-Man (2001 PC) original PKR3 WAV identification, READ-ONLY
 * by default. A separate OFF-by-default option may restore verified
 * corrupt partial WAV sectors in native secondary DirectSound buffers.
 *
 * User's game assets never leave this process: game-dir data.pkr is opened
 * rb, validated and decoded in memory using vendored miniz tinfl.
 * We NEVER feed the game's possibly corrupted PCM into SDL3 from here.
 * This probe is the prerequisite to an original-source SDL3 backend.
 *
 * PKR3 metadata layout from krystalgamer/spidey-decomp pkr.h / pkr.cpp.
 * Supported compressed flags: -2 (raw), +2 (zlib), with original CRC32.
 * All archive offsets/sizes/counts are validated BEFORE reading payloads.
 *
 * The PKR and original WAV cache are never modified. Only the optional
 * strict tail-restore path may write to a stopped native DS buffer.
 * No external subprocesses.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_STDIO
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "third_party/miniz/miniz_tinfl.h"
#include "pkr_audio_index.h"

#define PKR_HEADER_BYTES 8u
#define PKR_FOOTER_BYTES 12u
#define PKR_DIR_BYTES 40u
#define PKR_FILE_BYTES 52u
#define MAX_PKR_DIRS 4096u
#define MAX_PKR_FILES 16000u
#define MAX_PKR_ITEM (16u*1024u*1024u)
#define MAX_WAV_COUNT 2048u
#define MAX_PCM_CACHE (128u*1024u*1024u)
#define MIN_MATCH_BYTES 1024u
#define MAX_LOG_MATCHES 192u
#define MAX_FULL_COMPARE_BYTES (1024u*1024u)
#define MAX_FULL_COMPARE_LOGS 256u

typedef struct {
  char dir[33];
  char name[33];
  uint32_t crc;
  int32_t method;
  uint32_t offset,length,stored;
} PKRRecord;
typedef struct {
  char dir[33],name[33];
  uint8_t *pcm;
  uint32_t pcm_bytes,rate,header_bytes;
  unsigned reports;
  unsigned tail_restore_count;
  unsigned full_checks;
} WAVSource;

static WAVSource *sources=NULL;
static unsigned source_count=0,index_ready=0,reports=0,full_reports=0;
static int tail_restore_enabled=0;
static void (*diag)(const char *)=NULL;
static unsigned loaded_bytes=0;

static uint32_t le32(const uint8_t *p){
  return (uint32_t)p[0] | (uint32_t)p[1]<<8 |
         (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint16_t le16(const uint8_t *p){
  return (uint16_t)(p[0] | (uint16_t)p[1]<<8);
}
static uint32_t crc32_pkraw(const uint8_t *p,size_t n){
  uint32_t c=0xFFFFFFFFu;
  size_t i;
  for(i=0;i<n;i++){
    unsigned j;c^=p[i];
    for(j=0;j<8u;j++)
      c=(c>>1) ^ ((c&1u)?0xEDB88320u:0u);
  }
  return ~c;
}
static void bounded_name(char out[33],const uint8_t *p){
  memcpy(out,p,32);
  out[32]=0;
}
static int is_wav(const char name[33]){
  size_t n=strlen(name);
  return n>4 && name[n-4]=='.' &&
    (name[n-3]=='w'||name[n-3]=='W') &&
    (name[n-2]=='a'||name[n-2]=='A') &&
    (name[n-1]=='v'||name[n-1]=='V');
}
static int read_at(FILE *f,uint32_t off,void *buf,uint32_t n){
  if(off>0x7FFFFFFFu||fseek(f,(long)off,SEEK_SET))return 0;
  return fread(buf,1,n,f)==n;
}
/* Parse fmt/data RIFF chunks instead of assuming 44-byte WAV headers. */
static int parse_wav(const uint8_t *wav,uint32_t size,
                     const uint8_t **pcm,uint32_t *bytes,uint32_t *rate){
  uint32_t off=12u,lim,fmt_rate=0,data_size=0;
  const uint8_t *audio=NULL;
  int format_ok=0;
  if(size<36u||memcmp(wav,"RIFF",4)||memcmp(wav+8,"WAVE",4))return 0;
  lim=le32(wav+4);
  if(lim>size-8u || lim<28u)return 0;
  lim+=8u;
  while(off+8u<=lim){
    uint32_t n=le32(wav+off+4u),body=off+8u;
    if(n>lim-body)return 0;
    if(memcmp(wav+off,"fmt ",4)==0 && n>=16u){
      const uint8_t *f=wav+body;
      if(le16(f)==1 && le16(f+2)==1 &&
         le16(f+12)==2 && le16(f+14)==16 &&
         le32(f+4)>=8000u && le32(f+4)<=96000u){
        fmt_rate=le32(f+4);format_ok=1;
      }
    }else if(memcmp(wav+off,"data",4)==0 && !audio){
      audio=wav+body;data_size=n;
    }
    if(n&1u && n==lim-body)return 0;
    off=body+n+(n&1u);
  }
  if(!format_ok || !audio || data_size<MIN_MATCH_BYTES*2u ||
     (data_size&1u))return 0;
  *pcm=audio;*bytes=data_size;*rate=fmt_rate;
  return 1;
}
static int record_is_valid(const PKRRecord *e,uint32_t footer){
  if(e->length<36u || e->length>MAX_PKR_ITEM ||
     e->stored==0u || e->stored>MAX_PKR_ITEM ||
     e->offset<PKR_HEADER_BYTES || e->offset>=footer ||
     e->stored>footer-e->offset)return 0;
  if(e->method==-2 && e->length!=e->stored)return 0;
  return e->method==-2 || e->method==2;
}
static void log_index(const char *description,unsigned a,unsigned b,unsigned c){
  char m[260];
  if(!diag)return;
  snprintf(m,sizeof(m),"PKR-AUDIO: %s total=%u valid=%u other=%u",
           description,a,b,c);
  diag(m);
}
static int load_audio_index(const char *path){
  FILE *f=NULL;
  long total_size;
  uint8_t head[8],foot[12],directory[40],record[52];
  uint32_t footer,numdirs,numfiles,d,file_cursor=0,declared_files=0;
  PKRRecord *entries=NULL;
  unsigned available=0,invalid=0,nonpcm=0,decoded=0,crc_fail=0;
  int success=0;
  f=fopen(path,"rb");
  if(!f){if(diag)diag("PKR-AUDIO: REFUSED cannot read game-dir data.pkr (source archive not found)");return 0;}
  if(fseek(f,0,SEEK_END))goto finished;
  total_size=ftell(f);
  if(total_size<20 || (unsigned long)total_size>0x7FFFFFFFul ||
     !read_at(f,0,head,8u) || memcmp(head,"PKR3",4))goto finished;
  footer=le32(head+4);
  if(footer<8u || footer>(uint32_t)total_size-12u ||
     !read_at(f,footer,foot,12u))goto finished;
  numdirs=le32(foot+4);numfiles=le32(foot+8);
  if(!numdirs || numdirs>MAX_PKR_DIRS ||
     !numfiles || numfiles>MAX_PKR_FILES ||
     numdirs>(UINT32_MAX-numfiles*PKR_FILE_BYTES-PKR_FOOTER_BYTES)/PKR_DIR_BYTES ||
     footer+12u+numdirs*40u+numfiles*52u>(uint32_t)total_size)goto finished;
  entries=(PKRRecord*)calloc(numfiles,sizeof(PKRRecord));
  sources=(WAVSource*)calloc(MAX_WAV_COUNT,sizeof(WAVSource));
  if(!entries||!sources)goto finished;
  for(d=0;d<numdirs;d++){
    uint32_t count;
    if(!read_at(f,footer+12u+d*40u,directory,40u))goto finished;
    count=le32(directory+36);
    if(count>numfiles-declared_files)goto finished;
    for(uint32_t j=0;j<count;j++){
      PKRRecord *e=&entries[file_cursor++];
      bounded_name(e->dir,directory);
    }
    declared_files+=count;
  }
  if(declared_files!=numfiles)goto finished;
  for(d=0;d<numfiles;d++){
    PKRRecord *e=&entries[d];
    if(!read_at(f,footer+12u+numdirs*40u+d*52u,record,52u))goto finished;
    bounded_name(e->name,record);
    e->crc=le32(record+32);e->method=(int32_t)le32(record+36);
    e->offset=le32(record+40);e->length=le32(record+44);
    e->stored=le32(record+48);
    if(is_wav(e->name)){
      available++;
      if(!record_is_valid(e,footer))invalid++;
      else{
        uint8_t *packed=NULL,*raw=NULL;
        const uint8_t *pcm=NULL;
        uint32_t pcm_bytes=0,hz=0;
        int unzipped=0;
        if(source_count>=MAX_WAV_COUNT || loaded_bytes>=MAX_PCM_CACHE){
          invalid++;continue;
        }
        packed=(uint8_t*)malloc(e->stored);
        raw=(uint8_t*)malloc(e->length);
        if(packed && raw && read_at(f,e->offset,packed,e->stored)){
          if(e->method==-2){
            memcpy(raw,packed,e->length);unzipped=1;
          }else{
            size_t n=tinfl_decompress_mem_to_mem(raw,e->length,
              packed,e->stored,TINFL_FLAG_PARSE_ZLIB_HEADER);
            unzipped=n==e->length;
          }
          if(unzipped && crc32_pkraw(raw,e->length)==e->crc &&
             parse_wav(raw,e->length,&pcm,&pcm_bytes,&hz)){
            if(pcm_bytes<=MAX_PCM_CACHE-loaded_bytes){
              WAVSource *dst=&sources[source_count];
              dst->pcm=(uint8_t*)malloc(pcm_bytes);
              if(dst->pcm){
                memcpy(dst->pcm,pcm,pcm_bytes);
                strcpy(dst->dir,e->dir);
                strcpy(dst->name,e->name);
                dst->pcm_bytes=pcm_bytes;dst->rate=hz;
                dst->header_bytes=(uint32_t)(pcm-raw);
                loaded_bytes+=pcm_bytes;
                source_count++;decoded++;
              }else invalid++;
            }else invalid++;
          }else{
            if(unzipped && crc32_pkraw(raw,e->length)!=e->crc)crc_fail++;
            else nonpcm++;
          }
        }else invalid++;
        free(packed);free(raw);
      }
    }
  }
  success=1;
finished:
  if(f)fclose(f);
  free(entries);
  if(!success){
    if(diag)diag("PKR-AUDIO: REFUSED invalid/truncated PKR3 index; original gameplay unaffected");
    return 0;
  }
  log_index("SOURCE-INDEX",available,decoded,invalid+nonpcm+crc_fail);
  if(diag){
    char line[180];
    snprintf(line,sizeof(line),
      "PKR-AUDIO: CACHE WAV PCM16 mono=%u bytes=%u crc_errors=%u (READ-ONLY; no SDL3 override)",
      source_count,loaded_bytes,crc_fail);
    diag(line);
  }
  return decoded>0 && crc_fail==0;
}
void SpiderPKRAudioIndex_Init(void (*logger)(const char*),int enabled){
  char path[MAX_PATH],*slash;
  DWORD n;
  diag=logger;
  if(enabled!=1)return;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(!n||n>=sizeof(path)||(slash=strrchr(path,'\\'))==NULL)return;
  strcpy(slash+1,"data.pkr");
  index_ready=load_audio_index(path);
  if(index_ready && diag)
    diag("PKR-AUDIO: PROBE ARMED - passive format+length+PCM prefix matching on native game Play; NO REPLACEMENT");
}
void SpiderPKRAudioIndex_Probe(int slot,IDirectSoundBuffer *buf,int looping){
  DSBCAPS caps;
  WAVEFORMATEX fmt;
  DWORD size1=0,size2=0,got=0,need;
  uint8_t *first=NULL,*second=NULL;
  WAVSource *winner=NULL;
  unsigned i,matches=0;
  char line[255];
  HRESULT hr;
  if(!index_ready||!buf||slot<0||slot>=32)return;
  memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
  memset(&fmt,0,sizeof(fmt));
  if(FAILED(IDirectSoundBuffer_GetCaps(buf,&caps)) ||
     FAILED(IDirectSoundBuffer_GetFormat(buf,&fmt,sizeof(fmt),&got)) ||
     got<16u || fmt.wFormatTag!=WAVE_FORMAT_PCM ||
     fmt.nChannels!=1u || fmt.wBitsPerSample!=16u ||
     fmt.nBlockAlign!=2u || caps.dwBufferBytes<MIN_MATCH_BYTES*2u)return;
  /* Avoid DS Lock altogether unless at least one archive item has
   * the exact runtime PCM length and original sample rate. */
  for(i=0;i<source_count;i++)
    if(sources[i].pcm_bytes==caps.dwBufferBytes &&
       sources[i].rate==fmt.nSamplesPerSec){matches++;break;}
  if(!matches)return;
  need=MIN_MATCH_BYTES;
  hr=IDirectSoundBuffer_Lock(buf,0,need,
      (void**)&first,&size1,(void**)&second,&size2,0);
  if(FAILED(hr))return;
  if(first && size1+size2==need){
    for(i=0;i<source_count;i++){
      WAVSource *w=&sources[i];
      if(w->pcm_bytes!=caps.dwBufferBytes ||
         w->rate!=fmt.nSamplesPerSec)continue;
      if(size1 && memcmp(w->pcm,first,size1))continue;
      if(size2 && (!second || memcmp(w->pcm+size1,second,size2)))continue;
      matches++;
      if(!winner)winner=w;
    }
  }
  hr=IDirectSoundBuffer_Unlock(buf,first,size1,second,size2);
  if(FAILED(hr) || !winner)return;
  /* Preflight matches==1 means some length+rate candidate exists.
   * Exactly one prefix match raises matches to 2; higher is ambiguous. */
  if(winner->reports++==0 && reports<MAX_LOG_MATCHES){
    reports++;
    snprintf(line,sizeof(line),
      "PKR-AUDIO: %s slot=%d loop=%d original=%s/%s bytes=%lu hz=%lu matching_prefix=1024 candidates=%u (READ-ONLY)",
      (matches==2)?"MATCH":"AMBIGUOUS",
      slot,!!looping,winner->dir,winner->name,
      (unsigned long)winner->pcm_bytes,(unsigned long)winner->rate,
      (matches>0)?matches-1u:0u);
    if(diag)diag(line);
  }
  /* Read-only, source-exact FULL PCM verification, never trust a prefix
   * for a replacement decision. Bound both bytes/time and reporting.
   * Sample at most twice per distinct source to detect variable tails. */
  if(matches==2 && winner->full_checks<2u &&
     full_reports<MAX_FULL_COMPARE_LOGS &&
     winner->pcm_bytes<=MAX_FULL_COMPARE_BYTES){
    DWORD a=0,b=0;
    BYTE *p=NULL,*q=NULL;
    uint32_t pos,first_bad=UINT32_MAX,last_bad=0,bad_count=0;
    HRESULT unlocked;
    hr=IDirectSoundBuffer_Lock(buf,0,winner->pcm_bytes,
                               (void**)&p,&a,(void**)&q,&b,0);
    if(FAILED(hr))return;
    if(p && a+b==winner->pcm_bytes && (!b || q)){
      for(pos=0;pos<winner->pcm_bytes;pos++){
        uint8_t live=(pos<a)?p[pos]:q[pos-a];
        if(live!=winner->pcm[pos]){
          if(first_bad==UINT32_MAX)first_bad=pos;
          last_bad=pos;
          bad_count++;
        }
      }
    }else first_bad=UINT32_MAX-1u;
    unlocked=IDirectSoundBuffer_Unlock(buf,p,a,q,b);
    if(FAILED(unlocked) || first_bad==UINT32_MAX-1u)return;
    winner->full_checks++;
    full_reports++;
    if(first_bad==UINT32_MAX){
      snprintf(line,sizeof(line),
        "PKR-AUDIO: FULL-PCM IDENTICAL source=%s/%s slot=%d loop=%d bytes=%lu compare=%u/2 (READ-ONLY)",
        winner->dir,winner->name,slot,!!looping,
        (unsigned long)winner->pcm_bytes,winner->full_checks);
    }else{
      snprintf(line,sizeof(line),
        "PKR-AUDIO: FULL-PCM DIFF source=%s/%s slot=%d loop=%d bytes=%lu first_diff=%lu last_diff=%lu mismatched_bytes=%lu first_mod_2048=%lu compare=%u/2 (READ-ONLY)",
        winner->dir,winner->name,slot,!!looping,
        (unsigned long)winner->pcm_bytes,
        (unsigned long)first_bad,(unsigned long)last_bad,
        (unsigned long)bad_count,(unsigned long)(first_bad%2048u),
        winner->full_checks);
    }
    if(diag)diag(line);
  }
}


/* Experimental, fail-open generalized workaround for the exact recurring
 * game-owned PCM tail fault shown in the user's 53/53 comparisons:
 *   PCM first mismatch + RIFF data offset (44) = N * 2048.
 *
 * We do NOT trust a prefix alone and do NOT fill unknown buffers.
 * Only a single CRC-verified PKR source that matches format/size and
 * every byte before the expected 2-KiB sector boundary is eligible.
 * The actual first mismatching byte MUST be that boundary, not just
 * somewhere in the suffix. This is source-backed and identity guarded,
 * but is NOT a fix for the as-yet unknown native loader instruction.
 *
 * Native DirectSound handles Play, loops, IsPlaying, Stop, pan, volume,
 * pitch, game scene/focus, and all game voice allocation as before.
 * We only repair the proven damaged PCM suffix under the same DS lock
 * and only when the buffer is not currently playing. */
static uint8_t ds_locked_byte(const uint8_t *a,const uint8_t *b,
                              DWORD a_len,DWORD index){
  return index<a_len?a[index]:b[index-a_len];
}
void SpiderPKRAudioIndex_EnableTailRestore(int enabled){
  tail_restore_enabled=(enabled==1 && index_ready);
  if(tail_restore_enabled && diag)
    diag("PKR-AUDIO: TAIL-RESTORE ARMED (original CRC-verified WAV; strict 2048-boundary; stopped native DS buffer ONLY; no SDL3 reroute)");
  else if(enabled==1 && diag)
    diag("PKR-AUDIO: TAIL-RESTORE REFUSED; original data.pkr index unavailable");
}
int SpiderPKRAudioIndex_TryTailRestore(int slot,IDirectSoundBuffer *buf,int looping){
  DSBCAPS caps;
  WAVEFORMATEX fmt;
  DWORD got=0,status=0,a_len=0,b_len=0;
  uint8_t *a=NULL,*b=NULL;
  WAVSource *source=NULL;
  unsigned candidates=0,i;
  uint32_t cut,j;
  int changed=0;
  HRESULT result;
  char line[256];
  if(!tail_restore_enabled||!buf||slot<0||slot>=32)return 0;
  memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
  memset(&fmt,0,sizeof(fmt));
  if(FAILED(IDirectSoundBuffer_GetCaps(buf,&caps)) ||
     FAILED(IDirectSoundBuffer_GetFormat(buf,&fmt,sizeof(fmt),&got)) ||
     FAILED(IDirectSoundBuffer_GetStatus(buf,&status)) ||
     (status&DSBSTATUS_PLAYING) ||
     got<16u || fmt.wFormatTag!=WAVE_FORMAT_PCM ||
     fmt.nChannels!=1u || fmt.wBitsPerSample!=16u ||
     fmt.nBlockAlign!=2u || caps.dwBufferBytes<2048u ||
     caps.dwBufferBytes>MAX_FULL_COMPARE_BYTES)return 0;
  /* A valid source can only be from this exact CRC-checked PKR index.
   * A standard WAV data offset is currently required for the proven
   * 44-byte sector alignment; other RIFF layouts remain unmodified. */
  for(i=0;i<source_count;i++)
    if(sources[i].header_bytes==44u &&
       sources[i].pcm_bytes==caps.dwBufferBytes &&
       sources[i].rate==fmt.nSamplesPerSec){candidates++;}
  if(!candidates)return 0;

  result=IDirectSoundBuffer_Lock(buf,0,caps.dwBufferBytes,
                                 (void**)&a,&a_len,(void**)&b,&b_len,0);
  if(FAILED(result))return 0;
  if(!a || (a_len+b_len)!=caps.dwBufferBytes || (b_len && !b))
    goto unlock;
  /* Resolve uniqueness by exact size, rate and first 1024 PCM bytes.
   * Never pick arbitrarily among two source files. */
  for(i=0;i<source_count;i++){
    WAVSource *w=&sources[i];
    if(w->header_bytes!=44u ||
       w->pcm_bytes!=caps.dwBufferBytes ||
       w->rate!=fmt.nSamplesPerSec)continue;
    for(j=0;j<MIN_MATCH_BYTES;j++)
      if(ds_locked_byte(a,b,a_len,j)!=w->pcm[j])break;
    if(j==MIN_MATCH_BYTES){
      if(source){source=NULL;goto unlock;} /* ambiguous */
      source=w;
    }
  }
  if(!source)goto unlock;
  /* Last fully read sector of the original WAV, expressed in PCM bytes.
   * The supplied 44-byte header is the reason every first_diff%2048
   * equals 2004 in the observed corruption traces. */
  cut=((caps.dwBufferBytes+source->header_bytes)/2048u)*2048u-
      source->header_bytes;
  if(cut<MIN_MATCH_BYTES||cut>=caps.dwBufferBytes)goto unlock;
  /* Source PCM must be BYTE-IDENTICAL through the last complete sector. */
  for(j=0;j<cut;j++)
    if(ds_locked_byte(a,b,a_len,j)!=source->pcm[j])goto unlock;
  /* The FIRST mismatching byte must be precisely the next sector.
   * More general changes, legitimate fades/processing and non-sector
   * corruption are deliberately left unmodified. */
  if(ds_locked_byte(a,b,a_len,cut)==source->pcm[cut])goto unlock;
  /* The DirectSound Lock covers the full buffer: write only the tail,
   * without introducing an unlock/relock race against native playback. */
  if(cut<a_len){
    memcpy(a+cut,source->pcm+cut,a_len-cut);
    if(b_len)memcpy(b,source->pcm+a_len,b_len);
  }else if(b_len){
    memcpy(b+(cut-a_len),source->pcm+cut,caps.dwBufferBytes-cut);
  }
  changed=1;
unlock:
  result=IDirectSoundBuffer_Unlock(buf,a,a_len,b,b_len);
  if(FAILED(result)){
    if(diag)diag("PKR-AUDIO: TAIL-RESTORE WARNING DirectSound Unlock failed; native Play retained");
    return 0;
  }
  if(changed && source){
    source->tail_restore_count++;
    if(source->tail_restore_count<=2u && diag){
      snprintf(line,sizeof(line),
        "PKR-AUDIO: TAIL-RESTORED slot=%d loop=%d source=%s/%s pcm_bytes=%lu sector_tail_offset=%lu restored_bytes=%lu occurrence=%u (native DirectSound preserved)",
        slot,!!looping,source->dir,source->name,
        (unsigned long)source->pcm_bytes,(unsigned long)cut,
        (unsigned long)(source->pcm_bytes-cut),source->tail_restore_count);
      diag(line);
    }
  }
  return changed;
}
