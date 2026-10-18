#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

/* Exports required by SpideyPC.exe plus timer functions used by external wrappers.
   Resolve the real library from the Windows system directory, not by name. */
static HMODULE real_winmm;

/* Early, signature-guarded, in-memory No-CD patch.
   The PE image must be loaded at its expected image base.  Only minimal
   Win32 operations are performed under loader lock; no file I/O here. */
static volatile LONG early_patch_status = 0;
static void patch_early(void) {
    static const BYTE original[] = {0x81,0xEC,0x20,0x02,0x00,0x00,0x56,0x8B,0xB4,0x24,0x28,0x02,0x00,0x00};
    static const BYTE call1[] = {0xE8,0x2F,0x0A,0x00,0x00};
    static const BYTE call2[] = {0xE8,0x56,0x06,0x00,0x00};
    static const BYTE patch[] = {0x31,0xC0,0xC3}; /* xor eax,eax; ret (cdecl) */
    BYTE *base=(BYTE*)GetModuleHandleA(NULL), *target;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS32 *nt;
    DWORD old, ignored;
    if (!base) { early_patch_status=-1; return; }
    dos=(IMAGE_DOS_HEADER*)base;
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x1000) { early_patch_status=-2; return; }
    nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.ImageBase!=0x400000 || (uintptr_t)base!=0x400000 ||
        nt->OptionalHeader.SizeOfImage<=0x1163F0) { early_patch_status=-3; return; }
    target=base+0x1163E0;
    if (memcmp(target,original,sizeof(original)) ||
        memcmp(base+0x1159AC,call1,sizeof(call1)) ||
        memcmp(base+0x115D85,call2,sizeof(call2))) { early_patch_status=-4; return; }
    if (!VirtualProtect(target,sizeof(patch),PAGE_EXECUTE_READWRITE,&old)) { early_patch_status=-5; return; }
    memcpy(target,patch,sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(),target,sizeof(patch));
    if (!VirtualProtect(target,sizeof(patch),old,&ignored)) { early_patch_status=-6; return; }
    early_patch_status=1;
}

static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
static void log_proxy(const char *message) {
    char path[MAX_PATH]; char *slash; FILE *f;
    DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
    if (!n || n>=sizeof(path)) return;
    slash=strrchr(path,'\\');
    if (!slash || (size_t)(slash-path)+strlen("SpiderManFix.ini")+1>=sizeof(path)) return;
    strcpy(slash+1,"SpiderManFix.ini");
    if (GetPrivateProfileIntA("Debug","Log",0,path)!=1) return;
    if ((size_t)(slash-path)+strlen("SpiderManFix.log")+1>=sizeof(path)) return;
    strcpy(slash+1,"SpiderManFix.log");
    f=fopen(path,"a"); if (f) { fprintf(f,"%s\n",message); fclose(f); }
}
static BOOL CALLBACK load_real(PINIT_ONCE unused, PVOID arg, PVOID *ctx) {
    char path[MAX_PATH]; DWORD n;
    (void)unused; (void)arg; (void)ctx;
    n=GetSystemDirectoryA(path,sizeof(path));
    if (!n || n>=sizeof(path)-11) { log_proxy("ERROR: system directory path too long"); return TRUE; }
    strcat(path,"\\winmm.dll");
    real_winmm=LoadLibraryA(path);
    log_proxy(real_winmm ? "OK: real system winmm.dll loaded" : "ERROR: cannot load real system winmm.dll");
    return TRUE;
}
static FARPROC resolve(const char *name) {
    InitOnceExecuteOnce(&once,load_real,NULL,NULL);
    return real_winmm ? GetProcAddress(real_winmm,name) : NULL;
}
/* FARPROC and typed stdcall function pointers are both 32 bits on Win32.
   Copy the pointer representation rather than casting an unspecified FARPROC
   prototype to a function type with different parameters. */
#define RESOLVE_INTO(name, out) do { \
    FARPROC p_resolved = resolve(#name); \
    _Static_assert(sizeof(out) == sizeof(p_resolved), "Win32 function pointer size mismatch"); \
    memcpy(&(out), &p_resolved, sizeof(out)); \
} while (0)
MMRESULT WINAPI proxy_mmioAscend(HMMIO a,LPMMCKINFO b,UINT c) { __typeof__(&mmioAscend) f=NULL; RESOLVE_INTO(mmioAscend, f); return f?f(a,b,c):MMSYSERR_ERROR; }
LONG WINAPI proxy_mmioRead(HMMIO a,HPSTR b,LONG c) { __typeof__(&mmioRead) f=NULL; RESOLVE_INTO(mmioRead, f); return f?f(a,b,c):-1; }
MMRESULT WINAPI proxy_mmioClose(HMMIO a,UINT b) { __typeof__(&mmioClose) f=NULL; RESOLVE_INTO(mmioClose, f); return f?f(a,b):MMSYSERR_ERROR; }
FOURCC WINAPI proxy_mmioStringToFOURCCA(LPCSTR a,UINT b) { __typeof__(&mmioStringToFOURCCA) f=NULL; RESOLVE_INTO(mmioStringToFOURCCA, f); return f?f(a,b):0; }
HMMIO WINAPI proxy_mmioOpenA(LPSTR a,LPMMIOINFO b,DWORD c) { __typeof__(&mmioOpenA) f=NULL; RESOLVE_INTO(mmioOpenA, f); return f?f(a,b,c):NULL; }
MMRESULT WINAPI proxy_mmioDescend(HMMIO a,LPMMCKINFO b,const MMCKINFO *c,UINT d) { __typeof__(&mmioDescend) f=NULL; RESOLVE_INTO(mmioDescend, f); return f?f(a,b,c,d):MMSYSERR_ERROR; }
DWORD WINAPI proxy_timeGetTime(void) { __typeof__(&timeGetTime) f=NULL; RESOLVE_INTO(timeGetTime, f); return f?f():GetTickCount(); }
MMRESULT WINAPI proxy_timeEndPeriod(UINT a) { __typeof__(&timeEndPeriod) f=NULL; RESOLVE_INTO(timeEndPeriod, f); return f?f(a):MMSYSERR_ERROR; }
MMRESULT WINAPI proxy_timeBeginPeriod(UINT a) { __typeof__(&timeBeginPeriod) f=NULL; RESOLVE_INTO(timeBeginPeriod, f); return f?f(a):MMSYSERR_ERROR; }
MMRESULT WINAPI proxy_timeSetEvent(UINT a,UINT b,LPTIMECALLBACK c,DWORD_PTR d,UINT e) { __typeof__(&timeSetEvent) f=NULL; RESOLVE_INTO(timeSetEvent, f); return f?f(a,b,c,d,e):0; }
MMRESULT WINAPI proxy_timeKillEvent(UINT a) { __typeof__(&timeKillEvent) f=NULL; RESOLVE_INTO(timeKillEvent, f); return f?f(a):MMSYSERR_ERROR; }
MCIERROR WINAPI proxy_mciSendCommandA(MCIDEVICEID a,UINT b,DWORD_PTR c,DWORD_PTR d) { __typeof__(&mciSendCommandA) f=NULL; RESOLVE_INTO(mciSendCommandA, f); return f?f(a,b,c,d):MCIERR_DRIVER_INTERNAL; }
MMRESULT WINAPI proxy_timeGetDevCaps(LPTIMECAPS a,UINT b) { __typeof__(&timeGetDevCaps) f=NULL; RESOLVE_INTO(timeGetDevCaps, f); return f?f(a,b):MMSYSERR_ERROR; }
MMRESULT WINAPI proxy_timeGetSystemTime(LPMMTIME a, UINT b) {
    __typeof__(&timeGetSystemTime) f=NULL; RESOLVE_INTO(timeGetSystemTime, f);
    return f?f(a,b):MMSYSERR_ERROR;
}
static DWORD WINAPI init_fix(LPVOID ignored) {
    char path[MAX_PATH]; DWORD n; HMODULE mod;
    (void)ignored;
    {
        char status[128];
        snprintf(status,sizeof(status),"START: SpiderManFix 1.0.0 early No-CD patch status=%ld (1=applied)",(long)early_patch_status);
        log_proxy(status);
    }
    log_proxy("START: winmm proxy loaded; loading SpiderManFix.dll");
    n=GetModuleFileNameA(NULL,path,sizeof(path));
    if (!n || n>=sizeof(path)) return 0;
    char *slash=strrchr(path,'\\');
    if (!slash || (size_t)(slash-path)+strlen("SpiderManFix.dll")+1>=sizeof(path)) return 0;
    strcpy(slash+1,"SpiderManFix.dll");
    {
        char report[512];
        DWORD attributes=GetFileAttributesA(path);
        DWORD file_error=(attributes==INVALID_FILE_ATTRIBUTES) ? GetLastError() : ERROR_SUCCESS;
        snprintf(report,sizeof(report),"DLL-LOAD: target=%s; file_present=%s; file_probe_error=%lu",
                 path,(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))?"yes":"no",
                 (unsigned long)file_error);
        log_proxy(report);
    }
    mod=LoadLibraryA(path);
    {
        DWORD load_error=mod ? ERROR_SUCCESS : GetLastError(); /* MUST capture immediately */
        char report[512];
        if (mod) {
            log_proxy("OK: SpiderManFix.dll LoadLibrary succeeded");
        } else {
            const char *reason;
            switch (load_error) {
                case ERROR_MOD_NOT_FOUND: reason="missing DLL or one of its imports"; break;
                case ERROR_BAD_EXE_FORMAT: reason="invalid executable image / wrong architecture"; break;
                case ERROR_PROC_NOT_FOUND: reason="missing imported entry point"; break;
                case ERROR_DLL_INIT_FAILED: reason="DLL initialization failed"; break;
                case ERROR_ACCESS_DENIED: reason="access denied"; break;
                default: reason="see Win32 error code"; break;
            }
            snprintf(report,sizeof(report),"ERROR: SpiderManFix.dll LoadLibrary failed: winerr=%lu (%s)",
                     (unsigned long)load_error,reason);
            log_proxy(report);
        }
    }
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if (reason==DLL_PROCESS_ATTACH) {
        HANDLE thread;
        DisableThreadLibraryCalls(instance);
        patch_early();
        thread=CreateThread(NULL,0,init_fix,NULL,0,NULL);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}