/* F12 experimental native Shell_MainMenu Quit activation.
 * Verified against SpideyPC.exe SHA-256:
 * 0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
 *
 * Prior fixes hooked 0x004A1A26 / 0x004A202F, part of a secondary menu
 * path that NEVER runs in the idle title menu (user Windows logs #464/#468).
 * The REAL Shell_MainMenu at 0x00493990 has a per-frame tick CALL at
 * 0x00493DD6 -> 0x00505720. Its normal item-activation branch begins
 * at 0x004941D5 and reads the selected slot at [ESP+0x40].
 *
 * Original native menu table at 0x00552AA8:
 *   index 3 -> text "quit" -> action 0x12 (18).
 *
 * On F12, use a short-lived one-shot request. In the native Shell_MainMenu
 * game-thread frame CALL wrapper, finish the ORIGINAL tick, set the normal
 * selected item local to 3, and return to the game's own selection/cleanup
 * branch 0x004941D5. This is identical in effect to selecting "quit" on
 * the real menu. NEVER post WM_CLOSE or call native resource destructors.
 *
 * Opening cinematics now use the game's two native Bink frame-tick
 * CALL sites. An opt-in F12 request makes those ticks return "finished",
 * so the ORIGINAL 0x470871 BinkClose path runs in the video-owning
 * game thread. All four sequential movie calls take this same path.
 * On entry to the REAL main menu, the already Windows-validated Quit
 * item is selected on the normal menu thread, before the first menu
 * frame is presented. No WM_CLOSE, no process kill, no direct BinkClose.
 *
 * Comic screens / 3D remain unsupported. Exact code bytes MUST match
 * before any hook is installed.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "quick_exit_f12.h"

#define SPIDER_NATIVE_F12_POST ((BYTE *)(uintptr_t)0x00515373u)
#define SPIDER_SHELL_MENU_TICK ((BYTE *)(uintptr_t)0x00493DD6u)
#define SPIDER_SHELL_MENU_QUIT_RETURN ((const BYTE *)(uintptr_t)0x004941D5u)
#define SPIDER_SHELL_MENU_ITEMS ((const BYTE *)(uintptr_t)0x00552AA8u)
#define SPIDER_NATIVE_MENU_TICK ((void (__cdecl *)(void))(uintptr_t)0x00505720u)
#define SPIDER_INTRO_TICK_CALL_1 ((BYTE *)(uintptr_t)0x0047081Du)
#define SPIDER_INTRO_TICK_CALL_2 ((BYTE *)(uintptr_t)0x0047085Fu)
#define SPIDER_NATIVE_BINK_FRAME_TICK ((BYTE (__cdecl *)(void))(uintptr_t)0x0050B5A0u)
#define SPIDER_GAME_BINK_ACTIVE ((const volatile DWORD *)(uintptr_t)0x006151F8u)

static const BYTE native_f12_original[6]={0xFF,0x15,0x70,0xB1,0x53,0x00};
static const BYTE native_f12_bypass[6]={0x83,0xC4,0x10,0x90,0x90,0x90};
static const BYTE shell_menu_tick_original[5]={0xE8,0x45,0x19,0x07,0x00};
static const BYTE shell_menu_quit_return_signature[5]={0x6A,0x00,0x68,0x00,0x20};
static const BYTE intro_tick_call_original_1[5]={0xE8,0x7E,0xAD,0x09,0x00};
static const BYTE intro_tick_call_original_2[5]={0xE8,0x3C,0xAD,0x09,0x00};

static volatile LONG enabled=0;
static volatile LONG pending=0;
static volatile LONG delivered=0;
static volatile LONG menu_frame_count=0;
static volatile LONG last_menu_frame_ms=0;
static volatile LONG last_movie_frame_ms=0;
/* The request persists across the four native movie calls.
 * Only the already verified real menu Quit routine consumes it. */
static volatile LONG intro_quit_pending=0;
static volatile LONG intro_tick_shortcircuited=0;
static PVOID volatile game_hwnd=NULL;
static void (*logger)(const char *)=NULL;

static void say(const char *msg) {
  if(logger)logger(msg);
}

int SpiderQuickExitF12_Enabled(void) {
  return InterlockedCompareExchange(&enabled,0,0)==1;
}

void SpiderQuickExitF12_AttachWindow(HWND hwnd) {
  InterlockedExchangePointer(&game_hwnd,(PVOID)hwnd);
}

void SpiderQuickExitF12_ObserveMovieFrame(void) {
  InterlockedExchange(&last_movie_frame_ms,(LONG)GetTickCount());
}

/* Native BinkFrame tick wrapper, called by the game thread at the two
 * original 0x470750 video-loop sites. False makes the owning caller
 * branch to 0x470871 and invoke original BinkClose, audio reset and
 * movie-active clearing. Never call BinkClose or access Bink handles in
 * this hook, and never destroy HWND while the video loop is alive. */
static BYTE __cdecl spider_f12_native_intro_tick(void) {
  if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1) {
    if(InterlockedCompareExchange(&intro_tick_shortcircuited,1,0)==0)
      say("QUICK-EXIT-F12: native Bink tick returned finished; original game video cleanup will run");
    return 0;
  }
  return SPIDER_NATIVE_BINK_FRAME_TICK();
}

/* Returns nonzero ONLY when a recent, explicitly requested F12 action
 * may be delivered inside the ACTUAL Shell_MainMenu frame loop. This
 * never mutates engine state or executes resource cleanup itself.
 * Executed on the original game thread after the game's normal tick. */
int __cdecl SpiderQuickExitF12_ConsumeRealMenuRequest(void) {
  LONG n=InterlockedIncrement(&menu_frame_count);
  InterlockedExchange(&last_menu_frame_ms,(LONG)GetTickCount());
  if(n==1)
    say("QUICK-EXIT-F12: REAL Shell_MainMenu frame callback observed (0x493DD6)");
  /* The game's main sequence calls Shell_MainMenu only after finishing
   * its four opening 0x470750 movies. Transfer a latched intro F12 to
   * the PREVIOUSLY USER-VALIDATED native Quit item here. Never issue a
   * WndProc close or write a game global quit flag. */
  if(InterlockedCompareExchange(&intro_quit_pending,0,1)==1) {
    InterlockedExchange(&delivered,1);
    say("QUICK-EXIT-F12: intro Bink cleanup completed; native Shell_MainMenu Quit activated: slot=3 action=18");
    return 1;
  }
  if(InterlockedCompareExchange(&pending,0,1)==1) {
    InterlockedExchange(&delivered,1);
    say("QUICK-EXIT-F12: native Shell_MainMenu Quit activated: slot=3 action=18, native cleanup branch=0x4941D5");
    return 1;
  }
  return 0;
}

/* x86 stack at entry to the patched game CALL:
 *   [esp+0x00] = original return address 0x00493DDB
 *   [esp+0x44] = caller's native menu selected-index local
 * pushfl + pushal each preserve the caller's flags and registers and
 * relocate those to [esp+0x24] and [esp+0x68] respectively.
 * On F12, redirect just the native RETURN to the existing item-selection
 * branch. The branch releases the real menu's objects and returns the
 * original action ID via the native table. No off-thread cleanup.
 *
 * GCC x86 MinGW basic asm syntax, function is naked (no prologue).
 * Function intentionally has no C statements or compiler-generated
 * epilogue; the only end instruction is its own RET. */
__attribute__((naked,used)) static void spider_real_menu_tick_hook(void) {
  __asm__ __volatile__(
    "pushfl\n\t"
    "pushal\n\t"
    "movl $0x00505720, %eax\n\t"
    "call *%eax\n\t"
    "call _SpiderQuickExitF12_ConsumeRealMenuRequest\n\t"
    "testl %eax, %eax\n\t"
    "jz 1f\n\t"
    "movl $3, 0x68(%esp)\n\t"
    "movl $0x004941D5, 0x24(%esp)\n\t"
    "1:\n\t"
    "popal\n\t"
    "popfl\n\t"
    "ret\n\t"
  );
}

static int write_code(BYTE *address,const BYTE *expected,const BYTE *next,
                      SIZE_T count) {
  DWORD old_protect,ignored;
  if(memcmp(address,expected,count)!=0)return 0;
  if(!VirtualProtect(address,count,PAGE_EXECUTE_READWRITE,&old_protect))
    return 0;
  memcpy(address,next,count);
  FlushInstructionCache(GetCurrentProcess(),address,count);
  VirtualProtect(address,count,old_protect,&ignored);
  return 1;
}

static void restore_original_menu_tick(void) {
  DWORD old_protect,ignored;
  if(!VirtualProtect(SPIDER_SHELL_MENU_TICK,5,
                     PAGE_EXECUTE_READWRITE,&old_protect))return;
  memcpy(SPIDER_SHELL_MENU_TICK,shell_menu_tick_original,5);
  FlushInstructionCache(GetCurrentProcess(),SPIDER_SHELL_MENU_TICK,5);
  VirtualProtect(SPIDER_SHELL_MENU_TICK,5,old_protect,&ignored);
}

static int install_real_menu_hook(void) {
  intptr_t distance=(intptr_t)(void *)spider_real_menu_tick_hook-
                    (intptr_t)(SPIDER_SHELL_MENU_TICK+5);
  int32_t rel;
  BYTE patched[5]={0xE8,0,0,0,0};
  if(distance<(intptr_t)INT32_MIN || distance>(intptr_t)INT32_MAX)return 0;
  rel=(int32_t)distance;
  memcpy(patched+1,&rel,4);
  return write_code(SPIDER_SHELL_MENU_TICK,shell_menu_tick_original,patched,5);
}

static int patch_intro_tick_call(BYTE *site,const BYTE original[5]) {
  intptr_t delta=(intptr_t)(void *)spider_f12_native_intro_tick-
                 (intptr_t)(site+5);
  int32_t rel;
  BYTE call_bytes[5]={0xE8,0,0,0,0};
  if(delta<(intptr_t)INT32_MIN || delta>(intptr_t)INT32_MAX)return 0;
  rel=(int32_t)delta;
  memcpy(call_bytes+1,&rel,4);
  return write_code(site,original,call_bytes,5);
}

static void restore_intro_tick_call(BYTE *site,const BYTE original[5]) {
  DWORD old_protect,ignored;
  if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old_protect))return;
  memcpy(site,original,5);
  FlushInstructionCache(GetCurrentProcess(),site,5);
  VirtualProtect(site,5,old_protect,&ignored);
}

static int install_intro_tick_hooks(void) {
  if(memcmp(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1,5)!=0 ||
     memcmp(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2,5)!=0)
    return 0;
  if(!patch_intro_tick_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1))
    return 0;
  if(!patch_intro_tick_call(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2)) {
    restore_intro_tick_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1);
    return 0;
  }
  return 1;
}

static void restore_intro_tick_hooks(void) {
  restore_intro_tick_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1);
  restore_intro_tick_call(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2);
}

static int suppress_unsafe_native_f12(void) {
  return write_code(SPIDER_NATIVE_F12_POST,native_f12_original,
                    native_f12_bypass,sizeof(native_f12_original));
}

static DWORD WINAPI f12_input_thread(LPVOID ignored) {
  BOOL previously_down=TRUE;
  (void)ignored;
  for(;;) {
    HWND hwnd=(HWND)InterlockedCompareExchangePointer(&game_hwnd,NULL,NULL);
    BOOL focused=hwnd && IsWindow(hwnd) && GetForegroundWindow()==hwnd;
    BOOL down=(GetAsyncKeyState(VK_F12)&0x8000)!=0;
    if(!focused) {
      previously_down=TRUE;
    } else if(!down) {
      previously_down=FALSE;
    } else if(!previously_down) {
      DWORD now=GetTickCount();
      LONG menu_time=InterlockedCompareExchange(&last_menu_frame_ms,0,0);
      LONG movie_time=InterlockedCompareExchange(&last_movie_frame_ms,0,0);
      previously_down=TRUE;
      if((movie_time && (DWORD)(now-(DWORD)movie_time)<750u) ||
         *SPIDER_GAME_BINK_ACTIVE!=0u) {
        /* Set a one-shot intent: Bink tick wrappers will finish the
         * current clip and all remaining intro movies via game-owned
         * BinkClose, then Shell_MainMenu takes its native Quit branch. */
        if(InterlockedCompareExchange(&intro_quit_pending,1,0)==0)
          say("QUICK-EXIT-F12: intro movie exit requested; native Bink cleanup then real Shell_MainMenu Quit");
      } else if(!menu_time || (DWORD)(now-(DWORD)menu_time)>400u) {
        say("QUICK-EXIT-F12: REAL Shell_MainMenu not active; F12 safely ignored");
      } else {
        unsigned elapsed=0;
        InterlockedExchange(&delivered,0);
        InterlockedExchange(&pending,1);
        say("QUICK-EXIT-F12: F12 queued for REAL Shell_MainMenu (0x493DD6)");
        while(elapsed<800u && InterlockedCompareExchange(&delivered,0,0)==0) {
          Sleep(20);
          elapsed+=20;
        }
        InterlockedExchange(&pending,0);
        if(InterlockedCompareExchange(&delivered,0,0)==0)
          say("QUICK-EXIT-F12: native menu frame not reached within 800ms; no exit");
      }
    }
    Sleep(25);
  }
  return 0;
}

void SpiderQuickExitF12_Init(void (*log_fn)(const char *)) {
  char exe[MAX_PATH],ini[MAX_PATH],val[24],*slash;
  DWORD n;
  HANDLE thread;
  logger=log_fn;
  n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(!n || n>=sizeof(exe) || !(slash=strrchr(exe,'\\'))) {
    say("QUICK-EXIT-F12: disabled (INI path unavailable)");
    return;
  }
  if((size_t)(slash-exe)+sizeof("SpiderMan2k1Fix.ini")>sizeof(ini)) {
    say("QUICK-EXIT-F12: disabled (INI path too long)");
    return;
  }
  memcpy(ini,exe,(size_t)(slash-exe)+1);
  strcpy(ini+(slash-exe)+1,"SpiderMan2k1Fix.ini");
  GetPrivateProfileStringA("System","QuickExitF12","0",val,sizeof(val),ini);
  /* Even with opt-in OFF, suppress the game's original unsafe
   * VK_F12 -> WM_CLOSE, which is known to orphan Bink and the process. */
  if(strcmp(val,"1")!=0) {
    if(suppress_unsafe_native_f12())
      say("QUICK-EXIT-F12: OFF; unsafe original F12 WM_CLOSE suppressed");
    else
      say("QUICK-EXIT-F12: OFF; native F12 signature mismatch");
    return;
  }
  /* Shell_MainMenu has 8 entries. Slot 3 must be "quit" action=0x12,
   * as verified from 0x00552AA8 + 3*40 + 12. Verify this as well as
   * both exact game code sites before touching ANY instruction. */
  if(memcmp(SPIDER_NATIVE_F12_POST,native_f12_original,6)!=0 ||
     memcmp(SPIDER_SHELL_MENU_TICK,shell_menu_tick_original,5)!=0 ||
     memcmp(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1,5)!=0 ||
     memcmp(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2,5)!=0 ||
     memcmp(SPIDER_SHELL_MENU_QUIT_RETURN,
            shell_menu_quit_return_signature,5)!=0 ||
     *(const DWORD *)(SPIDER_SHELL_MENU_ITEMS+3*40+12)!=0x12u) {
    say("QUICK-EXIT-F12: DISABLED: real Shell_MainMenu code/table signature mismatch");
    return;
  }
  if(!install_real_menu_hook()) {
    say("QUICK-EXIT-F12: DISABLED: real Shell_MainMenu tick hook failed");
    return;
  }
  if(!install_intro_tick_hooks()) {
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: original Bink tick hooks could not be installed");
    return;
  }
  if(!suppress_unsafe_native_f12()) {
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: native F12 WM_CLOSE suppression failed");
    return;
  }
  InterlockedExchange(&enabled,1);
  thread=CreateThread(NULL,0,f12_input_thread,NULL,0,NULL);
  if(!thread) {
    InterlockedExchange(&enabled,0);
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: F12 input thread could not start");
    return;
  }
  CloseHandle(thread);
  say("QUICK-EXIT-F12: native Shell_MainMenu Quit hook active; intro movies use native Bink cleanup before Quit; comic/3D unsupported");
}
