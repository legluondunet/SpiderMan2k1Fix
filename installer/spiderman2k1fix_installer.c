#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>
#include "../version_info.h"

/* One PE32 Windows/Wine executable containing only project-owned/redistributable
 * runtime binaries. Original retail game data is NEVER embedded. The input
 * retail ISO is not modified and is refused if its layout cannot be verified.
 * The final retail CD installation recipe is under investigation (#149).
 */
#define SPIDER_INSTALLER_MAX_FILES 4096u
#define SPIDER_INSTALLER_BLOCK 2048u
#define SPIDER_INSTALLER_BUFFER 65536u
#define SPIDER_INSTALLER_PATH 1024u
#define RES_WINMM 1101
#define RES_FIX 1102
#define RES_SDL3 1103
#define RES_SDL3_LICENSE 1104
#define RES_X11 1105
#define RES_README 1106
#define RES_CHANGELOG 1107

#define RETAIL_EXE_BYTES 1507328ULL
static const char retail_sha[] = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea";
static wchar_t launcher_dir[SPIDER_INSTALLER_PATH];
static wchar_t game_dir[SPIDER_INSTALLER_PATH];
static wchar_t log_path[SPIDER_INSTALLER_PATH];
static FILE *logfile;

typedef struct DiscEntry {
  wchar_t name[256];
  DWORD lba, bytes;
  int directory;
} DiscEntry;
typedef struct DiscImage {
  HANDLE file;
  ULONGLONG length;
  ULONGLONG volume_length;
  DWORD sector;
  DWORD count;
  int joliet;
  int error;
  DiscEntry *entries;
  wchar_t volume[33];
  DWORD visited[SPIDER_INSTALLER_MAX_FILES];
  DWORD visited_count;
} DiscImage;

static void message(const wchar_t *format, ...) {
  va_list args, copy;
  va_start(args,format);
  va_copy(copy,args);
  vwprintf(format,copy);
  wprintf(L"\n");
  if(logfile) {vfwprintf(logfile,format,args);fputwc(L'\n',logfile);fflush(logfile);}
  va_end(copy);
  va_end(args);
}
static int combine(wchar_t *out,size_t cap,const wchar_t *dir,const wchar_t *part) {
  int n;
  if(!out||!cap||!dir||!part||!part[0])return 0;
  n=_snwprintf(out,cap,L"%ls\\%ls",dir,part);
  if(n<0||(size_t)n>=cap)return 0;
  out[n]=0;return 1;
}
static int regular_file(const wchar_t *path) {
  DWORD attr=GetFileAttributesW(path);
  return attr!=INVALID_FILE_ATTRIBUTES && !(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
static int safe_directory(const wchar_t *path,int make) {
  DWORD attr=GetFileAttributesW(path);
  if(attr==INVALID_FILE_ATTRIBUTES && make){
    if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    attr=GetFileAttributesW(path);
  }
  return attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_DIRECTORY) &&
         !(attr&FILE_ATTRIBUTE_REPARSE_POINT);
}
static int extension_is(const wchar_t *s,const wchar_t *ext) {
  const wchar_t *dot=wcsrchr(s,L'.');return dot&&!_wcsicmp(dot,ext);
}
static int sha256_file(const wchar_t *path,char result[65]) {
  HANDLE f=INVALID_HANDLE_VALUE;HCRYPTPROV provider=0;HCRYPTHASH hash=0;
  BYTE buffer[SPIDER_INSTALLER_BUFFER], digest[32];DWORD got,sz=sizeof(digest),i;
  int ok=0;
  f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(f==INVALID_HANDLE_VALUE)return 0;
  if(!CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))goto done;
  if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash))goto done;
  do {
    if(!ReadFile(f,buffer,sizeof(buffer),&got,NULL))goto done;
    if(got&&!CryptHashData(hash,buffer,got,0))goto done;
  }while(got);
  if(!CryptGetHashParam(hash,HP_HASHVAL,digest,&sz,0)||sz!=32)goto done;
  for(i=0;i<32;i++)sprintf(result+i*2,"%02x",digest[i]);
  result[64]=0;ok=1;
done:
  if(hash)CryptDestroyHash(hash);
  if(provider)CryptReleaseContext(provider,0);
  if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
  return ok;
}
static int verified_retail(const wchar_t *path) {
  WIN32_FILE_ATTRIBUTE_DATA attr;ULARGE_INTEGER size;char hash[65];
  if(!regular_file(path)||!GetFileAttributesExW(path,GetFileExInfoStandard,&attr))return 0;
  size.LowPart=attr.nFileSizeLow;size.HighPart=attr.nFileSizeHigh;
  if(size.QuadPart!=RETAIL_EXE_BYTES){
    message(L"VERIFY: unsupported SpideyPC.exe size=%llu (expected %llu)",
      (unsigned long long)size.QuadPart,(unsigned long long)RETAIL_EXE_BYTES);
    return 0;
  }
  if(!sha256_file(path,hash)){
    message(L"VERIFY: cannot SHA-256 the original game executable; Win32=%lu",GetLastError());return 0;
  }
  message(L"VERIFY: SpideyPC.exe SHA-256=%hs",hash);
  if(strcmp(hash,retail_sha)){
    message(L"VERIFY: unsupported original game hash; refusing to install or launch");return 0;
  }
  return 1;
}
static int read_iso(DiscImage *iso,ULONGLONG offset,void *buf,DWORD bytes) {
  LARGE_INTEGER pos;DWORD received=0;
  if(offset>iso->length||bytes>iso->length-offset)return 0;
  pos.QuadPart=(LONGLONG)offset;
  return SetFilePointerEx(iso->file,pos,NULL,FILE_BEGIN) &&
         ReadFile(iso->file,buf,bytes,&received,NULL) && received==bytes;
}
static uint32_t iso_le32(const BYTE *p){
  return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int iso_extent(DiscImage *iso,DWORD lba,DWORD bytes) {
  ULONGLONG start=(ULONGLONG)lba*SPIDER_INSTALLER_BLOCK;
  return start<=iso->volume_length && (ULONGLONG)bytes<=iso->volume_length-start &&
         start<=iso->length && (ULONGLONG)bytes<=iso->length-start;
}
static int valid_iso_name(const wchar_t *s) {
  size_t i,n=wcslen(s);
  if(!n||n>120||!wcscmp(s,L".")||!wcscmp(s,L".."))return 0;
  for(i=0;i<n;i++){
    if(s[i]<32||wcschr(L"\\/:*?\"<>|",s[i]))return 0;
  }
  return s[n-1]!=L' ' && s[n-1]!=L'.';
}
static int iso_decode_name(const BYTE *data,unsigned len,int joliet,wchar_t *out,size_t cap) {
  unsigned i,n=0;
  if(!len || (joliet && (len&1u)))return 0;
  if(joliet){
    for(i=0;i<len;i+=2){
      wchar_t c=(wchar_t)(((unsigned)data[i]<<8)|data[i+1]);
      if(!c||n+1>=cap)return 0;
      out[n++]=c;
    }
  }else{
    for(i=0;i<len;i++){
      if(!data[i]||data[i]>=128||n+1>=cap)return 0;
      out[n++]=(wchar_t)data[i];
    }
  }
  out[n]=0;
  /* ISO 9660 version suffix ;1 and optional terminal full stop. */
  {wchar_t *semi=wcschr(out,L';');if(semi)*semi=0;}
  n=(unsigned)wcslen(out);
  if(n && out[n-1]==L'.')out[n-1]=0;
  return valid_iso_name(out);
}
static int iso_walk(DiscImage *iso,DWORD extent,DWORD bytes,
                    const wchar_t *prefix,unsigned depth) {
  BYTE sector[SPIDER_INSTALLER_BLOCK];
  ULONGLONG pos=0;
  unsigned visited;
  if(depth>16u||!iso_extent(iso,extent,bytes)||bytes>64u*1024u*1024u)return 0;
  for(visited=0;visited<iso->visited_count;visited++)
    if(iso->visited[visited]==extent)return 0; /* cyclic/duplicate directory */
  if(iso->visited_count>=SPIDER_INSTALLER_MAX_FILES)return 0;
  iso->visited[iso->visited_count++]=extent;
  while(pos<bytes){
    unsigned offset=(unsigned)(pos%SPIDER_INSTALLER_BLOCK), record;
    BYTE *p;
    if(!read_iso(iso,(ULONGLONG)extent*SPIDER_INSTALLER_BLOCK+
                 (pos/SPIDER_INSTALLER_BLOCK)*SPIDER_INSTALLER_BLOCK,
                 sector,SPIDER_INSTALLER_BLOCK))return 0;
    record=sector[offset];
    if(!record){pos+=SPIDER_INSTALLER_BLOCK-offset;continue;}
    if(offset+record>SPIDER_INSTALLER_BLOCK||record<34 ||
       sector[offset+32]>record-33)return 0;
    p=sector+offset;
    if(!(p[32]==1&&(p[33]==0||p[33]==1))){
      wchar_t shortname[256],rel[256];
      DWORD child_extent=iso_le32(p+2),child_size=iso_le32(p+10);
      unsigned dir=(p[25]&2)!=0;
      if(!iso_decode_name(p+33,p[32],iso->joliet,shortname,256))return 0;
      if(prefix && *prefix){
        if(_snwprintf(rel,256,L"%ls/%ls",prefix,shortname)<0||
           wcslen(prefix)+1+wcslen(shortname)>=256)return 0;
      }else wcscpy(rel,shortname);
      if(!iso_extent(iso,child_extent,child_size)||iso->count>=SPIDER_INSTALLER_MAX_FILES)return 0;
      wcscpy(iso->entries[iso->count].name,rel);
      iso->entries[iso->count].lba=child_extent;
      iso->entries[iso->count].bytes=child_size;
      iso->entries[iso->count].directory=dir;
      iso->count++;
      if(dir && !iso_walk(iso,child_extent,child_size,rel,depth+1u))return 0;
    }
    pos+=record;
  }
  return 1;
}
static void iso_close(DiscImage *iso){
  if(iso->file!=INVALID_HANDLE_VALUE)CloseHandle(iso->file);
  free(iso->entries);memset(iso,0,sizeof(*iso));iso->file=INVALID_HANDLE_VALUE;
}
static int iso_open(DiscImage *iso,const wchar_t *path) {
  BYTE sector[SPIDER_INSTALLER_BLOCK],root[34];
  LARGE_INTEGER size;DWORD i;int saw_primary=0,selected=0;
  memset(iso,0,sizeof(*iso));iso->file=INVALID_HANDLE_VALUE;
  iso->sector=SPIDER_INSTALLER_BLOCK;
  iso->file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(iso->file==INVALID_HANDLE_VALUE){message(L"ISO: cannot open %ls",path);return 0;}
  if(!GetFileSizeEx(iso->file,&size)||size.QuadPart<18LL*SPIDER_INSTALLER_BLOCK){
    message(L"ISO: file too small or size unavailable");return 0;
  }
  iso->length=(ULONGLONG)size.QuadPart;
  /* First supported mode is sector-aligned ISO9660 (2048-byte logical
   * sectors). Raw BIN/CUE and installer CAB reconstruction are separate
   * requirements and must fail explicitly, never be guessed. */
  for(i=16;i<80;i++){
    if(!read_iso(iso,(ULONGLONG)i*SPIDER_INSTALLER_BLOCK,sector,sizeof(sector)))return 0;
    if(memcmp(sector+1,"CD001",5)||sector[6]!=1){message(L"ISO: invalid volume descriptor at %lu",i);return 0;}
    if(sector[0]==255)break;
    if(sector[0]==1 && !saw_primary){
      DWORD blocks=iso_le32(sector+80);
      if(!blocks|| (ULONGLONG)blocks*SPIDER_INSTALLER_BLOCK>iso->length)return 0;
      iso->volume_length=(ULONGLONG)blocks*SPIDER_INSTALLER_BLOCK;
      memcpy(root,sector+156,34);saw_primary=1;
      {unsigned k;for(k=0;k<32;k++)iso->volume[k]=(wchar_t)sector[40+k];iso->volume[32]=0;}
    }
    if(sector[0]==2&&saw_primary&&sector[88]==0x25&&sector[89]==0x2F&&
       (sector[90]==0x40||sector[90]==0x43||sector[90]==0x45)){
      memcpy(root,sector+156,34);iso->joliet=1;selected=1;
    }
  }
  if(!saw_primary){message(L"ISO: ISO9660 primary descriptor absent");return 0;}
  if(!selected)iso->joliet=0;
  if(root[0]<34 || !(root[25]&2) || !iso_extent(iso,iso_le32(root+2),iso_le32(root+10)))return 0;
  iso->entries=(DiscEntry*)calloc(SPIDER_INSTALLER_MAX_FILES,sizeof(DiscEntry));
  if(!iso->entries)return 0;
  if(!iso_walk(iso,iso_le32(root+2),iso_le32(root+10),L"",0)){
    message(L"ISO: unsafe or unsupported directory records; refusing extraction");return 0;
  }
  message(L"ISO: volume='%ls' format=%ls files=%lu size=%llu",
    iso->volume,iso->joliet?L"ISO9660+Joliet":L"ISO9660",iso->count,
    (unsigned long long)iso->length);
  return 1;
}
static DiscEntry* find_disc_file(DiscImage *iso,const wchar_t *name){
  DWORD i;for(i=0;i<iso->count;i++)
    if(!iso->entries[i].directory&&!_wcsicmp(iso->entries[i].name,name))
      return &iso->entries[i];
  return NULL;
}
static int iso_copy(DiscImage *iso,const DiscEntry *item,const wchar_t *dst) {
  BYTE buffer[SPIDER_INSTALLER_BUFFER];
  HANDLE out=INVALID_HANDLE_VALUE;ULONGLONG remain=item->bytes;
  ULONGLONG offset=(ULONGLONG)item->lba*SPIDER_INSTALLER_BLOCK;
  DWORD chunk,wrote;int ok=0;
  if(!iso_extent(iso,item->lba,item->bytes))return 0;
  out=CreateFileW(dst,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(out==INVALID_HANDLE_VALUE)return 0;
  while(remain){
    chunk=(DWORD)(remain>sizeof(buffer)?sizeof(buffer):remain);
    if(!read_iso(iso,offset,buffer,chunk)||
       !WriteFile(out,buffer,chunk,&wrote,NULL)||wrote!=chunk)goto done;
    offset+=chunk;remain-=chunk;
  }
  ok=1;
done:
  CloseHandle(out);
  if(!ok)DeleteFileW(dst);
  return ok;
}
static int find_adjacent_iso(wchar_t *dest,size_t cap) {
  WIN32_FIND_DATAW entry;HANDLE search;
  wchar_t pattern[SPIDER_INSTALLER_PATH];unsigned n=0;
  if(!combine(pattern,SPIDER_INSTALLER_PATH,launcher_dir,L"*"))return -1;
  search=FindFirstFileW(pattern,&entry);
  if(search==INVALID_HANDLE_VALUE)return 0;
  do{
    if((entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       !extension_is(entry.cFileName,L".iso"))continue;
    if(++n==1&&!combine(dest,cap,launcher_dir,entry.cFileName))n=2;
  }while(FindNextFileW(search,&entry));
  FindClose(search);
  return n==1?1:n? -1:0;
}
static int deploy_payloads_to(const wchar_t *root);
static int verify_payloads(void);
static int prepare_game_from_iso(DiscImage *iso) {
  /* Supported DIRECT-FILE images only. The real retail Spider-Man CD uses
   * an installer: until its CAB/layout recipe is verified against the
   * user's media, an extracted ISO is not a complete game installation.
   * Refuse instead of copying compressed archives as runnable assets. */
  static const wchar_t *needed[]={L"SpideyPC.exe",L"data.pkr",L"media.pkr",L"binkw32.dll"};
  DiscEntry *parts[4];
  wchar_t staging[SPIDER_INSTALLER_PATH],target[SPIDER_INSTALLER_PATH];
  unsigned i;int ok=0;
  for(i=0;i<4;i++){
    parts[i]=find_disc_file(iso,needed[i]);
    if(!parts[i]){
      message(L"ISO: missing directly installable %ls; CD setup archive recipe not yet verified",needed[i]);
      return 0;
    }
  }
  if(!combine(staging,SPIDER_INSTALLER_PATH,launcher_dir,L"SpiderMan2k1Fix.installing"))return 0;
  if(GetFileAttributesW(game_dir)!=INVALID_FILE_ATTRIBUTES ||
     GetFileAttributesW(staging)!=INVALID_FILE_ATTRIBUTES){
    message(L"INSTALL: destination/staging already exists; refusing to overwrite user data");return 0;
  }
  if(!safe_directory(staging,1))return 0;
  for(i=0;i<4;i++){
    if(!combine(target,SPIDER_INSTALLER_PATH,staging,needed[i])||
       !iso_copy(iso,parts[i],target)){
      message(L"INSTALL: unable to extract verified file %ls",needed[i]);goto abort;
    }
    message(L"INSTALL: copied original %ls bytes=%lu",needed[i],parts[i]->bytes);
  }
  if(!combine(target,SPIDER_INSTALLER_PATH,staging,L"SpideyPC.exe")||
     !verified_retail(target))goto abort;
  /* Bundled assets are written in private staging BEFORE publishing.
   * No post-install update or repair mode exists. */
  if(!deploy_payloads_to(staging)){
    message(L"INSTALL: cannot stage bundled runtime files");goto abort;
  }
  if(!MoveFileExW(staging,game_dir,MOVEFILE_WRITE_THROUGH)){
    message(L"INSTALL: unable to publish staging directory (Win32 %lu)",GetLastError());goto abort;
  }
  ok=1;
abort:
  if(!ok){
    /* Clean only names created in this unique private staging folder. */
    static const wchar_t *names[]={
      L"SpideyPC.exe",L"data.pkr",L"media.pkr",L"binkw32.dll",L"winmm.dll",
      L"SpiderMan2k1Fix\\SpiderMan2k1Fix.dll",
      L"SpiderMan2k1Fix\\SDL3.dll",
      L"SpiderMan2k1Fix\\SDL3-LICENSE.txt",
      L"SpiderMan2k1Fix\\README.md",
      L"SpiderMan2k1Fix\\CHANGELOG.md",
      L"SpiderMan2k1Fix\\linux\\spiderman-focus-helper"
    };
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++)
      if(combine(target,SPIDER_INSTALLER_PATH,staging,names[i]))
        DeleteFileW(target);
    if(combine(target,SPIDER_INSTALLER_PATH,staging,L"SpiderMan2k1Fix\\linux"))
      RemoveDirectoryW(target);
    if(combine(target,SPIDER_INSTALLER_PATH,staging,L"SpiderMan2k1Fix"))
      RemoveDirectoryW(target);
    RemoveDirectoryW(staging);
  }
  return ok;
}
static int resource_bytes(WORD id,const BYTE **data,DWORD *size) {
  HRSRC res=FindResourceW(NULL,MAKEINTRESOURCEW(id),RT_RCDATA);
  HGLOBAL loaded;
  if(!res)return 0;
  loaded=LoadResource(NULL,res);
  if(!loaded)return 0;
  *data=(const BYTE *)LockResource(loaded);*size=SizeofResource(NULL,res);
  return *data&&*size;
}
static int same_file(const wchar_t *dest,const BYTE *buf,DWORD len) {
  HANDLE file;LARGE_INTEGER size;BYTE chunk[SPIDER_INSTALLER_BUFFER];
  DWORD got;DWORD offset=0;
  if(!regular_file(dest))return 0;
  file=CreateFileW(dest,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
  if(file==INVALID_HANDLE_VALUE)return 0;
  if(!GetFileSizeEx(file,&size)||(ULONGLONG)size.QuadPart!=len){CloseHandle(file);return 0;}
  while(offset<len){
    DWORD slice=len-offset>sizeof(chunk)?sizeof(chunk):len-offset;
    if(!ReadFile(file,chunk,slice,&got,NULL)||got!=slice||memcmp(chunk,buf+offset,slice)){
      CloseHandle(file);return 0;
    }
    offset+=slice;
  }
  CloseHandle(file);return 1;
}
/* Initial installation only: no automatic update or DLL replacement.
 * Never mutate an installation after it has been published. */
static int resource_install(WORD id,const wchar_t *base,const wchar_t *name){
  const BYTE *bytes;DWORD len,done;
  wchar_t dest[SPIDER_INSTALLER_PATH],temp[SPIDER_INSTALLER_PATH];
  HANDLE output;
  if(!resource_bytes(id,&bytes,&len)||
     !combine(dest,SPIDER_INSTALLER_PATH,base,name))return 0;
  if(GetFileAttributesW(dest)!=INVALID_FILE_ATTRIBUTES){
    message(L"INSTALL: refusing existing bundled asset %ls",name);return 0;
  }
  if(_snwprintf(temp,SPIDER_INSTALLER_PATH,L"%ls.new-%lu",
                dest,GetCurrentProcessId())<0)return 0;
  if(GetFileAttributesW(temp)!=INVALID_FILE_ATTRIBUTES)return 0;
  output=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,
                     FILE_ATTRIBUTE_NORMAL,NULL);
  if(output==INVALID_HANDLE_VALUE)return 0;
  if(!WriteFile(output,bytes,len,&done,NULL)||done!=len){
    CloseHandle(output);DeleteFileW(temp);return 0;
  }
  if(!FlushFileBuffers(output)){CloseHandle(output);DeleteFileW(temp);return 0;}
  CloseHandle(output);
  if(!same_file(temp,bytes,len)){DeleteFileW(temp);return 0;}
  if(!MoveFileExW(temp,dest,MOVEFILE_WRITE_THROUGH)){
    DeleteFileW(temp);return 0;
  }
  message(L"INSTALL: bundled %ls bytes=%lu",name,len);
  return 1;
}
static int resource_verify(WORD id,const wchar_t *base,const wchar_t *name){
  const BYTE *bytes;DWORD len;wchar_t dest[SPIDER_INSTALLER_PATH];
  if(!resource_bytes(id,&bytes,&len)||
     !combine(dest,SPIDER_INSTALLER_PATH,base,name)||
     !same_file(dest,bytes,len)){
    message(L"VERIFY: missing or altered runtime %ls (no automatic repair)",name);
    return 0;
  }
  return 1;
}
static int deploy_payloads_to(const wchar_t *root){
  wchar_t code[SPIDER_INSTALLER_PATH],linux[SPIDER_INSTALLER_PATH];
  if(!safe_directory(root,0) ||
     !combine(code,SPIDER_INSTALLER_PATH,root,L"SpiderMan2k1Fix")||
     !safe_directory(code,1)||
     !combine(linux,SPIDER_INSTALLER_PATH,code,L"linux")||
     !safe_directory(linux,1))return 0;
  return resource_install(RES_WINMM,root,L"winmm.dll") &&
         resource_install(RES_FIX,code,L"SpiderMan2k1Fix.dll") &&
         resource_install(RES_SDL3,code,L"SDL3.dll") &&
         resource_install(RES_SDL3_LICENSE,code,L"SDL3-LICENSE.txt") &&
         resource_install(RES_X11,linux,L"spiderman-focus-helper") &&
         resource_install(RES_README,code,L"README.md") &&
         resource_install(RES_CHANGELOG,code,L"CHANGELOG.md");
}
static int verify_payloads(void){
  wchar_t code[SPIDER_INSTALLER_PATH],linux[SPIDER_INSTALLER_PATH];
  if(!safe_directory(game_dir,0) ||
     !combine(code,SPIDER_INSTALLER_PATH,game_dir,L"SpiderMan2k1Fix")||
     !safe_directory(code,0)||
     !combine(linux,SPIDER_INSTALLER_PATH,code,L"linux")||
     !safe_directory(linux,0))return 0;
  return resource_verify(RES_WINMM,game_dir,L"winmm.dll") &&
         resource_verify(RES_FIX,code,L"SpiderMan2k1Fix.dll") &&
         resource_verify(RES_SDL3,code,L"SDL3.dll") &&
         resource_verify(RES_SDL3_LICENSE,code,L"SDL3-LICENSE.txt") &&
         resource_verify(RES_X11,linux,L"spiderman-focus-helper") &&
         resource_verify(RES_README,code,L"README.md") &&
         resource_verify(RES_CHANGELOG,code,L"CHANGELOG.md");
}
static int launch_game(void){
  wchar_t path[SPIDER_INSTALLER_PATH],cmd[SPIDER_INSTALLER_PATH+4];
  STARTUPINFOW si;PROCESS_INFORMATION pi;DWORD exit_code=0;
  if(!combine(path,SPIDER_INSTALLER_PATH,game_dir,L"SpideyPC.exe")||
     !verified_retail(path))return 0;
  memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));si.cb=sizeof(si);
  if(_snwprintf(cmd,sizeof(cmd)/sizeof(cmd[0]),L"\"%ls\"",path)<0)return 0;
  /* The winmm native proxy must be selected on Wine. Explicit child-only
   * environment inheritance; never mutate the user's profile or registry. */
  {
    wchar_t previous[1024];DWORD len=GetEnvironmentVariableW(L"WINEDLLOVERRIDES",previous,1024);
    if(!len){SetEnvironmentVariableW(L"WINEDLLOVERRIDES",L"winmm=n,b");}
    else if(len<1024 && !wcsstr(previous,L"winmm=")){
      wchar_t value[1100];
      if(_snwprintf(value,1100,L"%ls;winmm=n,b",previous)>0)
        SetEnvironmentVariableW(L"WINEDLLOVERRIDES",value);
    }
  }
  /* Pass the standalone launcher's directory to the game child so its
   * winmm proxy and core write a shared log beside SpiderMan2k1Fix.exe. */
  if(!SetEnvironmentVariableW(L"SPIDERMAN2K1FIX_LAUNCHER_DIR",launcher_dir)){
    message(L"LAUNCH: cannot set runtime log directory");return 0;
  }
  message(L"LAUNCH: %ls; runtime log beside standalone launcher",path);
  if(!CreateProcessW(path,cmd,NULL,NULL,FALSE,0,NULL,game_dir,&si,&pi)){
    message(L"LAUNCH: CreateProcess failed Win32=%lu",GetLastError());return 0;
  }
  CloseHandle(pi.hThread);WaitForSingleObject(pi.hProcess,INFINITE);
  if(!GetExitCodeProcess(pi.hProcess,&exit_code)){CloseHandle(pi.hProcess);return 0;}
  CloseHandle(pi.hProcess);
  message(L"LAUNCH: game exit code=%lu",exit_code);
  return exit_code==0;
}
static void usage(void){
  message(L"SpiderMan2k1Fix v%hs (ISO installer bootstrap; NOT retail-CD validated)",SPIDERMAN2K1FIX_VERSION);
  message(L"Usage: SpiderMan2k1Fix.exe [--inspect | --verify | --version | --help]");
  message(L"Place exactly one original .iso beside the launcher. Safe inspection is ready;");
  message(L"retail InstallShield/CAB layout requires source validation before installation.");
  message(L"Logs next to launcher: SpiderMan2k1Fix_installer.log + SpiderMan2k1Fix.log");
}
int wmain(int argc,wchar_t **argv) {
  wchar_t executable[SPIDER_INSTALLER_PATH],*slash,path[SPIDER_INSTALLER_PATH];
  wchar_t gameexe[SPIDER_INSTALLER_PATH];
  DiscImage iso;int result=1,scan;
  DWORD length=GetModuleFileNameW(NULL,executable,SPIDER_INSTALLER_PATH);
  if(!length||length>=SPIDER_INSTALLER_PATH)return 2;
  slash=wcsrchr(executable,L'\\');if(!slash)return 2;
  *slash=0;wcscpy(launcher_dir,executable);
  if(!combine(game_dir,SPIDER_INSTALLER_PATH,launcher_dir,L"SpiderMan2k1Fix")||
     !combine(log_path,SPIDER_INSTALLER_PATH,launcher_dir,L"SpiderMan2k1Fix_installer.log"))return 2;
  logfile=_wfopen(log_path,L"w, ccs=UTF-8");
  message(L"START: SpiderMan2k1Fix installer v%hs; Win32/Wine shared code; source ISO unchanged",SPIDERMAN2K1FIX_VERSION);
  message(L"PATH: install root=%ls",game_dir);
  if(argc>2){usage();goto done;}
  if(argc==2 && (!wcscmp(argv[1],L"--help")||!wcscmp(argv[1],L"-h"))){usage();result=0;goto done;}
  if(argc==2 && !wcscmp(argv[1],L"--version")){result=0;goto done;}
  if(!combine(gameexe,SPIDER_INSTALLER_PATH,game_dir,L"SpideyPC.exe"))goto done;
  if(argc==2 && !wcscmp(argv[1],L"--verify")){
    result=(verified_retail(gameexe)&&verify_payloads())?0:1;goto done;
  }
  if(argc==2 && wcscmp(argv[1],L"--inspect")){usage();goto done;}
  if(argc==1 && regular_file(gameexe)){
    /* Existing installation: verify and launch. NEVER update or repair. */
    if(!verified_retail(gameexe) || !verify_payloads()){
      message(L"VERIFY: existing installation invalid; no automatic change");goto done;
    }
    result=launch_game()?0:1;goto done;
  }
  scan=find_adjacent_iso(path,SPIDER_INSTALLER_PATH);
  if(scan!=1){
    message(L"ISO: expected one adjacent .iso; detected none or multiple (state=%d)",scan);
    goto done;
  }
  message(L"ISO: inspecting %ls",path);
  if(!iso_open(&iso,path)){iso_close(&iso);goto done;}
  {DWORD n,listed=0;
    for(n=0;n<iso.count;n++)if(!iso.entries[n].directory){
      message(L"ISO-FILE: %ls size=%lu lba=%lu",iso.entries[n].name,
        iso.entries[n].bytes,iso.entries[n].lba);
      listed++;
    }
    message(L"ISO: inspected %lu regular files",listed);
  }
  if(argc==2 && !wcscmp(argv[1],L"--inspect")){iso_close(&iso);result=0;goto done;}
  if(!prepare_game_from_iso(&iso)){
    message(L"INSTALL: CD image cannot yet be installed safely without the original setup recipe; no partial game was published");
    iso_close(&iso);goto done;
  }
  iso_close(&iso);
  /* The original game and bundled runtimes were staged and published
   * together. Never deploy or update components after publication. */
  result=launch_game()?0:1;
done:
  message(L"END: status=%d",result);
  if(logfile)fclose(logfile);
  return result;
}

