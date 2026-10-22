/* Experimental F12 -> genuine main-menu Quit dispatch (supported EXE only).
 *
 * The original WM_KEYDOWN/VK_F12 handler posts WM_CLOSE, which can destroy
 * the HWND while the engine, Bink and DirectDraw keep running. Do not send
 * WM_CLOSE, call ExitProcess/TerminateProcess or invoke the game's resource
 * teardown (0x0048D880) from a worker or DirectDraw callback.
 *
 * The game's main-menu dispatcher calls selection resolver 0x00493990 at
 * the exact CALL site 0x004A202F. Return value 18 maps to jump-table
 * index 7 -> the native Quit case 0x004A238A. That case sets the quit
 * flag AND invokes 0x0048D880 on the game's own thread, returning to the
 * original top-level state machine.
 *
 * Phase 1 is MENU ONLY. Outside that dispatcher the request expires
 * harmlessly, with no deferred hidden exit and no resource mutation.
 * This is not yet an all-scenes F12 implementation.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "quick_exit_f12.h"

#define SPIDER_NATIVE_F12_POST ((BYTE *)(uintptr_t)0x00515373u)
#define SPIDER_MENU_SELECTOR_CALL ((BYTE *)(uintptr_t)0x004A202Fu)
#define SPIDER_MENU_LOOP_READ_CALL ((BYTE *)(uintptr_t)0x004A1A26u)
#define SPIDER_MENU_LOOP_READ_PTR ((const volatile DWORD *)(uintptr_t)0x006B4CA0u)
#define SPIDER_MENU_SELECTOR ((int (__cdecl *)(int))(uintptr_t)0x00493990u)
#define SPIDER_MENU_QUIT_CASE ((const BYTE *)(uintptr_t)0x004A238Au)
#define SPIDER_MENU_QUIT_CLEANUP_CALL ((const BYTE *)(uintptr_t)0x004A2391u)
#define SPIDER_TOPLEVEL_QUIT_CHECK ((const BYTE *)(uintptr_t)0x00455E50u)

static const BYTE native_f12_original[6]={0xFF,0x15,0x70,0xB1,0x53,0x00};
static const BYTE native_f12_bypass[6]={0x83,0xC4,0x10,0x90,0x90,0x90};
static const BYTE menu_selector_original[5]={0xE8,0x5C,0x19,0xFF,0xFF};
/* The original 0x004A1A26 MOV EAX,[0x006B4CA0] is followed by
 * CMP EAX,EDI / JA 0x004A1A36, the menu loop's own teardown path.
 * Replace only that MOV with a cdecl CALL returning the same EAX by
 * default; on F12 return 0xffffffff to take the native exit branch.
 * No global engine pointers, menu resources or WndProc are mutated. */
static const BYTE menu_loop_read_original[5]={0xA1,0xA0,0x4C,0x6B,0x00};
static const BYTE menu_loop_exit_compare_signature[4]={0x3B,0xC7,0x77,0x07};
static const BYTE menu_quit_case_signature[7]={0xC6,0x05,0x88,0xCF,0x60,0x00,0x01};
static const BYTE menu_quit_cleanup_signature[5]={0xE8,0xEA,0xB4,0xFE,0xFF};
static const BYTE top_level_check_signature[6]={0x38,0x1D,0x88,0xCF,0x60,0x00};

#define SPIDER_MENU_QUIT_ID 18
#define SPIDER_F12_DISPATCH_TIMEOUT_MS 1200u

static volatile LONG enabled=0;
static volatile LONG menu_quit_pending=0;
static volatile LONG menu_quit_delivered=0;
static volatile LONG menu_quit_armed=0;
static volatile LONG menu_quit_arm_ms=0;
static volatile LONG movie_last_frame_at=0;
static volatile LONG movie_frame_count=0;
static PVOID volatile game_hwnd=NULL;
static void (*quick_exit_log)(const char *)=NULL;

static void say(const char *line) {
  if(quick_exit_log)quick_exit_log(line);
}

int SpiderQuickExitF12_Enabled(void) {
  return InterlockedCompareExchange(&enabled,0,0)==1;
}

void SpiderQuickExitF12_AttachWindow(HWND hwnd) {
  InterlockedExchangePointer(&game_hwnd,(PVOID)hwnd);
}

/* Passive video observation, performed only at existing game-thread Blt. */
void SpiderQuickExitF12_ObserveMovieFrame(void) {
  InterlockedExchange(&movie_last_frame_at,(LONG)GetTickCount());
  InterlockedIncrement(&movie_frame_count);
}

/* Executed on the real menu game thread, once per native menu frame.
 * Preserve its exact MOV semantics when F12 is not pending. By returning
 * UINT32_MAX (larger than the real menu sentinel in EDI), we let the
 * original JA perform all of the menu's normal teardown before Quit. */
static DWORD __cdecl hooked_menu_loop_exit_read(void) {
  DWORD original=*SPIDER_MENU_LOOP_READ_PTR;
  if(InterlockedCompareExchange(&menu_quit_pending,0,1)==1) {
    InterlockedExchange(&menu_quit_arm_ms,(LONG)GetTickCount());
    InterlockedExchange(&menu_quit_armed,1);
    say("QUICK-EXIT-F12: native MENU loop exit branch requested on game thread");
    return 0xFFFFFFFFu;
  }
  return original;
}

/* Called AFTER the menu's own cleanup has returned, on the game thread.
 * The original menu selector runs first unless our F12 request is armed. */
static int __cdecl hooked_native_menu_selection(int argument) {
  int selected=SPIDER_MENU_SELECTOR(argument);
  if(InterlockedCompareExchange(&menu_quit_armed,0,1)==1) {
    DWORD age=(DWORD)(GetTickCount()-
                       (DWORD)InterlockedCompareExchange(&menu_quit_arm_ms,0,0));
    if(age>15000u) {
      say("QUICK-EXIT-F12: abandoned stale native menu Quit after 15s");
      return selected;
    }
    InterlockedExchange(&menu_quit_delivered,1);
    say("QUICK-EXIT-F12: native MENU Quit command 18 delivered on game thread after native menu cleanup");
    return SPIDER_MENU_QUIT_ID;
  }
  return selected;
}

static int replace_exact_bytes(BYTE *site,const BYTE *expected,
                               const BYTE *replacement,SIZE_T count) {
  DWORD old_protect,ignored;
  if(memcmp(site,expected,count)!=0)return 0;
  if(!VirtualProtect(site,count,PAGE_EXECUTE_READWRITE,&old_protect))return 0;
  memcpy(site,replacement,count);
  FlushInstructionCache(GetCurrentProcess(),site,count);
  VirtualProtect(site,count,old_protect,&ignored);
  return 1;
}

static int suppress_native_f12_close(void) {
  return replace_exact_bytes(SPIDER_NATIVE_F12_POST,
      native_f12_original,native_f12_bypass,sizeof(native_f12_original));
}

static void restore_native_f12_close(void) {
  replace_exact_bytes(SPIDER_NATIVE_F12_POST,
      native_f12_bypass,native_f12_original,sizeof(native_f12_original));
}

static void restore_menu_loop_read(void) {
  BYTE *site=SPIDER_MENU_LOOP_READ_CALL;
  DWORD old_protect,ignored;
  if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old_protect))return;
  memcpy(site,menu_loop_read_original,5);
  FlushInstructionCache(GetCurrentProcess(),site,5);
  VirtualProtect(site,5,old_protect,&ignored);
}

static void restore_menu_dispatcher(void) {
  BYTE *site=SPIDER_MENU_SELECTOR_CALL;
  DWORD old_protect,ignored;
  if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old_protect))return;
  memcpy(site,menu_selector_original,5);
  FlushInstructionCache(GetCurrentProcess(),site,5);
  VirtualProtect(site,5,old_protect,&ignored);
}

static int install_rel32_call(BYTE *site,const BYTE expected[5],
                              const void *callback) {
  intptr_t disp=(intptr_t)callback-(intptr_t)(site+5);
  int32_t relative;
  BYTE patched[5];
  if(disp<(intptr_t)INT32_MIN || disp>(intptr_t)INT32_MAX)return 0;
  relative=(int32_t)disp;
  patched[0]=0xE8;
  memcpy(patched+1,&relative,4);
  return replace_exact_bytes(site,expected,patched,5);
}

static int install_menu_dispatcher(void) {
  return install_rel32_call(SPIDER_MENU_SELECTOR_CALL,
     menu_selector_original,(const void *)hooked_native_menu_selection);
}

static int install_menu_loop_read(void) {
  return install_rel32_call(SPIDER_MENU_LOOP_READ_CALL,
     menu_loop_read_original,(const void *)hooked_menu_loop_exit_read);
}

static DWORD WINAPI quick_exit_f12_thread(LPVOID unused) {
  BOOL previously_down=TRUE; /* require a release after startup/focus loss */
  (void)unused;
  for(;;) {
    HWND hwnd=(HWND)InterlockedCompareExchangePointer(&game_hwnd,NULL,NULL);
    BOOL focused=hwnd && IsWindow(hwnd) && GetForegroundWindow()==hwnd;
    BOOL down=(GetAsyncKeyState(VK_F12)&0x8000)!=0;
    if(!focused) {
      previously_down=TRUE;
    } else if(!down) {
      previously_down=FALSE;
    } else if(!previously_down) {
      LONG last=InterlockedCompareExchange(&movie_last_frame_at,0,0);
      DWORD now=GetTickCount();
      previously_down=TRUE;
      /* F12 inside opening movies cannot be routed through the menu until
       * it actually becomes active. Do not destroy HWND or alter Bink. */
      if(last && (DWORD)(now-(DWORD)last)<800u) {
        say("QUICK-EXIT-F12: movie active; no native menu Quit dispatch yet (no shutdown requested)");
      } else {
        DWORD begun=now;
        InterlockedExchange(&menu_quit_delivered,0);
        InterlockedExchange(&menu_quit_pending,1);
        say("QUICK-EXIT-F12: waiting up to 1.2s for native menu command dispatcher");
        for(;;) {
          if(InterlockedCompareExchange(&menu_quit_armed,0,0)==1)break;
          if((DWORD)(GetTickCount()-begun)>=SPIDER_F12_DISPATCH_TIMEOUT_MS)break;
          Sleep(20);
        }
        InterlockedExchange(&menu_quit_pending,0);
        if(InterlockedCompareExchange(&menu_quit_armed,0,0)==1) {
          DWORD cleanup_begin=GetTickCount();
          say("QUICK-EXIT-F12: native MENU loop exited; awaiting native Quit dispatch after cleanup");
          while(InterlockedCompareExchange(&menu_quit_delivered,0,0)==0 &&
                (DWORD)(GetTickCount()-cleanup_begin)<15000u)
            Sleep(20);
          if(InterlockedCompareExchange(&menu_quit_delivered,0,0)==0)
            say("QUICK-EXIT-F12: WARNING menu Quit not dispatched within 15s; no forced shutdown");
        } else {
          say("QUICK-EXIT-F12: menu loop not reached; F12 safely ignored outside menu");
        }
      }
    }
    Sleep(25);
  }
  return 0; /* Unreachable in normal operation; satisfy WINAPI contract. */
}

void SpiderQuickExitF12_Init(void (*log_fn)(const char *)) {
  char exe[MAX_PATH],ini[MAX_PATH],value[24],*slash;
  DWORD n;
  HANDLE thread;
  quick_exit_log=log_fn;
  n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(!n || n>=sizeof(exe) || !(slash=strrchr(exe,'\\'))) {
    say("QUICK-EXIT-F12: disabled (game INI path unavailable)");
    return;
  }
  if((size_t)(slash-exe)+sizeof("SpiderMan2k1Fix.ini")>sizeof(ini)) {
    say("QUICK-EXIT-F12: disabled (game INI path too long)");
    return;
  }
  memcpy(ini,exe,(size_t)(slash-exe)+1);
  strcpy(ini+(slash-exe)+1,"SpiderMan2k1Fix.ini");
  GetPrivateProfileStringA("System","QuickExitF12","0",value,sizeof(value),ini);

  /* The original game's unsafe F12 -> WM_CLOSE has to be disabled even
   * when QuickExitF12=0, otherwise F12 would still close only the window.
   * Exact image SHA is checked by the caller during DLL startup. */
  if(strcmp(value,"1")!=0) {
    if(suppress_native_f12_close())
      say("QUICK-EXIT-F12: OFF; original unsafe F12/WM_CLOSE suppressed");
    else say("QUICK-EXIT-F12: OFF; original F12 signature not found");
    return;
  }
  if(memcmp(SPIDER_NATIVE_F12_POST,native_f12_original,6) ||
     memcmp(SPIDER_MENU_SELECTOR_CALL,menu_selector_original,5) ||
     memcmp(SPIDER_MENU_LOOP_READ_CALL,menu_loop_read_original,5) ||
     memcmp(SPIDER_MENU_LOOP_READ_CALL+5,menu_loop_exit_compare_signature,4) ||
     memcmp(SPIDER_MENU_QUIT_CASE,menu_quit_case_signature,7) ||
     memcmp(SPIDER_MENU_QUIT_CLEANUP_CALL,menu_quit_cleanup_signature,5) ||
     memcmp(SPIDER_TOPLEVEL_QUIT_CHECK,top_level_check_signature,6)) {
    say("QUICK-EXIT-F12: disabled: native F12/menu Quit signatures mismatch");
    return;
  }
  /* Install menu interception first. If suppressing native F12 fails,
   * roll it back before allowing input. */
  if(!install_menu_dispatcher()) {
    say("QUICK-EXIT-F12: disabled: native menu selector hook failed");
    return;
  }
  if(!install_menu_loop_read()) {
    restore_menu_dispatcher();
    say("QUICK-EXIT-F12: disabled: native menu loop MOV hook failed");
    return;
  }
  if(!suppress_native_f12_close()) {
    restore_menu_loop_read();
    restore_menu_dispatcher();
    say("QUICK-EXIT-F12: disabled: native F12 suppression failed");
    return;
  }
  InterlockedExchange(&enabled,1);
  thread=CreateThread(NULL,0,quick_exit_f12_thread,NULL,0,NULL);
  if(!thread) {
    InterlockedExchange(&enabled,0);
    restore_native_f12_close();
    restore_menu_loop_read();
    restore_menu_dispatcher();
    say("QUICK-EXIT-F12: disabled: key watcher thread creation failed");
    return;
  }
  CloseHandle(thread);
  say("QUICK-EXIT-F12: experimental native MENU loop-exit and Quit dispatch installed; 3D/comics not supported");
}
