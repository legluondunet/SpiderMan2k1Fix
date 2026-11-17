/* Win32 first-run diagnostic policy test (no proprietary game files). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include "../../first_run_log_policy.h"

#define CHECK(e,msg) do { if(!(e)){ \
  fprintf(stderr,"FIRST-RUN-LOG: FAIL %s error=%lu\n",msg, \
          (unsigned long)GetLastError()); goto done; \
} } while(0)

int main(void) {
  wchar_t temp[MAX_PATH],folder[MAX_PATH],ini[MAX_PATH];
  char inia[MAX_PATH*4];
  HANDLE f=INVALID_HANDLE_VALUE;
  DWORD n,written;
  const char *user_disabled="[Debug]\r\nLog=0\r\n";
  const char *user_enabled="[Debug]\r\nLog=1\r\n";
  const char *entries[]={user_disabled,user_enabled};
  unsigned i;
  int success=0,isdir=0;
  n=GetTempPathW(MAX_PATH,temp);
  if(!n || n>=MAX_PATH)return 2;
  if(!GetTempFileNameW(temp,L"sfl",0,folder))return 2;
  if(!DeleteFileW(folder) || !CreateDirectoryW(folder,NULL))return 2;
  if(swprintf(ini,MAX_PATH,L"%ls\\SpiderMan2k1Fix.ini",folder)<0)
    goto done;
  if(!WideCharToMultiByte(CP_ACP,0,ini,-1,inia,sizeof(inia),NULL,NULL))
    goto done;
  CHECK(SpiderFirstRunLog_IniMissingW(ini) &&
        SpiderFirstRunLog_IniMissingA(inia),"missing ini enables one-shot log");
  for(i=0;i<2;i++){
    f=CreateFileW(ini,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                  FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(f!=INVALID_HANDLE_VALUE,"create chosen user INI");
    CHECK(WriteFile(f,entries[i],(DWORD)strlen(entries[i]),&written,NULL) &&
          written==strlen(entries[i]),"write chosen Log value");
    CloseHandle(f);f=INVALID_HANDLE_VALUE;
    CHECK(!SpiderFirstRunLog_IniMissingW(ini) &&
          !SpiderFirstRunLog_IniMissingA(inia),
          "existing Log=0 or Log=1 must not be auto-overridden");
  }
  CHECK(DeleteFileW(ini),"remove test INI");
  CHECK(CreateDirectoryW(ini,NULL),"create same-named occupied directory");
  isdir=1;
  CHECK(!SpiderFirstRunLog_IniMissingW(ini) &&
        !SpiderFirstRunLog_IniMissingA(inia),
        "directory at INI path is not a first-run trigger");
  success=1;
done:
  if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
  if(isdir)RemoveDirectoryW(ini);
  else DeleteFileW(ini);
  RemoveDirectoryW(folder);
  if(success)puts("PASS: first session gets automatic log; existing Log=0/1 remains user-controlled");
  return success?0:1;
}
