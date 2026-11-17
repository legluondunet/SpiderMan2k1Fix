/* Native Windows x86 synthetic suspended-process regression test.
 * No original game data is loaded or distributed. The child process runs at
 * IMAGE_BASE=0x10000000, while a synthetic retail-sized PE32 image is allocated at 0x400000.
 * The actual production remote-memory patcher is linked unchanged.
 *
 * Every failure kills the suspended child; NO thread is ever resumed.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../installer/unified_import_alias.h"

#define DUMMY_BASE ((uintptr_t)0x00400000u)
#define DUMMY_SIZE 0x02A0D000u /* exact retail mapped image, including huge .data BSS */
#define IMPORT_DESC 0x00130000u
#define IMPORT_NAME 0x00131000u
#define ORIGINAL_THUNK 0x00132000u
#define FUNCTION_NAME 0x00133000u
#define FUNCTION_NAME2 0x00133100u
#define IAT_THUNK 0x0013b010u

static int put(HANDLE process,uintptr_t offset,const void *bytes,SIZE_T n) {
  SIZE_T written=0;
  return WriteProcessMemory(process,(void*)(DUMMY_BASE+offset),
                            bytes,n,&written) && written==n;
}
static int readback(HANDLE process,uintptr_t offset,void *bytes,SIZE_T n) {
  SIZE_T got=0;
  return ReadProcessMemory(process,(void*)(DUMMY_BASE+offset),
                           bytes,n,&got) && got==n;
}
static int fixture(HANDLE p) {
  IMAGE_DOS_HEADER mz;
  IMAGE_NT_HEADERS32 nt;
  IMAGE_IMPORT_DESCRIPTOR desc[2];
  IMAGE_THUNK_DATA32 thunk[3];
  static const char dllname[10]="DDRAW.dll";
  static const BYTE symbol[]={
      0,0,'D','i','r','e','c','t','D','r','a','w',
      'C','r','e','a','t','e','E','x',0};
  static const BYTE symbol2[]={
      0,0,'D','i','r','e','c','t','D','r','a','w',
      'E','n','u','m','e','r','a','t','e','E','x','A',0};
  static const BYTE nocd[]={0x81,0xEC,0x20,0x02,0,0,0x56,
                              0x8B,0xB4,0x24,0x28,0x02,0,0};
  static const BYTE call1[]={0xE8,0x2F,0x0A,0,0};
  static const BYTE call2[]={0xE8,0x56,0x06,0,0};
  static const BYTE stop[]={0xA1,0x1C,0xBF,0x6B,0,0x56,0x50,
                              0x8B,0x08,0xFF,0x51,0x48,0x8B,0xF0,0x85,0xF6};
  static const BYTE cleanup[]={0x6A,0x01,0x6A,0,0xE8,0x23,0x05,0,0,
                                0x83,0xC4,0x08,0x5E,0xC3};
  static const BYTE callee[]={0x8B,0x44,0x24,0x08,0x53,0x56,
                               0x57,0x85,0xC0,0x74,0x4E};
  static const BYTE nops[14]={
      0x90,0x90,0x90,0x90,0x90,0x90,0x90,
      0x90,0x90,0x90,0x90,0x90,0x90,0x90};
  memset(&mz,0,sizeof(mz));
  mz.e_magic=IMAGE_DOS_SIGNATURE;
  mz.e_lfanew=0x80;
  memset(&nt,0,sizeof(nt));
  nt.Signature=IMAGE_NT_SIGNATURE;
  nt.FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
  nt.FileHeader.Characteristics=IMAGE_FILE_EXECUTABLE_IMAGE|IMAGE_FILE_32BIT_MACHINE;
  nt.FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER32);
  nt.FileHeader.NumberOfSections=1;
  nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
  nt.OptionalHeader.ImageBase=(DWORD)DUMMY_BASE;
  nt.OptionalHeader.SizeOfImage=DUMMY_SIZE;
  nt.OptionalHeader.NumberOfRvaAndSizes=IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
  nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress=
      IMPORT_DESC;
  nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size=
      sizeof(desc);
  memset(desc,0,sizeof(desc));
  desc[0].Name=IMPORT_NAME;
  desc[0].OriginalFirstThunk=ORIGINAL_THUNK;
  desc[0].FirstThunk=IAT_THUNK;
  memset(thunk,0,sizeof(thunk));
  thunk[0].u1.AddressOfData=FUNCTION_NAME;
  thunk[1].u1.AddressOfData=FUNCTION_NAME2;
  return put(p,0,&mz,sizeof(mz)) &&
         put(p,0x80,&nt,sizeof(nt)) &&
         put(p,IMPORT_DESC,desc,sizeof(desc)) &&
         put(p,IMPORT_NAME,dllname,sizeof(dllname)) &&
         put(p,ORIGINAL_THUNK,thunk,sizeof(thunk)) &&
         put(p,FUNCTION_NAME,symbol,sizeof(symbol)) &&
         put(p,FUNCTION_NAME2,symbol2,sizeof(symbol2)) &&
         put(p,0x1163E0,nocd,sizeof(nocd)) &&
         put(p,0x1159AC,call1,sizeof(call1)) &&
         put(p,0x115D85,call2,sizeof(call2)) &&
         put(p,0x103AF0,stop,sizeof(stop)) &&
         put(p,0x103B24,cleanup,sizeof(cleanup)) &&
         put(p,0x104050,callee,sizeof(callee)) &&
         put(p,0x103B32,nops,sizeof(nops));
}
#define CHECK(expr,msg) do {if(!(expr)){ \
  fprintf(stderr,"SUSPENDED-PE32: FAIL %s (Win32=%lu)\n", \
          msg,(unsigned long)GetLastError());goto cleanup; \
}} while(0)

int wmain(void) {
  wchar_t filename[MAX_PATH],cmd[MAX_PATH+20];
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  DWORD n;
  void *allocated=NULL;
  int ok=0;
  char dllname[10];
  BYTE patch[14];
  IMAGE_NT_HEADERS32 nt;
  IMAGE_THUNK_DATA32 extra;
  n=GetModuleFileNameW(NULL,filename,MAX_PATH);
  if(!n || n>=MAX_PATH)return 2;
  if(swprintf(cmd,MAX_PATH+20,L"\"%ls\" --suspended",filename)<0)return 2;
  memset(&si,0,sizeof(si));si.cb=sizeof(si);
  memset(&pi,0,sizeof(pi));
  CHECK(CreateProcessW(filename,cmd,NULL,NULL,FALSE,CREATE_SUSPENDED,
                       NULL,NULL,&si,&pi),"create suspended synthetic child");
  allocated=VirtualAllocEx(pi.hProcess,(void*)DUMMY_BASE,DUMMY_SIZE,
                            MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
  CHECK(allocated==(void*)DUMMY_BASE,"allocate dummy PE at fixed game image base");
  CHECK(fixture(pi.hProcess),"publish synthetic PE headers and signatures");
  /* Invalid header must be rejected, then restored for positive tests. */
  CHECK(readback(pi.hProcess,0x80,&nt,sizeof(nt)),"read original NT headers");
  nt.FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
  CHECK(put(pi.hProcess,0x80,&nt,sizeof(nt)),"tamper x86 machine field");
  CHECK(!SpiderUnified_AliasSuspendedDDraw(pi.hProcess),
        "reject non-i386 PE before any alias");
  nt.FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
  CHECK(put(pi.hProcess,0x80,&nt,sizeof(nt)),"restore valid PE32");
  nt.OptionalHeader.SizeOfImage=0x02000000u;
  CHECK(put(pi.hProcess,0x80,&nt,sizeof(nt)),"tamper mapped image size");
  CHECK(!SpiderUnified_AliasSuspendedDDraw(pi.hProcess),
        "reject non-retail SizeOfImage before any alias");
  nt.OptionalHeader.SizeOfImage=DUMMY_SIZE;
  CHECK(put(pi.hProcess,0x80,&nt,sizeof(nt)),"restore exact retail image size");
  /* A THIRD imported function would fail to load against fix.dll. Reject
   * any such descriptor without changing the original DLL name. */
  memset(&extra,0,sizeof(extra));extra.u1.AddressOfData=FUNCTION_NAME;
  CHECK(put(pi.hProcess,ORIGINAL_THUNK+8u,&extra,sizeof(extra)),
        "insert unexpected third DirectDraw import");
  CHECK(!SpiderUnified_AliasSuspendedDDraw(pi.hProcess),
        "refuse import descriptor with third method");
  CHECK(readback(pi.hProcess,IMPORT_NAME,dllname,sizeof(dllname)) &&
        strcmp(dllname,"DDRAW.dll")==0,
        "original import preserved on rejected second method");
  memset(&extra,0,sizeof(extra));
  CHECK(put(pi.hProcess,ORIGINAL_THUNK+8u,&extra,sizeof(extra)),
        "restore exactly two native DirectDraw methods");
  CHECK(SpiderUnified_AliasSuspendedDDraw(pi.hProcess),"apply alias");
  CHECK(readback(pi.hProcess,IMPORT_NAME,dllname,sizeof(dllname)),
        "read alias after patch");
  CHECK(memcmp(dllname,"fix.dll",8)==0 && dllname[8]==0 && dllname[9]==0,
        "alias is exactly 10 bytes with trailing zeros");
  CHECK(SpiderUnified_PatchSuspendedNoCD(pi.hProcess),
        "install exact-memory No-CD patch");
  CHECK(readback(pi.hProcess,0x1163E0,patch,3) &&
        patch[0]==0x31 && patch[1]==0xC0 && patch[2]==0xC3,
        "No-CD replacement persisted");
  CHECK(SpiderUnified_PatchSuspendedPrimaryNullStop(pi.hProcess),
        "install null primary Stop protection");
  CHECK(readback(pi.hProcess,0x103AF7,patch,5) && patch[0]==0xE9,
        "primary Stop branch persisted");
  CHECK(readback(pi.hProcess,0x103B32,patch,14) &&
        patch[0]==0x85 && patch[1]==0xC0,
        "secondary Stop code cave persisted");
  ok=1;
cleanup:
  if(pi.hProcess) {
    /* The synthetic child NEVER resumes; this also cleans up test failures. */
    TerminateProcess(pi.hProcess,0);
    WaitForSingleObject(pi.hProcess,10000);
  }
  if(pi.hThread)CloseHandle(pi.hThread);
  if(pi.hProcess)CloseHandle(pi.hProcess);
  if(ok)puts("PASS: native synthetic suspended PE32 alias/No-CD/NULL-Stop guard; negative signatures fail closed");
  return ok?0:1;
}
