/* Wine smoke host: verify the packaged x86 DLLs load together.
 * Not the Spider-Man game. Its intentionally unsupported EXE must never be patched.
 * No game assets or original EXE are required/distributed.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

static int local_winmm_is_loaded(void) {
    char dll[MAX_PATH],exe[MAX_PATH],*dslash,*eslash;
    HMODULE mod=GetModuleHandleA("winmm.dll");
    if(!mod || !GetModuleFileNameA(mod,dll,sizeof(dll)) ||
       !GetModuleFileNameA(NULL,exe,sizeof(exe))) {
        fputs("SMOKE ERROR: winmm or EXE path unavailable\n",stderr);
        return 0;
    }
    printf("SMOKE: winmm loaded from %s\n",dll);
    dslash=strrchr(dll,'\\'); eslash=strrchr(exe,'\\');
    if(!dslash || !eslash || lstrcmpiA(dslash+1,"winmm.dll")) {
        fputs("SMOKE ERROR: unexpected winmm module location\n",stderr);
        return 0;
    }
    *dslash=0; *eslash=0;
    if(lstrcmpiA(dll,exe)) {
        fputs("SMOKE ERROR: Wine did not load the game's local native winmm.dll\n",stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    unsigned n;
    DWORD now=timeGetTime(); /* Imported WinMM export must forward to Wine. */
    HMODULE fix=NULL;
    printf("SMOKE: WinMM timeGetTime=%lu\n",(unsigned long)now);
    if(!local_winmm_is_loaded()) return 2;
    for(n=0;n<80;n++) {
        fix=GetModuleHandleA("SpiderMan2k1Fix.dll");
        if(fix) break;
        Sleep(250);  /* Proxy loads the second DLL on a worker thread. */
    }
    if(!fix) {
        fputs("SMOKE ERROR: proxy did not load SpiderMan2k1Fix.dll\n",stderr);
        return 3;
    }
    Sleep(1500); /* Let the fix DLL worker finish its signature check and log. */
    puts("SMOKE OK: both native DLLs loaded under Wine (unsupported host untouched)");
    return 0;
}
