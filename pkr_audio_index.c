/*
 * Spider-Man (2001 PC) original PKR3 WAV source identification.
 * The independent, opt-in native PKR loader restores verified WAV
 * remainders BEFORE creating DirectSound buffers.
 *
 * User's game assets never leave this process: game-dir data.pkr is opened
 * rb, validated and decoded in memory using vendored miniz tinfl.
 * We NEVER feed the game's possibly corrupted PCM into another player.
 *
 * PKR3 metadata layout from krystalgamer/spidey-decomp pkr.h / pkr.cpp.
 * Supported compressed flags: -2 (raw), +2 (zlib), with original CRC32.
 * All archive offsets/sizes/counts are validated BEFORE reading payloads.
 *
 * The PKR and original WAV cache are never modified. The separate
 * Heli1-specific opt-in declick is the only DirectSound sample edit.
 * No late DirectSound tail restoration; no external subprocesses.
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
  uint8_t *pcm; /* aliases bytes in raw; NEVER free separately */
  uint8_t *raw; /* owned CRC-verified complete original WAV/RIFF bytes */
  uint32_t raw_bytes,pcm_bytes,rate,header_bytes;
  unsigned reports;
  unsigned full_checks;
} WAVSource;

static WAVSource *sources=NULL;
static unsigned source_count=0,index_ready=0,reports=0,full_reports=0;
static int heli1_declick_enabled=0;
static void (*diag)(const char *)=NULL;
static unsigned loaded_bytes=0,retained_bytes=0;

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
/* Read-only forensic check for the 2.520-second Heli1.wav loop.
 * The user's 48-kHz recording contains recurring impulses with exactly
 * this period, including pairs about 820 ms apart. Do NOT touch PCM:
 * print the strongest isolated source-sample jumps and the loop seam
 * to distinguish existing original content from playback artefacts.
 * No DirectSound, file writes, or game-state changes. */
static void log_heli1_source_discontinuities(void){
  unsigned i,rank;
  if(!diag)return;
  for(i=0;i<source_count;i++){
    const WAVSource *w=&sources[i];
    uint32_t sample_count,first,last,seam_delta,chosen[8]={0};
    unsigned used=0;
    char line[260];
    if(strcmp(w->name,"Heli1.wav")!=0 || w->rate!=12000u ||
       w->pcm_bytes!=60480u || w->header_bytes!=44u || !w->pcm)
      continue;
    sample_count=w->pcm_bytes/2u;
    first=(uint16_t)le16(w->pcm);
    last=(uint16_t)le16(w->pcm+w->pcm_bytes-2u);
    seam_delta=(uint32_t)abs((int)(int16_t)first-(int)(int16_t)last);
    snprintf(line,sizeof(line),
      "PKR-AUDIO: HELI1-SOURCE original=%s/%s samples=%lu hz=%lu duration_ms=2520 loop_seam_delta=%lu last=%d first=%d (READ-ONLY)",
      w->dir,w->name,(unsigned long)sample_count,
      (unsigned long)w->rate,(unsigned long)seam_delta,
      (int)(int16_t)last,(int)(int16_t)first);
    diag(line);
    /* Pick up to 8 largest jumps, at least 120ms apart, without
     * assuming that every transient is unwanted audio. */
    for(rank=0;rank<8u;rank++){
      uint32_t s,best_sample=0,best_delta=0;
      int best_prev=0,best_next=0;
      for(s=1;s<sample_count;s++){
        unsigned p;
        int prev,next;
        uint32_t delta;
        for(p=0;p<used;p++)
          if(s+1440u>chosen[p] && s<chosen[p]+1440u)break;
        if(p<used)continue;
        prev=(int)(int16_t)le16(w->pcm+(s-1u)*2u);
        next=(int)(int16_t)le16(w->pcm+s*2u);
        delta=(uint32_t)abs(next-prev);
        if(delta<=best_delta)continue;
        best_delta=delta;best_sample=s;best_prev=prev;best_next=next;
      }
      if(!best_delta)break;
      chosen[used++]=best_sample;
      snprintf(line,sizeof(line),
        "PKR-AUDIO: HELI1-SOURCE-JUMP rank=%u sample=%lu ms=%.3f delta=%lu prev=%d next=%d (READ-ONLY; no audio modification)",
        rank+1u,(unsigned long)best_sample,
        (double)best_sample*1000.0/(double)w->rate,
        (unsigned long)best_delta,best_prev,best_next);
      diag(line);
    }
    break;
  }
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
            if(e->length<=MAX_PCM_CACHE-retained_bytes){
              WAVSource *dst=&sources[source_count];
              uint32_t pcm_offset=(uint32_t)(pcm-raw);
              /* Preserve the SAME independently CRC32-checked bytes used
               * to populate PCM. One ownership/allocation, not two copies.
               * Retaining the full file also covers optional RIFF chunks
               * AFTER the data chunk (observed 44/190 extra bytes). */
              dst->raw=raw;
              dst->raw_bytes=e->length;
              dst->pcm=raw+pcm_offset;
              raw=NULL; /* ownership transferred to WAVSource */
              strcpy(dst->dir,e->dir);
              strcpy(dst->name,e->name);
              dst->pcm_bytes=pcm_bytes;dst->rate=hz;
              dst->header_bytes=pcm_offset;
              retained_bytes+=e->length;
              loaded_bytes+=pcm_bytes;
              source_count++;decoded++;
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
      "PKR-AUDIO: CACHE WAV PCM16 mono=%u bytes=%u crc_errors=%u (READ-ONLY; native playback unchanged)",
      source_count,loaded_bytes,crc_fail);
    diag(line);
  }
  if(decoded>0 && crc_fail==0)log_heli1_source_discontinuities();
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
    diag("PKR-AUDIO: PROBE ARMED - passive format+length+PCM prefix matching on native game Play; NATIVE PLAY UNCHANGED");
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


/* Helper shared by read-only diagnostics and the Heli1-specific declick.
 * This only addresses already locked DirectSound buffers. */
static uint8_t ds_locked_byte(const uint8_t *a,const uint8_t *b,
                              DWORD a_len,DWORD index){
  return index<a_len?a[index]:b[index-a_len];
}
/* Match *all the native game's already-read full resource sectors*
 * byte-for-byte to the CRC32-verified, independently decoded WAV file.
 * This includes the WAV container, optional chunks after data, and the
 * complete PCM prefix. No guesswork about fixed 44-byte WAV file length.
 *
 * The native game always allocates total_bytes and reads floor(total/2048)
 * blocks; we only return the original file's remainder if a CRC-verified
 * source matches the entire prefix and every matching source's remaining
 * bytes agree (aliases permitted), with the sector overlapping actual PCM,
 * not just metadata.
 * Caller verifies one native scratch-sector read against this source before
 * ANY write. CRC is validated during PKR index construction. */
const unsigned char *SpiderPKRAudioIndex_ExpectedLoaderWavTail(
    const unsigned char *wav,unsigned complete_bytes,unsigned total_bytes,
    const char **source_name){
  unsigned i,matches=0,remainder;
  const WAVSource *match=NULL;
  if(source_name)*source_name=NULL;
  if(!index_ready||!wav||complete_bytes<2048u||
     total_bytes<=complete_bytes||total_bytes>MAX_PKR_ITEM||
     complete_bytes!=(total_bytes&~2047u)||
     total_bytes-complete_bytes>=2048u)return NULL;
  if(memcmp(wav,"RIFF",4u)!=0||memcmp(wav+8u,"WAVE",4u)!=0||
     le32(wav+4u)!=total_bytes-8u)return NULL;
  remainder=total_bytes-complete_bytes;
  for(i=0;i<source_count;i++){
    const WAVSource *w=&sources[i];
    uint32_t pcm_end;
    if(!w->raw||w->raw_bytes!=total_bytes||
       w->header_bytes<44u||w->header_bytes>complete_bytes||
       w->pcm_bytes>total_bytes-w->header_bytes||
       w->rate<8000u||w->rate>96000u)continue;
    pcm_end=w->header_bytes+w->pcm_bytes;
    /* No need to patch a sector containing only non-audio metadata. */
    if(pcm_end<=complete_bytes)continue;
    if(memcmp(wav,w->raw,complete_bytes)!=0)continue;
    /* Several names may refer to exactly the same original WAV bytes.
     * Multiple prefix matches are safe ONLY when all independently
     * CRC32-verified candidates also have IDENTICAL remaining bytes.
     * Otherwise their resource identity is ambiguous: fail closed.
     * The loader still reads a fresh native sector and compares it
     * against this expected remainder before any game-memory write. */
    if(match && memcmp(match->raw+complete_bytes,
                       w->raw+complete_bytes,remainder)!=0)return NULL;
    if(!match)match=w;
    matches++;
  }
  if(!matches||!match)return NULL;
  if(source_name)*source_name=matches==1u?match->name:
    "CRC-identical WAV aliases";
  return match->raw+complete_bytes; /* full exact remainder, incl. metadata */
}


/* Strictly forensic: explain why a native WAV read cannot be matched to
 * the independently CRC-verified source index. Read at most the already
 * loaded prefix in the game's allocated WAV staging buffer; DO NOT read
 * past complete_bytes, access the game's resource handle, or change the
 * original source-matching gates. A first-sector hint is NOT proof of
 * identity and must never authorize a correction. */
void SpiderPKRAudioIndex_DescribeLoaderSkip(
    const unsigned char *wav,unsigned complete_bytes,unsigned total_bytes,
    char *out,unsigned out_capacity){
  unsigned i,same_size=0,head2048=0,full_prefix=0;
  const WAVSource *head_hint=NULL,*full_hint=NULL;
  if(!out||!out_capacity)return;
  out[0]=0;
  if(!index_ready||!wav||complete_bytes<2048u||
     complete_bytes>total_bytes){
    snprintf(out,out_capacity,"index_unavailable=1");
    return;
  }
  for(i=0;i<source_count;i++){
    const WAVSource *w=&sources[i];
    if(!w->raw)continue;
    if(w->raw_bytes==total_bytes)same_size++;
    if(w->raw_bytes>=2048u &&
       memcmp(w->raw,wav,2048u)==0){
      if(++head2048==1u)head_hint=w;
    }
    if(w->raw_bytes==total_bytes &&
       w->raw_bytes>=complete_bytes &&
       memcmp(w->raw,wav,complete_bytes)==0){
      if(++full_prefix==1u)full_hint=w;
    }
  }
  snprintf(out,out_capacity,
    "indexed=%u same_size=%u head2048=%u full_prefix=%u head_hint=%s full_hint=%s",
    source_count,same_size,head2048,full_prefix,
    head2048==1u && head_hint?head_hint->name:"-",
    full_prefix==1u && full_hint?full_hint->name:"-");
}

/* Experimental / opt-in: eliminate two objectively isolated one-sample
 * outliers in the original CRC-verified Heli1.wav (12kHz mono PCM16).
 * This does NOT alter data.pkr or the cached original. The captured
 * game soundtrack contains TWO phase-shifted concurrent copies of
 * Heli1, explaining two click events 820 ms apart each 2520 ms cycle.
 * Only these exact known signed PCM samples change:
 *   sample 12095: -9    -> 29634  (between 30604 and 28663)
 *   sample 12109: 32765 -> -30655 (between -30587 and -30723)
 * Complete native buffer must exactly match the verified original
 * EXCEPT that these two samples may already be patched.
 * This declick is independent from the original native loader tail repair.
 */
void SpiderPKRAudioIndex_EnableHeli1Declick(int enabled){
  heli1_declick_enabled=(enabled==1 && index_ready);
  if(heli1_declick_enabled && diag)
    diag("PKR-AUDIO: HELI1-DECLICK ARMED (two original PCM samples only; stopped native buffer; EXPERIMENTAL)");
  else if(enabled==1 && diag)
    diag("PKR-AUDIO: HELI1-DECLICK REFUSED (verified original source index unavailable)");
}
static uint16_t ds_read_u16(const uint8_t *a,const uint8_t *b,DWORD alen,DWORD sample){
  DWORD i=sample*2u;
  return (uint16_t)(ds_locked_byte(a,b,alen,i) |
                    ((uint16_t)ds_locked_byte(a,b,alen,i+1u)<<8));
}
static void ds_write_u16(uint8_t *a,uint8_t *b,DWORD alen,DWORD sample,uint16_t v){
  DWORD i=sample*2u;
  if(i<alen)a[i]=(uint8_t)v;else b[i-alen]=(uint8_t)v;
  i++;
  if(i<alen)a[i]=(uint8_t)(v>>8);else b[i-alen]=(uint8_t)(v>>8);
}
int SpiderPKRAudioIndex_TryHeli1Declick(int slot,IDirectSoundBuffer *buf,int looping){
  static const DWORD positions[2]={12095u,12109u};
  static const int16_t original_samples[2]={-9,32765};
  static const int16_t replacements[2]={29634,-30655};
  WAVSource *w=NULL;
  DSBCAPS caps;
  WAVEFORMATEX fmt;
  DWORD fmt_got=0,status=0,alen=0,blen=0,i,j;
  BYTE *a=NULL,*b=NULL;
  HRESULT hr;
  int valid=1,changed=0;
  unsigned s;
  char line[200];
  if(!heli1_declick_enabled||!buf||slot<0||slot>=32||!looping)return 0;
  for(s=0;s<source_count;s++){
    WAVSource *q=&sources[s];
    if(strcmp(q->name,"Heli1.wav")==0 && q->pcm_bytes==60480u &&
       q->rate==12000u && q->header_bytes==44u && q->pcm){
      if(w)return 0; /* refuse duplicate named/format originals */
      w=q;
    }
  }
  if(!w)return 0;
  for(i=0;i<2u;i++){
    DWORD k=positions[i];
    /* Refuse other editions with different source bytes or neighbors. */
    if((int16_t)le16(w->pcm+2u*k)!=original_samples[i] ||
       (i==0 && ((int16_t)le16(w->pcm+2u*(k-1u))!=30604 ||
                 (int16_t)le16(w->pcm+2u*(k+1u))!=28663)) ||
       (i==1 && ((int16_t)le16(w->pcm+2u*(k-1u))!=-30587 ||
                 (int16_t)le16(w->pcm+2u*(k+1u))!=-30723)))
      return 0;
  }
  memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);
  memset(&fmt,0,sizeof(fmt));
  if(FAILED(IDirectSoundBuffer_GetCaps(buf,&caps)) ||
     FAILED(IDirectSoundBuffer_GetFormat(buf,&fmt,sizeof(fmt),&fmt_got)) ||
     fmt_got<16u || caps.dwBufferBytes!=w->pcm_bytes ||
     fmt.wFormatTag!=WAVE_FORMAT_PCM || fmt.nChannels!=1 ||
     fmt.wBitsPerSample!=16 || fmt.nBlockAlign!=2 ||
     fmt.nSamplesPerSec!=w->rate ||
     FAILED(IDirectSoundBuffer_GetStatus(buf,&status)) ||
     (status&DSBSTATUS_PLAYING))return 0;
  hr=IDirectSoundBuffer_Lock(buf,0,caps.dwBufferBytes,
                            (void**)&a,&alen,(void**)&b,&blen,0);
  if(FAILED(hr))return 0;
  if(!a||alen+blen!=caps.dwBufferBytes||(blen&&!b)){valid=0;goto unlock_heli;}
  /* Whole-buffer byte identity, not only a WAV prefix: no other sounds,
   * arbitrarily damaged buffers, or previously modified PCM qualify. */
  for(i=0;i<w->pcm_bytes/2u;i++){
    uint16_t got=ds_read_u16(a,b,alen,i);
    uint16_t expected=le16(w->pcm+2u*i);
    if(i==positions[0]||i==positions[1]){
      unsigned k=(i==positions[0]?0u:1u);
      if(got!=expected && got!=(uint16_t)replacements[k]){valid=0;break;}
    }else if(got!=expected){valid=0;break;}
  }
  if(!valid)goto unlock_heli;
  for(j=0;j<2u;j++){
    if(ds_read_u16(a,b,alen,positions[j])!=(uint16_t)replacements[j]){
      ds_write_u16(a,b,alen,positions[j],(uint16_t)replacements[j]);
      changed=1;
    }
  }
unlock_heli:
  hr=IDirectSoundBuffer_Unlock(buf,a,alen,b,blen);
  if(FAILED(hr)){
    if(diag)diag("PKR-AUDIO: HELI1-DECLICK WARNING DirectSound Unlock failed; native Play retained");
    return 0;
  }
  if(changed && diag){
    snprintf(line,sizeof(line),
      "PKR-AUDIO: HELI1-DECLICK slot=%d loop=%d source=%s/%s samples=12095,12109 replacement=29634,-30655 (native DirectSound preserved)",
      slot,!!looping,w->dir,w->name);
    diag(line);
  }
  return changed;
}
