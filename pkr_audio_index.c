/*
 * Spider-Man (2001 PC) original PKR3 WAV identification, READ-ONLY.
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
 * No original PKR or game buffer modification, no external subprocesses.
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
  uint32_t pcm_bytes,rate;
  unsigned reports;
} WAVSource;

static WAVSource *sources=NULL;
static unsigned source_count=0,index_ready=0,reports=0;
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
  if(FAILED(hr) || !winner || reports>=MAX_LOG_MATCHES)return;
  /* Log distinct source files rather than the same frequent footstep or
   * swing dozens of times. This keeps the later helicopter/boss effects
   * in the same playthrough observable without an unbounded logfile. */
  if(winner->reports++)return;
  reports++;
  snprintf(line,sizeof(line),
    "PKR-AUDIO: %s slot=%d loop=%d original=%s/%s bytes=%lu hz=%lu matching_prefix=1024 candidates=%u (READ-ONLY)",
    (matches<=2)?"MATCH":"AMBIGUOUS",
    slot,!!looping,winner->dir,winner->name,
    (unsigned long)winner->pcm_bytes,(unsigned long)winner->rate,
    (matches>0)?matches-1u:0u);
  if(diag)diag(line);
}
