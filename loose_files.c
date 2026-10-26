/* Optional PKR_ReadFile detour for game/data loose-file overrides.
 * Verified SpideyPC.exe only; never edits PKR archives or native CRC code.
 * Keep the game's own memory allocator and original failure fallback.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "loose_files.h"
#include "loose_files_path.h"

#define SPIDER_PKR_READ ((BYTE *)(uintptr_t)0x00519194u)
#define SPIDER_NATIVE_NEW ((void *(__cdecl *)(unsigned))(uintptr_t)0x00529BA2u)
#define SPIDER_NATIVE_DELETE ((void (__cdecl *)(void *))(uintptr_t)0x00529BB0u)
#define SPIDER_LOOSE_LIMIT (128u*1024u*1024u)
#define SPIDER_PROLOGUE_BYTES 9u

typedef unsigned char (__cdecl *SpiderPKRReadFn)(
  void *pkr,const char *directory,const char *filename,void **result,int *size);
static const BYTE pkr_prologue[SPIDER_PROLOGUE_BYTES]={
  0x55,0x8B,0xEC,0x81,0xEC,0x80,0x00,0x00,0x00
};
static SpiderPKRReadFn original_read=NULL;
static int enabled=0;
static char game_directory[MAX_PATH];
static void (*logger)(const char *)=NULL;

static void log_msg(const char *msg){if(logger)logger(msg);}
static void rel_jump(BYTE *bytes,const BYTE *execution_address,
                     const BYTE *destination){
  uint32_t delta=(uint32_t)(uintptr_t)destination-
                 (uint32_t)(uintptr_t)(execution_address+5);
  bytes[0]=0xE9;
  memcpy(bytes+1,&delta,sizeof(delta));
}
/* Reject junctions and symbolic links in data/ and every descendant.
 * Syntax-only path checking is insufficient under Win32. */
static int no_reparse_points(char *path,size_t base){
  char *p=path+base,*end=path+strlen(path);
  DWORD attrs;
  if(base>=strlen(path))return 0;
  for(;p<=end;p++){
    if(*p=='\\' || *p==0){
      char save=*p;
      *p=0;
      attrs=GetFileAttributesA(path);
      *p=save;
      if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&FILE_ATTRIBUTE_REPARSE_POINT))
        return 0;
    }
  }
  return 1;
}
static int read_override(const char *directory,const char *filename,
                         void **result,int *size){
  char path[MAX_PATH],msg[350];
  DWORD got=0,requested;
  LARGE_INTEGER length;
  HANDLE handle;
  void *buffer;
  if(!result||!size || !spider_loose_path(path,sizeof(path),
       game_directory,directory,filename))return 0;
  if(!no_reparse_points(path,strlen(game_directory)))return 0;
  handle=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL,NULL);
  if(handle==INVALID_HANDLE_VALUE)return 0;
  if(!GetFileSizeEx(handle,&length) || length.QuadPart<=0 ||
     length.QuadPart>SPIDER_LOOSE_LIMIT){
    CloseHandle(handle);
    return 0;
  }
  requested=(DWORD)length.QuadPart;
  /* Native PKR_ReadFile also calls this new operator. The game destroys
   * the resulting buffer using the matching native operator delete. */
  buffer=SPIDER_NATIVE_NEW(requested);
  if(!buffer){
    CloseHandle(handle);
    return 0;
  }
  if(!ReadFile(handle,buffer,requested,&got,NULL) || got!=requested){
    SPIDER_NATIVE_DELETE(buffer);
    CloseHandle(handle);
    return 0;
  }
  CloseHandle(handle);
  *result=buffer;
  *size=(int)requested;
  snprintf(msg,sizeof(msg),"LOOSE-FILES: override [%s] %s (%lu bytes)",
           directory,filename,(unsigned long)requested);
  log_msg(msg);
  return 1;
}
static unsigned char __cdecl hooked_pkr_read(
  void *pkr,const char *directory,const char *filename,void **result,int *size){
  if(enabled && directory && filename && read_override(directory,filename,result,size))
    return 1;
  return original_read?original_read(pkr,directory,filename,result,size):0;
}
static int install_pkr_hook(void){
  BYTE *target=SPIDER_PKR_READ,*trampoline;
  BYTE patch[SPIDER_PROLOGUE_BYTES];
  DWORD old_protect,unused;
  if(memcmp(target,pkr_prologue,sizeof(pkr_prologue)))return 0;
  trampoline=(BYTE*)VirtualAlloc(NULL,32,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
  if(!trampoline)return 0;
  memcpy(trampoline,target,SPIDER_PROLOGUE_BYTES);
  rel_jump(trampoline+SPIDER_PROLOGUE_BYTES,trampoline+SPIDER_PROLOGUE_BYTES,
           target+SPIDER_PROLOGUE_BYTES);
  FlushInstructionCache(GetCurrentProcess(),trampoline,SPIDER_PROLOGUE_BYTES+5);
  memset(patch,0x90,sizeof(patch));
  rel_jump(patch,target,(const BYTE*)hooked_pkr_read);
  if(!VirtualProtect(target,SPIDER_PROLOGUE_BYTES,PAGE_EXECUTE_READWRITE,&old_protect)){
    VirtualFree(trampoline,0,MEM_RELEASE);return 0;
  }
  /* Install only during the guarded early startup, before game PKR reads. */
  original_read=(SpiderPKRReadFn)(void *)trampoline;
  enabled=1;
  memcpy(target,patch,sizeof(patch));
  FlushInstructionCache(GetCurrentProcess(),target,sizeof(patch));
  if(!VirtualProtect(target,SPIDER_PROLOGUE_BYTES,old_protect,&unused)){
    enabled=0;
    memcpy(target,pkr_prologue,sizeof(pkr_prologue));
    FlushInstructionCache(GetCurrentProcess(),target,sizeof(pkr_prologue));
    VirtualProtect(target,SPIDER_PROLOGUE_BYTES,old_protect,&unused);
    original_read=NULL;
    VirtualFree(trampoline,0,MEM_RELEASE);
    return 0;
  }
  return 1;
}
void SpiderLoose_Init(void (*log_fn)(const char *)){
  char ini[MAX_PATH],path[MAX_PATH],*slash;
  DWORD n;
  logger=log_fn;
  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if(!n||n>=sizeof(path)||(slash=strrchr(path,'\\'))==NULL)return;
  if((size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1>=sizeof(path))return;
  strcpy(ini,path);
  strcpy(ini+(slash-path)+1,"SpiderMan2k1Fix.ini");
  if(GetPrivateProfileIntA("Modding","LooseFiles",0,ini)!=1){
    log_msg("LOOSE-FILES: disabled (default); native PKR reader unchanged");
    return;
  }
  /* Game install folder is authoritative; do not use current directory. */
  *(slash+1)=0;
  strcpy(game_directory,path);
  if(!install_pkr_hook()){
    log_msg("LOOSE-FILES: NOT INSTALLED (unexpected PKR_ReadFile bytes or memory protection failure)");
    return;
  }
  log_msg("LOOSE-FILES: enabled; data\\ overrides PKR; absent/invalid files fall back to archive");
}
