#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include "version_info.h"

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
/* First proxy message of each game process starts a fresh diagnostics file.
 * InitOnce serializes simultaneous proxy calls, avoiding a later truncation
 * that could erase an earlier startup line. All subsequent writes (including
 * SpiderMan2k1Fix.dll's) append to the SAME launch's log. */
static INIT_ONCE log_reset_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK reset_log_on_first_write(PINIT_ONCE init, PVOID arg, PVOID *ctx) {
    FILE *f;
    (void)init; (void)ctx;
    f=fopen((const char*)arg,"w");
    if(f) fclose(f);
    return TRUE;
}
static void log_proxy(const char *message) {
    char path[MAX_PATH]; char *slash; FILE *f;
    DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
    if (!n || n>=sizeof(path)) return;
    slash=strrchr(path,'\\');
    if (!slash || (size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1>=sizeof(path)) return;
    strcpy(slash+1,"SpiderMan2k1Fix.ini");
    if (GetPrivateProfileIntA("Debug","Log",0,path)!=1) return;
    if ((size_t)(slash-path)+strlen("SpiderMan2k1Fix.log")+1>=sizeof(path)) return;
    strcpy(slash+1,"SpiderMan2k1Fix.log");
    /* Do this before the first line (DPI-BOOT / START), not in the main DLL,
     * which loads later and would erase the proxy's earliest diagnostics. */
    InitOnceExecuteOnce(&log_reset_once,reset_log_on_first_write,path,NULL);
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
/* Additional typed WinMM forwards for native Windows game middleware. */
UINT WINAPI proxy_waveOutGetNumDevs(void) {
    __typeof__(&waveOutGetNumDevs) f=NULL; RESOLVE_INTO(waveOutGetNumDevs, f);
    return f ? f() : 0;
}
MMRESULT WINAPI proxy_waveOutGetDevCapsA(UINT_PTR a, LPWAVEOUTCAPSA b, UINT c) {
    __typeof__(&waveOutGetDevCapsA) f=NULL; RESOLVE_INTO(waveOutGetDevCapsA, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutGetErrorTextA(MMRESULT a, LPSTR b, UINT c) {
    __typeof__(&waveOutGetErrorTextA) f=NULL; RESOLVE_INTO(waveOutGetErrorTextA, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutOpen(LPHWAVEOUT a, UINT_PTR b, LPCWAVEFORMATEX c, DWORD_PTR d, DWORD_PTR e, DWORD flags) {
    __typeof__(&waveOutOpen) f=NULL; RESOLVE_INTO(waveOutOpen, f);
    return f ? f(a,b,c,d,e,flags) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutClose(HWAVEOUT a) {
    __typeof__(&waveOutClose) f=NULL; RESOLVE_INTO(waveOutClose, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutPrepareHeader(HWAVEOUT a, LPWAVEHDR b, UINT c) {
    __typeof__(&waveOutPrepareHeader) f=NULL; RESOLVE_INTO(waveOutPrepareHeader, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutUnprepareHeader(HWAVEOUT a, LPWAVEHDR b, UINT c) {
    __typeof__(&waveOutUnprepareHeader) f=NULL; RESOLVE_INTO(waveOutUnprepareHeader, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutWrite(HWAVEOUT a, LPWAVEHDR b, UINT c) {
    __typeof__(&waveOutWrite) f=NULL; RESOLVE_INTO(waveOutWrite, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutPause(HWAVEOUT a) {
    __typeof__(&waveOutPause) f=NULL; RESOLVE_INTO(waveOutPause, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutRestart(HWAVEOUT a) {
    __typeof__(&waveOutRestart) f=NULL; RESOLVE_INTO(waveOutRestart, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutReset(HWAVEOUT a) {
    __typeof__(&waveOutReset) f=NULL; RESOLVE_INTO(waveOutReset, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutBreakLoop(HWAVEOUT a) {
    __typeof__(&waveOutBreakLoop) f=NULL; RESOLVE_INTO(waveOutBreakLoop, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutGetPosition(HWAVEOUT a, LPMMTIME b, UINT c) {
    __typeof__(&waveOutGetPosition) f=NULL; RESOLVE_INTO(waveOutGetPosition, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutGetPitch(HWAVEOUT a, LPDWORD b) {
    __typeof__(&waveOutGetPitch) f=NULL; RESOLVE_INTO(waveOutGetPitch, f);
    return f ? f(a,b) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutSetPitch(HWAVEOUT a, DWORD b) {
    __typeof__(&waveOutSetPitch) f=NULL; RESOLVE_INTO(waveOutSetPitch, f);
    return f ? f(a,b) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutGetPlaybackRate(HWAVEOUT a, LPDWORD b) {
    __typeof__(&waveOutGetPlaybackRate) f=NULL; RESOLVE_INTO(waveOutGetPlaybackRate, f);
    return f ? f(a,b) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutSetPlaybackRate(HWAVEOUT a, DWORD b) {
    __typeof__(&waveOutSetPlaybackRate) f=NULL; RESOLVE_INTO(waveOutSetPlaybackRate, f);
    return f ? f(a,b) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutGetVolume(HWAVEOUT a, LPDWORD b) {
    __typeof__(&waveOutGetVolume) f=NULL; RESOLVE_INTO(waveOutGetVolume, f);
    return f ? f(a,b) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutSetVolume(HWAVEOUT a, DWORD b) {
    __typeof__(&waveOutSetVolume) f=NULL; RESOLVE_INTO(waveOutSetVolume, f);
    return f ? f(a,b) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutGetID(HWAVEOUT a, LPUINT b) {
    __typeof__(&waveOutGetID) f=NULL; RESOLVE_INTO(waveOutGetID, f);
    return f ? f(a,b) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveOutMessage(HWAVEOUT a, UINT b, DWORD_PTR c, DWORD_PTR d) {
    __typeof__(&waveOutMessage) f=NULL; RESOLVE_INTO(waveOutMessage, f);
    return f ? f(a,b,c,d) : MMSYSERR_ERROR;
}
UINT WINAPI proxy_waveInGetNumDevs(void) {
    __typeof__(&waveInGetNumDevs) f=NULL; RESOLVE_INTO(waveInGetNumDevs, f);
    return f ? f() : 0;
}
MMRESULT WINAPI proxy_waveInGetDevCapsA(UINT_PTR a, LPWAVEINCAPSA b, UINT c) {
    __typeof__(&waveInGetDevCapsA) f=NULL; RESOLVE_INTO(waveInGetDevCapsA, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInGetErrorTextA(MMRESULT a, LPSTR b, UINT c) {
    __typeof__(&waveInGetErrorTextA) f=NULL; RESOLVE_INTO(waveInGetErrorTextA, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInOpen(LPHWAVEIN a, UINT_PTR b, LPCWAVEFORMATEX c, DWORD_PTR d, DWORD_PTR e, DWORD flags) {
    __typeof__(&waveInOpen) f=NULL; RESOLVE_INTO(waveInOpen, f);
    return f ? f(a,b,c,d,e,flags) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInClose(HWAVEIN a) {
    __typeof__(&waveInClose) f=NULL; RESOLVE_INTO(waveInClose, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInPrepareHeader(HWAVEIN a, LPWAVEHDR b, UINT c) {
    __typeof__(&waveInPrepareHeader) f=NULL; RESOLVE_INTO(waveInPrepareHeader, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInUnprepareHeader(HWAVEIN a, LPWAVEHDR b, UINT c) {
    __typeof__(&waveInUnprepareHeader) f=NULL; RESOLVE_INTO(waveInUnprepareHeader, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInAddBuffer(HWAVEIN a, LPWAVEHDR b, UINT c) {
    __typeof__(&waveInAddBuffer) f=NULL; RESOLVE_INTO(waveInAddBuffer, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInStart(HWAVEIN a) {
    __typeof__(&waveInStart) f=NULL; RESOLVE_INTO(waveInStart, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInStop(HWAVEIN a) {
    __typeof__(&waveInStop) f=NULL; RESOLVE_INTO(waveInStop, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInReset(HWAVEIN a) {
    __typeof__(&waveInReset) f=NULL; RESOLVE_INTO(waveInReset, f);
    return f ? f(a) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInGetPosition(HWAVEIN a, LPMMTIME b, UINT c) {
    __typeof__(&waveInGetPosition) f=NULL; RESOLVE_INTO(waveInGetPosition, f);
    return f ? f(a,b,c) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInGetID(HWAVEIN a, LPUINT b) {
    __typeof__(&waveInGetID) f=NULL; RESOLVE_INTO(waveInGetID, f);
    return f ? f(a,b) : MMSYSERR_ERROR;
}
MMRESULT WINAPI proxy_waveInMessage(HWAVEIN a, UINT b, DWORD_PTR c, DWORD_PTR d) {
    __typeof__(&waveInMessage) f=NULL; RESOLVE_INTO(waveInMessage, f);
    return f ? f(a,b,c,d) : MMSYSERR_ERROR;
}
/* The game is DPI-unaware by default. DirectDraw uses physical pixels,
 * while an unaware top-level HWND sees a DPI-virtualized monitor rectangle.
 * Request system DPI awareness before the game's window is created, so
 * Win32 window geometry and DirectDraw surface sizes use the same pixels.
 * Keep user32 dynamically resolved for compatibility with older systems. */
static void enable_native_window_pixels(void) {
    typedef BOOL (WINAPI *SetProcessDPIAwareFn)(void);
    HMODULE user32=GetModuleHandleA("user32.dll");
    SetProcessDPIAwareFn set_aware=NULL;
    BOOL changed=FALSE;
    DWORD error=ERROR_PROC_NOT_FOUND;
    char line[160];
    if (!user32) user32=LoadLibraryA("user32.dll");
    if (user32)
        set_aware=(SetProcessDPIAwareFn)(void*)GetProcAddress(user32,"SetProcessDPIAware");
    if(set_aware) {
        changed=set_aware();
        error=changed ? ERROR_SUCCESS : GetLastError();
    }
    snprintf(line,sizeof(line),
      "DPI-BOOT: SetProcessDPIAware result=%s Win32=%lu (system-aware native window pixels)",
      changed?"OK":"NOT_CHANGED",(unsigned long)error);
    log_proxy(line);
}
static DWORD WINAPI init_fix(LPVOID ignored) {
    char path[MAX_PATH]; DWORD n; HMODULE mod;
    (void)ignored;
    enable_native_window_pixels();
    {
        char status[128];
        snprintf(status,sizeof(status),"START: " SPIDERMAN2K1FIX_VERSION_LABEL " early No-CD patch status=%ld (1=applied)",(long)early_patch_status);
        log_proxy(status);
    }
    log_proxy("START: winmm proxy loaded; loading SpiderMan2k1Fix.dll");
    n=GetModuleFileNameA(NULL,path,sizeof(path));
    if (!n || n>=sizeof(path)) return 0;
    char *slash=strrchr(path,'\\');
    if (!slash || (size_t)(slash-path)+strlen("SpiderMan2k1Fix.dll")+1>=sizeof(path)) return 0;
    strcpy(slash+1,"SpiderMan2k1Fix.dll");
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
            log_proxy("OK: SpiderMan2k1Fix.dll LoadLibrary succeeded");
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
            snprintf(report,sizeof(report),"ERROR: SpiderMan2k1Fix.dll LoadLibrary failed: winerr=%lu (%s)",
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
