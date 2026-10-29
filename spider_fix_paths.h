/* Shared executable-relative layout for proxy and core (Windows/Wine).
 * This header is deliberately self-contained: winmm.dll is loaded before
 * the core DLL, so neither binary can rely on path state in the other.
 * No path operations are allowed from either DLL's DllMain.
 */
#ifndef SPIDERMAN2K1FIX_PATHS_H
#define SPIDERMAN2K1FIX_PATHS_H
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static inline int spider_path_join_a(char *out,size_t cap,
                                     const char *dir,const char *name) {
  int n;
  if(!out || !cap || !dir || !*dir || !name || !*name)return 0;
  n=snprintf(out,cap,"%s\\%s",dir,name);
  return n>0 && (size_t)n<cap;
}
static inline int spider_path_exe_dir_a(char *out,size_t cap) {
  char exe[MAX_PATH],*slash;
  DWORD n=GetModuleFileNameA(NULL,exe,MAX_PATH);
  size_t len;
  if(!out || !cap || !n || n>=MAX_PATH)return 0;
  slash=strrchr(exe,'\\');
  if(!slash)return 0;
  len=(size_t)(slash-exe);
  if(len+1u>cap)return 0;
  memcpy(out,exe,len);
  out[len]=0;
  return 1;
}
static inline int spider_path_file_a(const char *path) {
  DWORD attr=path?GetFileAttributesA(path):INVALID_FILE_ATTRIBUTES;
  return attr!=INVALID_FILE_ATTRIBUTES &&
         !(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
/* Reject a junction/symlink as the destination for game-generated files. */
static inline int spider_path_dir_a(const char *path,int create) {
  DWORD attr;
  if(!path || !*path)return 0;
  attr=GetFileAttributesA(path);
  if(attr==INVALID_FILE_ATTRIBUTES && create) {
    if(!CreateDirectoryA(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)
      return 0;
    attr=GetFileAttributesA(path);
  }
  return attr!=INVALID_FILE_ATTRIBUTES &&
         (attr&FILE_ATTRIBUTE_DIRECTORY) &&
         !(attr&FILE_ATTRIBUTE_REPARSE_POINT);
}
static inline int spider_path_writable_a(const char *dir) {
  char tmp[MAX_PATH];
  unsigned i;
  if(!spider_path_dir_a(dir,0))return 0;
  for(i=0;i<4;i++) {
    HANDLE file;
    int n=snprintf(tmp,sizeof(tmp),"%s\\.SpiderMan2k1Fix-%lu-%lu.tmp",
                   dir,(unsigned long)GetCurrentProcessId(),
                   (unsigned long)(GetTickCount()+i));
    if(n<0 || (size_t)n>=sizeof(tmp))return 0;
    file=CreateFileA(tmp,GENERIC_WRITE,0,NULL,CREATE_NEW,
                      FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(file!=INVALID_HANDLE_VALUE) {CloseHandle(file);return 1;}
    if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS)
      return 0;
  }
  return 0;
}
static inline int spider_path_roots_a(char *game,size_t gamecap,
                                      char *local,size_t localcap) {
  char dir[MAX_PATH],app[MAX_PATH];
  DWORD n;
  if(game && gamecap)game[0]=0;
  if(local && localcap)local[0]=0;
  if(!spider_path_exe_dir_a(dir,sizeof(dir)))return 0;
  if(!spider_path_join_a(game,gamecap,dir,"SpiderMan2k1Fix"))return 0;
  n=GetEnvironmentVariableA("LOCALAPPDATA",app,sizeof(app));
  if(n && n<sizeof(app) && local && localcap)
    spider_path_join_a(local,localcap,app,"SpiderMan2k1Fix");
  return 1;
}
static inline int spider_path_asset_a(const char *filename,
                                      char *out,size_t cap) {
  char dir[MAX_PATH],local[MAX_PATH];
  return spider_path_roots_a(dir,sizeof(dir),local,sizeof(local)) &&
         spider_path_join_a(out,cap,dir,filename);
}
/* Transactional, non-overwriting legacy migration. Multiple proxy/core
 * calls can overlap at startup: copy to a private file, then publish the
 * complete bytes atomically without replacing an existing destination.
 * Never delete, truncate or rewrite the user's original root INI. */
static inline int spider_path_migrate_ini_a(const char *legacy,
                                            const char *dest) {
  char temp[MAX_PATH];
  int n;
  if(spider_path_file_a(dest))return 1;
  if(!spider_path_file_a(legacy))return 0;
  n=snprintf(temp,sizeof(temp),"%s.migrate-%lu-%lu.tmp",
             dest,(unsigned long)GetCurrentProcessId(),
             (unsigned long)GetCurrentThreadId());
  if(n<=0 || (size_t)n>=sizeof(temp))return 0;
  if(!CopyFileA(legacy,temp,TRUE))return spider_path_file_a(dest);
  if(MoveFileExA(temp,dest,MOVEFILE_WRITE_THROUGH))return 1;
  DeleteFileA(temp);
  return spider_path_file_a(dest);
}
/* New location takes priority. A pre-existing fallback overrides uncreated
 * game paths. Copy legacy root INI EXACTLY ONCE, only when target is absent.
 * Never remove the original. If both target folders are read-only, keep the
 * legacy INI for this session, rather than silently resetting user settings.
 * New installs use a writable game subfolder, then LOCALAPPDATA fallback. */
static inline int spider_path_ini_a(char *out,size_t cap) {
  char game[MAX_PATH],local[MAX_PATH],legacy[MAX_PATH],target[MAX_PATH];
  char exe_dir[MAX_PATH];
  if(!out || !cap || !spider_path_roots_a(game,sizeof(game),
                                          local,sizeof(local)) ||
     !spider_path_exe_dir_a(exe_dir,sizeof(exe_dir)) ||
     !spider_path_join_a(legacy,sizeof(legacy),exe_dir,"SpiderMan2k1Fix.ini"))
    return 0;
  if(spider_path_dir_a(game,0) &&
     spider_path_join_a(target,sizeof(target),game,"SpiderMan2k1Fix.ini") &&
     spider_path_file_a(target)) {
    if(strlen(target)+1u>cap)return 0;
    strcpy(out,target);return 1;
  }
  if(spider_path_dir_a(local,0) &&
     spider_path_join_a(target,sizeof(target),local,"SpiderMan2k1Fix.ini") &&
     spider_path_file_a(target)) {
    if(strlen(target)+1u>cap)return 0;
    strcpy(out,target);return 1;
  }
  if(spider_path_file_a(legacy)) {
    if(spider_path_dir_a(game,1) &&
       spider_path_join_a(target,sizeof(target),game,"SpiderMan2k1Fix.ini") &&
       spider_path_migrate_ini_a(legacy,target)) {
      if(strlen(target)+1u>cap)return 0;
      strcpy(out,target);return 1;
    }
    if(spider_path_dir_a(local,1) &&
       spider_path_join_a(target,sizeof(target),local,"SpiderMan2k1Fix.ini") &&
       spider_path_migrate_ini_a(legacy,target)) {
      if(strlen(target)+1u>cap)return 0;
      strcpy(out,target);return 1;
    }
    if(strlen(legacy)+1u>cap)return 0;
    strcpy(out,legacy);return 1;
  }
  if(spider_path_dir_a(game,1) && spider_path_writable_a(game) &&
     spider_path_join_a(target,sizeof(target),game,"SpiderMan2k1Fix.ini")) {
    if(strlen(target)+1u>cap)return 0;
    strcpy(out,target);return 1;
  }
  if(spider_path_dir_a(local,1) && spider_path_writable_a(local) &&
     spider_path_join_a(target,sizeof(target),local,"SpiderMan2k1Fix.ini")) {
    if(strlen(target)+1u>cap)return 0;
    strcpy(out,target);return 1;
  }
  return 0;
}
static inline int spider_path_log_a(char *out,size_t cap) {
  char ini[MAX_PATH],*slash;
  if(!spider_path_ini_a(ini,sizeof(ini)))return 0;
  slash=strrchr(ini,'\\');
  if(!slash)return 0;
  strcpy(slash+1,"SpiderMan2k1Fix.log");
  if(strlen(ini)+1u>cap)return 0;
  strcpy(out,ini);
  return 1;
}
/* Prefer new subfolder, accept root-only legacy installs until replaced.
 * Never fall back to a different DLL if the preferred file exists but
 * fails to load: such an installation needs an actionable error. */
static inline int spider_path_code_asset_a(const char *filename,
                                            char *out,size_t cap) {
  char preferred[MAX_PATH],root[MAX_PATH],legacy[MAX_PATH];
  if(!filename || !spider_path_asset_a(filename,preferred,sizeof(preferred)) ||
     !spider_path_exe_dir_a(root,sizeof(root)) ||
     !spider_path_join_a(legacy,sizeof(legacy),root,filename))return 0;
  if(spider_path_file_a(preferred) || !spider_path_file_a(legacy)) {
    if(strlen(preferred)+1u>cap)return 0;
    strcpy(out,preferred);
  } else {
    if(strlen(legacy)+1u>cap)return 0;
    strcpy(out,legacy);
  }
  return 1;
}
static inline int spider_path_dir_w(const WCHAR *path,int create) {
  DWORD attr;
  if(!path || !*path)return 0;
  attr=GetFileAttributesW(path);
  if(attr==INVALID_FILE_ATTRIBUTES && create) {
    if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)
      return 0;
    attr=GetFileAttributesW(path);
  }
  return attr!=INVALID_FILE_ATTRIBUTES &&
         (attr&FILE_ATTRIBUTE_DIRECTORY) &&
         !(attr&FILE_ATTRIBUTE_REPARSE_POINT);
}
static inline int spider_path_writable_w(const WCHAR *dir) {
  WCHAR tmp[MAX_PATH];
  HANDLE file;
  int n;
  if(!spider_path_dir_w(dir,0))return 0;
  n=swprintf(tmp,MAX_PATH,L"%ls\\.SpiderMan2k1Fix-%lu-%lu.tmp",
             dir,(unsigned long)GetCurrentProcessId(),
             (unsigned long)GetTickCount());
  if(n<0 || n>=MAX_PATH)return 0;
  file=CreateFileW(tmp,GENERIC_WRITE,0,NULL,CREATE_NEW,
                   FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
  if(file==INVALID_HANDLE_VALUE)return 0;
  CloseHandle(file);
  return 1;
}
/* Prefer <game>\SpiderMan2k1Fix\screenshots. If game folder is
 * unwritable, use per-user LOCALAPPDATA\SpiderMan2k1Fix\screenshots.
 * Both directory tests and temporary PNG publication use wide paths. */
static inline int spider_path_screenshots_w(WCHAR *out,size_t cap) {
  WCHAR exe[MAX_PATH],game[MAX_PATH],local[MAX_PATH],app[MAX_PATH],*slash;
  DWORD n=GetModuleFileNameW(NULL,exe,MAX_PATH),m;
  int rc;
  if(!out || !cap || !n || n>=MAX_PATH)return 0;
  slash=wcsrchr(exe,L'\\');
  if(!slash)return 0;
  *slash=0;
  rc=swprintf(game,MAX_PATH,L"%ls\\SpiderMan2k1Fix",exe);
  if(rc>0 && rc<MAX_PATH && spider_path_dir_w(game,1)) {
    rc=swprintf(out,cap,L"%ls\\screenshots",game);
    if(rc>0 && (size_t)rc<cap && spider_path_dir_w(out,1) &&
       spider_path_writable_w(out))return 1;
  }
  m=GetEnvironmentVariableW(L"LOCALAPPDATA",app,MAX_PATH);
  if(!m || m>=MAX_PATH)return 0;
  rc=swprintf(local,MAX_PATH,L"%ls\\SpiderMan2k1Fix",app);
  if(rc<=0 || rc>=MAX_PATH || !spider_path_dir_w(local,1))return 0;
  rc=swprintf(out,cap,L"%ls\\screenshots",local);
  return rc>0 && (size_t)rc<cap && spider_path_dir_w(out,1) &&
         spider_path_writable_w(out);
}
#endif
