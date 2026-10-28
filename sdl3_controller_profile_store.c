/* Windows-only, transactional SDL3 controller profile storage.
 * Preferred: <EXE>\\SpiderMan2k1Fix\\controllers\\VID-PID.ini
 * Fallback:  %LOCALAPPDATA%\\SpiderMan2k1Fix\\controllers\\VID-PID.ini
 * Local override takes precedence, so edits remain stable if game folder
 * is read-only. All paths are generated from fixed roots and numeric IDs.
 * Never write the original game's Spidey.cfg or main fix INI. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_controller_profile_store.h"
#include "sdl3_controller_profile_ini.h"
#define SPIDER_STORE_PATH_CAP 600u
static char game_root[SPIDER_STORE_PATH_CAP];
static char local_root[SPIDER_STORE_PATH_CAP];
static unsigned store_serial=0;
static int store_enabled=0;
static void (*store_report)(const char *)=NULL;
static void store_log(const char *msg) {
  if(store_report)store_report(msg);
}
static int join_dir(char *dst,size_t cap,const char *root,
                    const char *suffix) {
  int n;
  if(!dst || !root || !*root || !suffix)return 0;
  n=snprintf(dst,cap,"%s\\%s",root,suffix);
  return n>0 && (size_t)n<cap;
}
void SpiderSDL3ProfileStore_Init(int enabled,void (*report)(const char *)) {
  char exe[SPIDER_STORE_PATH_CAP],appdata[SPIDER_STORE_PATH_CAP];
  char *slash,*other;
  DWORD n;
  store_report=report;store_enabled=0;
  game_root[0]=0;local_root[0]=0;
  if(!enabled)return;
  n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(n && n<sizeof(exe)) {
    slash=strrchr(exe,'\\');
    other=strrchr(exe,'/');
    if(other && (!slash || other>slash))slash=other;
    if(slash) {
      *slash=0;
      join_dir(game_root,sizeof(game_root),exe,"SpiderMan2k1Fix");
    }
  }
  n=GetEnvironmentVariableA("LOCALAPPDATA",appdata,sizeof(appdata));
  if(n && n<sizeof(appdata))
    join_dir(local_root,sizeof(local_root),appdata,"SpiderMan2k1Fix");
  if(!game_root[0] && !local_root[0]) {
    store_log("SDL3-PROFILE: unavailable: neither game directory nor LOCALAPPDATA resolved");
    return;
  }
  store_enabled=1;
  store_log("SDL3-PROFILE: persistent per-model controller storage enabled (game directory, fallback LOCALAPPDATA; gameplay unchanged)");
}
static int path_for(const char *root,uint16_t vid,uint16_t pid,
                    char *out,size_t cap) {
  int n;
  if(!root || !*root || !vid || !pid)return 0;
  n=snprintf(out,cap,"%s\\controllers\\%04X-%04X.ini",
             root,(unsigned)vid,(unsigned)pid);
  return n>0 && (size_t)n<cap;
}
/* Do not follow reparse-point directories or controller INI files.
 * A user-controlled symlink must not redirect our profile writes. */
static int safe_dir(const char *dir,int create) {
  DWORD attr;
  if(!dir || !*dir)return 0;
  attr=GetFileAttributesA(dir);
  if(attr==INVALID_FILE_ATTRIBUTES && create) {
    if(!CreateDirectoryA(dir,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)
      return 0;
    attr=GetFileAttributesA(dir);
  }
  return attr!=INVALID_FILE_ATTRIBUTES &&
         (attr&FILE_ATTRIBUTE_DIRECTORY) &&
         !(attr&FILE_ATTRIBUTE_REPARSE_POINT);
}
static int ensure_dirs(const char *root) {
  char folder[SPIDER_STORE_PATH_CAP];
  if(!safe_dir(root,1) ||
     !join_dir(folder,sizeof(folder),root,"controllers"))return 0;
  return safe_dir(folder,1);
}
/* A 0-length/oversized/unreadable or wrong-version file never gets
 * partially applied to the in-memory profile. */
static int read_path(const char *path,uint16_t vid,uint16_t pid,
                     SpiderSDL3ControllerProfile *out) {
  DWORD attr,size,nread=0;
  BY_HANDLE_FILE_INFORMATION meta;
  HANDLE file;
  char buffer[SPIDER_SDL3_PROFILE_INI_MAX+1];
  int result=-2;
  attr=GetFileAttributesA(path);
  if(attr==INVALID_FILE_ATTRIBUTES) {
    DWORD err=GetLastError();
    return (err==ERROR_FILE_NOT_FOUND || err==ERROR_PATH_NOT_FOUND)?
           0:-2;
  }
  if(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return -1;
  file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
  if(file==INVALID_HANDLE_VALUE)return -2;
  if(!GetFileInformationByHandle(file,&meta) ||
     (meta.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
    goto done;
  if(meta.nFileSizeHigh!=0u ||
     meta.nFileSizeLow==0u ||
     meta.nFileSizeLow>SPIDER_SDL3_PROFILE_INI_MAX) {
    result=-1;goto done;
  }
  size=meta.nFileSizeLow;
  if(!ReadFile(file,buffer,size,&nread,NULL) || nread!=size)goto done;
  result=spider_sdl3_ini_decode(buffer,size,vid,pid,out)?1:-1;
done:
  CloseHandle(file);
  return result;
}
int SpiderSDL3ProfileStore_Load(uint16_t vid,uint16_t pid,
                                SpiderSDL3ControllerProfile *out) {
  char path[SPIDER_STORE_PATH_CAP],log[200];
  int rc;
  if(!store_enabled || !out || !vid || !pid)return 0;
  /* Fallback override is authoritative if present, even if damaged:
   * never silently read/overwrite the game's older profile instead. */
  if(local_root[0] && path_for(local_root,vid,pid,path,sizeof(path))) {
    rc=read_path(path,vid,pid,out);
    if(rc!=0) {
      snprintf(log,sizeof(log),
        "SDL3-PROFILE: LOCALAPPDATA model=%04X-%04X %s",
        (unsigned)vid,(unsigned)pid,
        rc==1?"LOADED":rc==-1?"INVALID; defaults retained":"READ ERROR; defaults retained");
      store_log(log);return rc;
    }
  }
  if(game_root[0] && path_for(game_root,vid,pid,path,sizeof(path))) {
    rc=read_path(path,vid,pid,out);
    if(rc!=0) {
      snprintf(log,sizeof(log),
        "SDL3-PROFILE: GAME model=%04X-%04X %s",
        (unsigned)vid,(unsigned)pid,
        rc==1?"LOADED":rc==-1?"INVALID; defaults retained":"READ ERROR; defaults retained");
      store_log(log);return rc;
    }
  }
  return 0;
}
/* Preserve malformed existing content before replacing it. An existing
 * .invalid.bak means it was backed up on a prior save. */
static int preserve_invalid(const char *path,uint16_t vid,uint16_t pid) {
  char backup[SPIDER_STORE_PATH_CAP];
  SpiderSDL3ControllerProfile trial;
  int rc=read_path(path,vid,pid,&trial);
  DWORD attr;
  int n;
  if(rc==0 || rc==1)return 1;
  if(rc==-2)return 0;
  n=snprintf(backup,sizeof(backup),"%s.invalid.bak",path);
  if(n<=0 || (size_t)n>=sizeof(backup))return 0;
  attr=GetFileAttributesA(backup);
  if(attr!=INVALID_FILE_ATTRIBUTES)
    return (attr&FILE_ATTRIBUTE_DIRECTORY)==0 &&
           !(attr&FILE_ATTRIBUTE_REPARSE_POINT);
  return CopyFileA(path,backup,TRUE)!=0;
}
/* Atomic replacement on the same filesystem, with flush-before-rename.
 * No live profile changes until both serialization and rename succeed. */
static int write_path(const char *root,
                      const SpiderSDL3ControllerProfile *profile) {
  char path[SPIDER_STORE_PATH_CAP],temp[SPIDER_STORE_PATH_CAP];
  char encoded[SPIDER_SDL3_PROFILE_INI_MAX+1];
  size_t bytes=0;
  DWORD written=0;
  int n,ok=0;
  HANDLE file;
  if(!root || !*root ||
     !path_for(root,profile->vendor,profile->product,path,sizeof(path)) ||
     !spider_sdl3_ini_encode(profile,encoded,sizeof(encoded),&bytes) ||
     !ensure_dirs(root) ||
     !preserve_invalid(path,profile->vendor,profile->product))return 0;
  store_serial++;
  n=snprintf(temp,sizeof(temp),"%s.tmp-%08lX-%08lX-%08X",
      path,(unsigned long)GetCurrentProcessId(),
      (unsigned long)GetTickCount(),store_serial);
  if(n<=0 || (size_t)n>=sizeof(temp))return 0;
  file=CreateFileA(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,
                   FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
  if(file==INVALID_HANDLE_VALUE)return 0;
  ok=WriteFile(file,encoded,(DWORD)bytes,&written,NULL) &&
     written==(DWORD)bytes && FlushFileBuffers(file);
  if(!CloseHandle(file))ok=0;
  if(ok)
    ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|
                             MOVEFILE_WRITE_THROUGH);
  if(!ok)DeleteFileA(temp);
  return ok?1:0;
}
int SpiderSDL3ProfileStore_Save(const SpiderSDL3ControllerProfile *profile) {
  char path[SPIDER_STORE_PATH_CAP],log[220];
  int prefer_local=0;
  if(!store_enabled || !spider_sdl3_profile_valid(profile))return 0;
  if(local_root[0] &&
     path_for(local_root,profile->vendor,profile->product,
              path,sizeof(path))) {
    DWORD attr=GetFileAttributesA(path);
    /* Existing LOCALAPPDATA override must stay authoritative. */
    prefer_local=(attr!=INVALID_FILE_ATTRIBUTES);
  }
  if(prefer_local) {
    if(write_path(local_root,profile))goto local_saved;
  } else if(game_root[0] && write_path(game_root,profile)) {
    snprintf(log,sizeof(log),"SDL3-PROFILE: SAVED GAME %04X-%04X.ini (atomic)",
             (unsigned)profile->vendor,(unsigned)profile->product);
    store_log(log);return 1;
  }
  if(local_root[0] && write_path(local_root,profile)) {
local_saved:
    snprintf(log,sizeof(log),
      "SDL3-PROFILE: SAVED LOCALAPPDATA %04X-%04X.ini (atomic; game path not used)",
      (unsigned)profile->vendor,(unsigned)profile->product);
    store_log(log);return 1;
  }
  snprintf(log,sizeof(log),
    "SDL3-PROFILE: SAVE FAILED model=%04X-%04X; previous in-memory profile retained",
    (unsigned)profile->vendor,(unsigned)profile->product);
  store_log(log);
  return 0;
}
