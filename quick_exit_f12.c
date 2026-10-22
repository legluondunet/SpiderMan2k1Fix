/* Optional F12 quick exit through the original game's native Quit byte.
 * A small polling thread is independent of DirectDraw Flip and DirectInput.
 * NO WM_CLOSE, DestroyWindow, TerminateProcess or ExitProcess: the game
 * must complete its own Bink cleanup and return through WinMain.
 * A failed Quit request leaves the game window visible for diagnosis.
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

/* Native game quit signal (verified in SHA-pinned SpideyPC.exe):
 * 0x004A238A sets ds:[0x0060CF88] to 1 for the game's own Quit action.
 * 0x00455E50 compares ds:[0x0060CF88] to zero and jumps directly to
 * 0x004561F2, the native engine cleanup and return from 0x00455C90.
 * WinMain (0x00515CBE onwards) then performs its final cleanup.
 *
 * WM_CLOSE from outside that state machine destroys HWND but the engine
 * continues to call DirectDraw Flip, as proven by Windows logs #439-#454.
 * The quick-exit shortcut must NEVER use WM_CLOSE. Set the original
 * game quit flag and let native cleanup finish on the game's thread.
 */
#define SPIDER_GAME_QUIT_FLAG ((volatile BYTE *)(uintptr_t)0x0060CF88u)
#define SPIDER_GAME_MODE ((const volatile DWORD *)(uintptr_t)0x0060CFA4u)
#define SPIDER_NATIVE_QUIT_SETTER ((const BYTE *)(uintptr_t)0x004A238Au)
#define SPIDER_NATIVE_QUIT_CHECK ((const BYTE *)(uintptr_t)0x00455E50u)
static const BYTE game_quit_set_signature[7]={0xC6,0x05,0x88,0xCF,0x60,0x00,0x01};
static const BYTE game_quit_check_signature[6]={0x38,0x1D,0x88,0xCF,0x60,0x00};

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
/* The movie runtime can be visible while SPIDER_INTRO_ACTIVE is zero.
 * The renderer's confirmed native video Blts are our authoritative
 * evidence for whether a Bink frame is still on screen. */
static volatile LONG intro_last_movie_blt_ms=0;
static volatile LONG intro_movie_blt_count=0;
static volatile LONG intro_abort_frame_count=0;

/* Original WndProc 0x00515368 handles WM_KEYDOWN/F12 by directly calling
 * PostMessageA(WM_CLOSE) here. If we leave this call in place, the original
 * WndProc destroys the window BEFORE our Bink-cleanup guard even runs.
 * 0x00515373: FF 15 70 B1 53 00 = CALL [PostMessageA IAT]
 * Stack at this site contains exactly four stdcall arguments.
 * Replace the six bytes with ADD ESP,16; NOP; NOP; NOP only when the
 * user expressly enables QuickExitF12. No WndProc subclass and no
 * changes to any other game keyboard or DirectInput action. */
#define SPIDER_NATIVE_F12_POST ((BYTE *)(uintptr_t)0x00515373u)
static const BYTE native_f12_original[6]={0xFF,0x15,0x70,0xB1,0x53,0x00};
static const BYTE native_f12_bypass[6]={0x83,0xC4,0x10,0x90,0x90,0x90};

static int native_f12_suppress_unsafe_window_close(void) {
  DWORD prot,ignored;
  if(memcmp(SPIDER_NATIVE_F12_POST,native_f12_original,6))return 0;
  if(!VirtualProtect(SPIDER_NATIVE_F12_POST,6,PAGE_EXECUTE_READWRITE,&prot))
    return 0;
  memcpy(SPIDER_NATIVE_F12_POST,native_f12_bypass,6);
  FlushInstructionCache(GetCurrentProcess(),SPIDER_NATIVE_F12_POST,6);
  VirtualProtect(SPIDER_NATIVE_F12_POST,6,prot,&ignored);
  return 1;
}

static void native_f12_restore(void) {
  DWORD prot,ignored;
  if(memcmp(SPIDER_NATIVE_F12_POST,native_f12_bypass,6))return;
  if(VirtualProtect(SPIDER_NATIVE_F12_POST,6,PAGE_EXECUTE_READWRITE,&prot)) {
    memcpy(SPIDER_NATIVE_F12_POST,native_f12_original,6);
    FlushInstructionCache(GetCurrentProcess(),SPIDER_NATIVE_F12_POST,6);
    VirtualProtect(SPIDER_NATIVE_F12_POST,6,prot,&ignored);
  }
}

/* Called only for a SUCCESSFUL decoded FMV Blt, after the original renderer
 * has processed it. Works for all four opening clips, independently of the
 * game's unreliable SPIDER_INTRO_ACTIVE flag. */
void SpiderQuickExitF12_ObserveMovieFrame(void) {
  InterlockedExchange(&intro_last_movie_blt_ms,(LONG)GetTickCount());
  InterlockedIncrement(&intro_movie_blt_count);
}

static int movie_frame_recent(DWORD now,DWORD window_ms) {
  LONG last=InterlockedCompareExchange(&intro_last_movie_blt_ms,0,0);
  return last!=0 && (DWORD)(now-(DWORD)last)<window_ms;
}
static const BYTE intro_call_1_original[5]={0xE8,0x7E,0xAD,0x09,0x00};
static const BYTE intro_call_2_original[5]={0xE8,0x3C,0xAD,0x09,0x00};

static BYTE __cdecl quick_exit_intro_tick(void) {
  /* This callback executes at the very site where the original caller
   * decides whether to continue Bink frames or invoke BinkClose. */
  if(InterlockedCompareExchange(&intro_abort_requested,0,0)==1) {
    InterlockedExchange(&intro_abort_seen,1);
    InterlockedExchange(&intro_abort_frame_count,
                        InterlockedCompareExchange(&intro_movie_blt_count,0,0));
    return 0; /* original game now jumps to 0x00470871 cleanup */
  }
  return SPIDER_INTRO_TICK();
}

static int patch_intro_call(BYTE *address,const BYTE expected[5],
                            const void *replacement) {
  BYTE patched[5];
  DWORD old_protect,ignored;
  int32_t displacement;
  intptr_t delta=(intptr_t)replacement-(intptr_t)(address+5);
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
      /* Never close HWND directly: WM_CLOSE/WM_DESTROY do not stop
       * the 2001 engine's own rendering or audio loops. */
      {
        DWORD now=GetTickCount();
        LONG blts=InterlockedCompareExchange(&intro_movie_blt_count,0,0);
        BOOL movie=movie_frame_recent(now,1500u)||
                   (*SPIDER_INTRO_ACTIVE!=0u);
        char info[240];
        InterlockedExchange(&intro_abort_requested,1);
        /* The game itself uses exactly this flag when its Quit action
         * is selected. This is the only exit request performed here. */
        *SPIDER_GAME_QUIT_FLAG=1u;
        snprintf(info,sizeof(info),
          "QUICK-EXIT-F12: game-native quit requested flag=1, movie=%d, blts=%ld, mode=%lu; NO WM_CLOSE",
          (int)movie,(long)blts,(unsigned long)*SPIDER_GAME_MODE);
        say(info);
        if(movie) {
          /* The next Bink tick(s) will return completion, causing the
           * original game thread to close each clip. After the four
           * opening movies the original quit-state check leads into
           * native cleanup without destroying the window prematurely. */
          unsigned attempts;
          for(attempts=0;attempts<600u;attempts++) {
            if(InterlockedCompareExchange(&intro_abort_seen,0,0)==1 &&
               *SPIDER_INTRO_ACTIVE==0u &&
               !movie_frame_recent(GetTickCount(),600u))
              break;
            Sleep(10);
          }
          if(attempts==600u) {
            say("QUICK-EXIT-F12: native movie cleanup unconfirmed after 6s; window kept intact, NO WM_CLOSE");
          } else {
            say("QUICK-EXIT-F12: native movie frames stopped; waiting for game's Quit cleanup, NO WM_CLOSE");
          }
        }
        /* Avoid duplicating exit requests. The external Windows process
         * watcher verifies whether the original cleanup really exits.
         * Never interpret a vanished HWND as proof of success. */
        return 0;
      }
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
    /* Optional feature is OFF: the original WM_KEYDOWN/F12 code can still
     * reach WM_CLOSE during intro movies. Suppress just that exact native
     * six-byte call, or F12 would remain partially active despite INI=0.
     * This safe one-site change leaves all other keyboard paths alone. */
    if(native_f12_suppress_unsafe_window_close())
      say("QUICK-EXIT-F12: disabled; original unsafe F12 WM_CLOSE also suppressed");
    else say("QUICK-EXIT-F12: WARNING disabled option, native F12 call signature unmatched");
    return;
  }
  /* Guard both original game quit instruction sites, the native F12
   * close and the exact Bink calls before installing any x86 patch. */
  if(memcmp(SPIDER_NATIVE_QUIT_SETTER,game_quit_set_signature,7)!=0 ||
     memcmp(SPIDER_NATIVE_QUIT_CHECK,game_quit_check_signature,6)!=0) {
    say("QUICK-EXIT-F12: DISABLED: native game Quit flag/check signature mismatch");
    return;
  }
  if(memcmp(SPIDER_NATIVE_F12_POST,native_f12_original,6)!=0 ||
     memcmp(SPIDER_INTRO_CALL_1,intro_call_1_original,5)!=0 ||
     memcmp(SPIDER_INTRO_CALL_2,intro_call_2_original,5)!=0) {
    say("QUICK-EXIT-F12: DISABLED: native F12/Bink call signatures unavailable");
    return;
  }
  if(!install_intro_exit_calls()) {
    say("QUICK-EXIT-F12: DISABLED: intro Bink tick hooks could not be installed");
    return;
  }
  if(!native_f12_suppress_unsafe_window_close()) {
    restore_intro_call(SPIDER_INTRO_CALL_1,intro_call_1_original);
    restore_intro_call(SPIDER_INTRO_CALL_2,intro_call_2_original);
    say("QUICK-EXIT-F12: DISABLED: native F12 window close could not be suppressed");
    return;
  }
  say("QUICK-EXIT-F12: original F12 WM_CLOSE suppressed; game-native Quit flag path active with Bink hooks");
  InterlockedExchange(&enabled,1);
  thread=CreateThread(NULL,0,quick_exit_f12_thread,NULL,0,NULL);
  if(!thread) {
    InterlockedExchange(&enabled,0);
    native_f12_restore();
    restore_intro_call(SPIDER_INTRO_CALL_1,intro_call_1_original);
    restore_intro_call(SPIDER_INTRO_CALL_2,intro_call_2_original);
    say("QUICK-EXIT-F12: ERROR thread creation failed; restored original F12");
    return;
  }
  CloseHandle(thread);
  say("QUICK-EXIT-F12: enabled; foreground F12 requests native game Quit (NEVER WM_CLOSE)");
}
