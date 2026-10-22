/* Optional F12 quick exit using the game's normal WM_CLOSE path.
 * A small polling thread is used instead of a DirectDraw Flip callback:
 * comics and opening stills can stop flipping for an arbitrary duration,
 * and DirectInput gameplay may not dispatch WM_KEYDOWN.
 * No TerminateProcess, ExitProcess, forced WM_QUIT, keyboard hooks, or
 * subclassing of the game's WndProc. No exit is triggered in the background.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "quick_exit_f12.h"

static volatile LONG enabled=0;
static volatile LONG closing=0;
static PVOID volatile game_hwnd=NULL;
static void (*quick_exit_log)(const char *)=NULL;

/* The supported 2001 executable has one native movie playback routine:
 * 0x0047081D and 0x0047085F both CALL 0x0050B5A0 (Bink frame tick).
 * The owning game routine at 0x00470871 calls the native 0x0050B790
 * movie cleanup AFTER the tick returns false. It clears its active flag
 * at 0x006151F8 only AFTER BinkClose and other native cleanup calls.
 *
 * A WM_CLOSE issued WHILE that synchronous intro loop is running closes
 * the window but leaves Bink audio/video rendering in the process.
 * By answering "finished" at a native movie tick in the game's thread,
 * we let the ORIGINAL movie owner run its own cleanup; only THEN post
 * WM_CLOSE. Never stop a Bink thread, free its handle or terminate the
 * process from our F12 polling thread.
 *
 * Both CALL bytes are guarded by the executable SHA gate in initialize()
 * and exact instruction signatures before patching. With any mismatch,
 * the opt-in feature stays disabled rather than risking incomplete exit.
 */
#define SPIDER_INTRO_CALL_1 ((BYTE *)(uintptr_t)0x0047081Du)
#define SPIDER_INTRO_CALL_2 ((BYTE *)(uintptr_t)0x0047085Fu)
#define SPIDER_INTRO_ACTIVE ((const volatile DWORD *)(uintptr_t)0x006151F8u)
#define SPIDER_INTRO_TICK ((BYTE (__cdecl *)(void))(uintptr_t)0x0050B5A0u)

static volatile LONG intro_abort_requested=0;
static volatile LONG intro_abort_seen=0;
static const BYTE intro_call_1_original[5]={0xE8,0x7E,0xAD,0x09,0x00};
static const BYTE intro_call_2_original[5]={0xE8,0x3C,0xAD,0x09,0x00};

static BYTE __cdecl quick_exit_intro_tick(void) {
  /* This callback executes at the very site where the original caller
   * decides whether to continue Bink frames or invoke BinkClose. */
  if(InterlockedCompareExchange(&intro_abort_requested,0,0)==1) {
    InterlockedExchange(&intro_abort_seen,1);
    return 0; /* original game now jumps to 0x00470871 cleanup */
  }
  return SPIDER_INTRO_TICK();
}

static int patch_intro_call(BYTE *address,const BYTE expected[5],
                            const void *replacement) {
  BYTE patched[5];
  DWORD old_protect,ignored;
  int32_t displacement;
  intptr_t delta=(const BYTE *)replacement-(address+5);
  if(memcmp(address,expected,5)!=0 ||
     delta<(intptr_t)INT32_MIN || delta>(intptr_t)INT32_MAX)return 0;
  displacement=(int32_t)delta;
  patched[0]=0xE8;
  memcpy(patched+1,&displacement,4);
  if(!VirtualProtect(address,5,PAGE_EXECUTE_READWRITE,&old_protect))return 0;
  memcpy(address,patched,5);
  FlushInstructionCache(GetCurrentProcess(),address,5);
  VirtualProtect(address,5,old_protect,&ignored);
  return 1;
}

static void restore_intro_call(BYTE *address,const BYTE original[5]) {
  DWORD old_protect,ignored;
  if(VirtualProtect(address,5,PAGE_EXECUTE_READWRITE,&old_protect)) {
    memcpy(address,original,5);
    FlushInstructionCache(GetCurrentProcess(),address,5);
    VirtualProtect(address,5,old_protect,&ignored);
  }
}

static int install_intro_exit_calls(void) {
  /* Verify BOTH before modifying EITHER. Guard against partial install. */
  if(memcmp(SPIDER_INTRO_CALL_1,intro_call_1_original,5)!=0 ||
     memcmp(SPIDER_INTRO_CALL_2,intro_call_2_original,5)!=0)return 0;
  if(!patch_intro_call(SPIDER_INTRO_CALL_1,intro_call_1_original,
                       (const void *)quick_exit_intro_tick))return 0;
  if(!patch_intro_call(SPIDER_INTRO_CALL_2,intro_call_2_original,
                       (const void *)quick_exit_intro_tick)) {
    restore_intro_call(SPIDER_INTRO_CALL_1,intro_call_1_original);
    return 0;
  }
  return 1;
}

static void say(const char *msg) {
  if(quick_exit_log)quick_exit_log(msg);
}

int SpiderQuickExitF12_Enabled(void) {
  return InterlockedCompareExchange(&enabled,0,0)==1;
}

void SpiderQuickExitF12_AttachWindow(HWND hwnd) {
  InterlockedExchangePointer(&game_hwnd,(PVOID)hwnd);
}

static DWORD WINAPI quick_exit_f12_thread(LPVOID unused) {
  BOOL previous_down=TRUE; /* require a released F12 after startup */
  (void)unused;
  for(;;) {
    HWND hwnd=(HWND)InterlockedCompareExchangePointer(&game_hwnd,NULL,NULL);
    BOOL focused=hwnd && IsWindow(hwnd) && GetForegroundWindow()==hwnd;
    BOOL down=(GetAsyncKeyState(VK_F12)&0x8000)!=0;
    if(!focused) {
      /* Never trigger from Alt+Tab or on regaining focus while held. */
      previous_down=TRUE;
    } else if(!down) {
      previous_down=FALSE;
    } else if(!previous_down &&
              InterlockedCompareExchange(&closing,1,0)==0) {
      /* The intro Bink loop may keep working even AFTER DestroyWindow.
       * Ask the native frame loop to end first. Our game-thread hook
       * returns "movie finished" and the game's own code calls BinkClose
       * and clears SPIDER_INTRO_ACTIVE before we send WM_CLOSE. */
      if(*SPIDER_INTRO_ACTIVE!=0u) {
        unsigned attempts;
        char info[180];
        say("QUICK-EXIT-F12: opening movie active; requesting native Bink completion BEFORE WM_CLOSE");
        InterlockedExchange(&intro_abort_requested,1);
        for(attempts=0;attempts<300u;attempts++) {
          if(InterlockedCompareExchange(&intro_abort_seen,0,0)==1 &&
             *SPIDER_INTRO_ACTIVE==0u)break;
          Sleep(10);
        }
        if(attempts==300u) {
          snprintf(info,sizeof(info),
            "QUICK-EXIT-F12: WARNING intro completion not confirmed (active=%lu seen=%ld); WM_CLOSE NOT posted",
            (unsigned long)*SPIDER_INTRO_ACTIVE,
            (long)InterlockedCompareExchange(&intro_abort_seen,0,0));
          say(info);
          /* Keep the cancel request latched for the native game thread.
           * Do not destroy the window while Bink is still rendering. */
          return 0;
        }
        say("QUICK-EXIT-F12: native intro completed; Bink released by original game");
      }
      /* WM_CLOSE is requested only AFTER the movie owner's cleanup. The
       * game's window/message teardown remains authoritative. */
      if(PostMessageA(hwnd,WM_CLOSE,0,0)) {
        unsigned attempts;
        say("QUICK-EXIT-F12: F12 pressed in foreground; WM_CLOSE posted (normal shutdown)");
        /* Read-only check: did the game's own WndProc destroy the window?
         * If not, report it instead of killing the process or bypassing
         * a possible native prompt. Ending normally is the game's job. */
        for(attempts=0;attempts<100u;attempts++) {
          if(!IsWindow(hwnd)) {
            say("QUICK-EXIT-F12: game window destroyed; verify process exit independently");
            return 0;
          }
          Sleep(50);
        }
        say("QUICK-EXIT-F12: WARNING WM_CLOSE posted, but game window still exists after 5s; no forced termination");
        return 0; /* one request only, no repeated exit messages */
      }
      InterlockedExchange(&closing,0);
      say("QUICK-EXIT-F12: ERROR could not post WM_CLOSE; game left running");
      previous_down=TRUE;
    }
    Sleep(25);
  }
}

void SpiderQuickExitF12_Init(void (*log_fn)(const char *)) {
  char exe[MAX_PATH],ini[MAX_PATH],value[24],*slash;
  DWORD len;
  HANDLE thread;
  quick_exit_log=log_fn;
  len=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(!len || len>=sizeof(exe) || !(slash=strrchr(exe,'\\'))) {
    say("QUICK-EXIT-F12: disabled (cannot locate game INI)");
    return;
  }
  if((size_t)(slash-exe)+strlen("SpiderMan2k1Fix.ini")+2>sizeof(ini)) {
    say("QUICK-EXIT-F12: disabled (INI path too long)");
    return;
  }
  memcpy(ini,exe,(size_t)(slash-exe)+1);
  strcpy(ini+(slash-exe)+1,"SpiderMan2k1Fix.ini");
  /* Strict value: only the literal 1 enables this optional exit path.
   * Missing [System] or QuickExitF12, including older INIs, means off. */
  GetPrivateProfileStringA("System","QuickExitF12","0",value,sizeof(value),ini);
  if(strcmp(value,"1")!=0) {
    say("QUICK-EXIT-F12: disabled by default ([System] QuickExitF12=0)");
    return;
  }
  if(!install_intro_exit_calls()) {
    say("QUICK-EXIT-F12: DISABLED: supported intro Bink CALL signatures unavailable");
    return;
  }
  say("QUICK-EXIT-F12: native intro completion hooks installed (two signature-guarded CALLs)");
  InterlockedExchange(&enabled,1);
  thread=CreateThread(NULL,0,quick_exit_f12_thread,NULL,0,NULL);
  if(!thread) {
    InterlockedExchange(&enabled,0);
    say("QUICK-EXIT-F12: ERROR thread creation failed; feature disabled");
    return;
  }
  CloseHandle(thread);
  say("QUICK-EXIT-F12: enabled; foreground F12 will post WM_CLOSE once");
}
