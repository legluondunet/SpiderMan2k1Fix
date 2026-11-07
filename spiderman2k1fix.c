#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include <shellapi.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <ddraw.h>
#include <d3d.h>
#include "hud_widescreen.h"
#include "hud_adaptive.h"
#include "ui_canvas_43.h"
#include "ui_transition_native43.h"
#include "ui_transition_native43_bounds25.h"
#include "ui_compositor_policy.h"
#include "hud_visual_candidates.h"
#include "hud_aim_reticle.h"
#include "hud_boss_probe.h"
#include "hud_boss_native.h"
#include "native_pause_title_bounds.h"
#include "native_pause_43_policy.h"
#include "frame_limiter.h"
#include "display_plan.h"
#include "menu_scale.h"
#include "comic_native.h"
#include "virtual_spidey_cfg.h"
#include "display_menu_lock.h"
#include "display_policy.h"
#include "fov_aspect.h"
#include "fov_aspect_win.h"
#include "input_focus_trace.h"
#include "wine_focus_autostart.h"
#include "gamepad_ui_profile.h"
#include "input_notacquired_retry.h"
#include "alt_latch_recovery.h"
#include "native_joystick_unplug.h"
#include "sdl3_gamepad_probe.h"
#include "sdl3_gameplay_plan_probe.h"
#include "sdl3_gameplay_live.h"
#include "sdl3_universal_gameplay.h"
#include "pause_any_key.h"
#include "pause_surface.h"
#include "focus_media_pause.h"
#include "focus_dsound_pause.h"
#include "menu_version.h"
#include "version_info.h"
#include "f9_png.h"
#include "spider_fix_paths.h"
#include "system_diagnostics.h"
#include "f9_toast.h"
#include "quick_exit_f12.h"
#include "pkr_audio_index.h"
#include "pkr_audio_play_hook.h"
#include "pkr_audio_loader.h"

static const BYTE expected_sha256[32] = {
  0x0a,0x11,0xa4,0x9f,0x3f,0x63,0xd4,0xbb,
  0x15,0x65,0x0f,0x32,0x2d,0x65,0x45,0x53,
  0xfc,0x47,0x23,0x4b,0x01,0x88,0x5b,0x85,
  0x2a,0x05,0x8f,0xf0,0x74,0x68,0x9d,0xea
};
static int debug_log_enabled=0;
/* Strictly opt-in: timestamp ALL log lines with a local wall-clock aligned
 * to xprop/date on the host. Add sparse Blt/Flip heartbeats for comparison.
 * This flag never changes game state, focus, or pause timing. */
static int focus_timeline_enabled=0;
/* Load once at startup, not on every drawing/logging call. */
static void debug_log_load_setting(void) {
  char path[MAX_PATH];
  debug_log_enabled=0;
  if(!spider_path_ini_a(path,sizeof(path)))return;
  debug_log_enabled=(GetPrivateProfileIntA("Debug","Log",0,path)==1);
  focus_timeline_enabled=0;
  if(debug_log_enabled) {
    char opt[3]={0};
    DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_FOCUS_TIMELINE",
                                     opt,sizeof(opt));
    focus_timeline_enabled=(n==1 && opt[0]=='1');
  }
}
static void log_line(const char *s) {
  static char path[MAX_PATH];
  static int resolved=0;
  FILE *f;
  if(!debug_log_enabled)return;
  if(!resolved) {
    if(!spider_path_log_a(path,sizeof(path)))return;
    resolved=1;
  }
  f=fopen(path,"a");
  if(f) {
    if(focus_timeline_enabled) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      fprintf(f,"[FOCUS-CLOCK %04u-%02u-%02u %02u:%02u:%02u.%03u local tick-ms=%lu] %s\n",
        (unsigned)st.wYear,(unsigned)st.wMonth,(unsigned)st.wDay,
        (unsigned)st.wHour,(unsigned)st.wMinute,(unsigned)st.wSecond,
        (unsigned)st.wMilliseconds,(unsigned long)GetTickCount(),s);
    } else {
      fprintf(f,"%s\n",s);
    }
    fclose(f);
  }
}
/* Windows creates a class="Ghost" surrogate after several seconds without
 * normal window-message retrieval. Our focus guard can then mistake that
 * surrogate for an Alt+Tab to a different program. This opt-in process
 * compatibility experiment prevents creation of Ghost windows, WITHOUT
 * disabling real focus-loss detection or modifying the game's input loops.
 * It is irreversible until the process exits; OFF by default. Call only
 * after the supported EXE hash and INI have been validated, early at boot. */
typedef VOID (WINAPI *SpiderDisableGhostingFn)(void);
static void pause_apply_windows_ghosting_option(void) {
  HMODULE user32=GetModuleHandleA("user32.dll");
  SpiderDisableGhostingFn disable_ghosting=user32?(SpiderDisableGhostingFn)
    GetProcAddress(user32,"DisableProcessWindowsGhosting"):NULL;
  /* This avoids spurious focus-loss reports from Windows' synthetic Ghost
   * windows during the game's long original message-processing stalls.
   * Like all focus workarounds, this applies only to the SHA-verified game. */
  if(!disable_ghosting) {
    log_line("FOCUS-GHOST: DisableProcessWindowsGhosting unavailable; continuing without suppression");
    return;
  }
  disable_ghosting();
  log_line("FOCUS-GHOST: process ghost-window suppression active (built-in focus protection)");
}

/* Keep the exact SHA-256 guard. A failure must explain which operation
 * failed, rather than labelling every file I/O or CryptoAPI problem as an
 * unsupported game binary. In particular, never fall back to an unchecked EXE.
 */
static void log_exe_check_failure(const char *step, DWORD error) {
  char line[220];
  snprintf(line,sizeof(line),"EXE-CHECK: ERROR step=%s Win32-error=%lu",
           step,(unsigned long)error);
  log_line(line);
}
static BOOL supported_exe(void) {
  char path[MAX_PATH], *name, line[512], actual[65];
  HANDLE file=INVALID_HANDLE_VALUE;
  HCRYPTPROV provider=0;
  HCRYPTHASH hash=0;
  BYTE buf[16384], digest[32];
  DWORD n, len=sizeof(digest);
  unsigned long long total_bytes=0;
  BOOL ok=FALSE;
  size_t i;

  n=GetModuleFileNameA(NULL,path,sizeof(path));
  if (!n || n>=sizeof(path)) {
    log_exe_check_failure("GetModuleFileNameA",GetLastError());
    return FALSE;
  }
  snprintf(line,sizeof(line),"EXE-CHECK: running-image=%s",path);
  log_line(line);
  name=strrchr(path,'\\');
  name=name?name+1:path;
  if(lstrcmpiA(name,"SpideyPC.exe")) {
    snprintf(line,sizeof(line),"EXE-CHECK: ERROR unexpected-name=%s (expected SpideyPC.exe)",name);
    log_line(line);
    return FALSE;
  }

  file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                   NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE) {
    log_exe_check_failure("CreateFileA",GetLastError());
    goto done;
  }
  if(!CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)) {
    log_exe_check_failure("CryptAcquireContextA",GetLastError());
    goto done;
  }
  if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) {
    log_exe_check_failure("CryptCreateHash(CALG_SHA_256)",GetLastError());
    goto done;
  }
  for(;;) {
    if(!ReadFile(file,buf,sizeof(buf),&n,NULL)) {
      log_exe_check_failure("ReadFile",GetLastError());
      goto done;
    }
    if(!n) break;
    total_bytes+=(unsigned long long)n;
    if(!CryptHashData(hash,buf,n,0)) {
      log_exe_check_failure("CryptHashData",GetLastError());
      goto done;
    }
  }
  if(!CryptGetHashParam(hash,HP_HASHVAL,digest,&len,0)) {
    log_exe_check_failure("CryptGetHashParam(HP_HASHVAL)",GetLastError());
    goto done;
  }
  if(len!=sizeof(expected_sha256)) {
    snprintf(line,sizeof(line),"EXE-CHECK: ERROR invalid-digest-length=%lu",(unsigned long)len);
    log_line(line);
    goto done;
  }
  for(i=0;i<32;i++) snprintf(actual+2*i,3,"%02x",(unsigned int)digest[i]);
  snprintf(line,sizeof(line),"EXE-CHECK: sha256=%s bytes=%llu",actual,total_bytes);
  log_line(line);
  ok=(memcmp(digest,expected_sha256,sizeof(expected_sha256))==0);
  if(!ok) log_line("EXE-CHECK: ERROR SHA-256 mismatch against supported game executable");
done:
  if(hash) CryptDestroyHash(hash);
  if(provider) CryptReleaseContext(provider,0);
  if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
  return ok;
}

/* Exact-signature Win11 fclose(NULL) recovery for the verified game EXE.
 *
 * 2026-10-19 ProcDump: C0000005 read from address 0xC, EIP 0052A805,
 * ESI=0, EDI=-1. This is MSVC's statically linked fclose(FILE *) at
 * game RVA 0x12A7FC. Return address 005186F8 proves this call site's
 * FILE* argument is NULL. fclose(NULL) has undefined behavior.
 *
 * On precisely this exception and caller, skip to fclose's
 * existing return epilogue (mov eax,edi; pop edi; pop esi; ret). This
 * safely returns EOF (-1) to the original caller rather than pretending
 * the file was closed. The non-null FILE* path is never changed.
 *
 * No game byte is patched. The VEH is armed by default only after SHA256
 * verification, with opcode and caller validation. An explicit environment
 * value SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD=0 disables it for diagnosis.
 * Its handler does not
 * perform IO, allocate or touch DirectDraw/DirectInput/windows.
 */
static uintptr_t null_fclose_image_base=0;
static PVOID null_fclose_veh_handle=NULL;
static volatile LONG null_fclose_interceptions=0;
static volatile LONG null_fclose_reported=0;
static LONG CALLBACK guarded_null_fclose_veh(PEXCEPTION_POINTERS info) {
  EXCEPTION_RECORD *ex;
  CONTEXT *ctx;
  uintptr_t slot,base;
  MEMORY_BASIC_INFORMATION mb;
  if(!info || !(ex=info->ExceptionRecord) || !(ctx=info->ContextRecord))
    return EXCEPTION_CONTINUE_SEARCH;
  base=null_fclose_image_base;
  if(!base || ex->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION ||
     ex->NumberParameters<2 || ex->ExceptionInformation[0]!=0 ||
     ex->ExceptionInformation[1]!=0xC ||
     (uintptr_t)ex->ExceptionAddress!=base+0x12A805 ||
     (uintptr_t)ctx->Eip!=base+0x12A805 ||
     ctx->Esi!=0 || ctx->Edi!=0xFFFFFFFFUL)
    return EXCEPTION_CONTINUE_SEARCH;
  /* Current position is after fclose's push esi / push edi prologue:
   * ESP+8 is its caller's return address, ESP+12 the NULL FILE*.
   */
  slot=(uintptr_t)ctx->Esp+8;
  if(slot<(uintptr_t)ctx->Esp || slot+8<slot ||
     VirtualQuery((LPCVOID)slot,&mb,sizeof(mb))!=sizeof(mb) ||
     mb.State!=MEM_COMMIT || (mb.Protect&(PAGE_NOACCESS|PAGE_GUARD)) ||
     slot+8>(uintptr_t)mb.BaseAddress+mb.RegionSize)
    return EXCEPTION_CONTINUE_SEARCH;
  if(*(DWORD*)slot!=(DWORD)(base+0x1186F8) || *(DWORD*)(slot+4)!=0)
    return EXCEPTION_CONTINUE_SEARCH;
  InterlockedIncrement(&null_fclose_interceptions);
  ctx->Eip=(DWORD)(base+0x12A828); /* original fclose error return */
  return EXCEPTION_CONTINUE_EXECUTION;
}
static void install_null_fclose_guard(void) {
  static const BYTE signature[]={
    0x56,0x8B,0x74,0x24,0x08,0x57,0x83,0xCF,0xFF,
    0xF6,0x46,0x0C,0x40
  };
  char env[4]={0};
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD",
                                  env,sizeof(env));
  if(n==1 && env[0]=='0') {
    log_line("FCLOSE-GUARD: disabled by diagnostic override");
    return;
  }
  if(!base || (uintptr_t)base!=0x400000 ||
     memcmp(base+0x12A7FC,signature,sizeof(signature))!=0) {
    log_line("FCLOSE-GUARD: ERROR signature/layout mismatch; not installed");
    return;
  }
  null_fclose_image_base=(uintptr_t)base;
  null_fclose_veh_handle=AddVectoredExceptionHandler(1,guarded_null_fclose_veh);
  if(null_fclose_veh_handle)
    log_line("FCLOSE-GUARD: exact-signature fclose(NULL) recovery armed (one caller, valid FILE* untouched)");
  else {
    null_fclose_image_base=0;
    log_line("FCLOSE-GUARD: ERROR AddVectoredExceptionHandler failed");
  }
}
static void report_null_fclose_recovery(void) {
  LONG hits=InterlockedCompareExchange(&null_fclose_interceptions,0,0);
  if(hits>InterlockedCompareExchange(&null_fclose_reported,0,0)) {
    char line[180];
    InterlockedExchange(&null_fclose_reported,hits);
    snprintf(line,sizeof(line),
      "FCLOSE-GUARD: recovered %ld fclose(NULL) call(s) at game EIP 0x0052A805 / caller 0x005186F8",
      (long)hits);
    log_line(line);
  }
}
/* DirectDraw7 borderless-windowed adaptation. Never alter physical display modes. */
typedef HRESULT (WINAPI *CreateExFn)(GUID *, void **, REFIID, IUnknown *);
typedef HRESULT (WINAPI *CoopFn)(void *, HWND, DWORD);
typedef HRESULT (WINAPI *ModeFn)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef HRESULT (WINAPI *SurfaceCreateFn)(void *, DDSURFACEDESC2 *, IDirectDrawSurface7 **, IUnknown *);
typedef HRESULT (WINAPI *SurfaceBltFn)(void *, LPRECT, IDirectDrawSurface7 *, LPRECT, DWORD, LPDDBLTFX);
typedef HRESULT (WINAPI *SurfaceFlipFn)(void *, IDirectDrawSurface7 *, DWORD);
typedef HRESULT (WINAPI *SurfaceGetAttachedFn)(void *, LPDDSCAPS2, IDirectDrawSurface7 **);
/* F9 screenshot path investigation. All five diagnostic hooks are installed
 * ONLY for SPIDERMAN2K1FIX_F9_TRACE=1 AND Debug Log=1. Read-only pass-through.
 * IDirectDrawSurface7 vtable slots: GetDC=17 GetSurfaceDesc=22 Lock=25
 * ReleaseDC=26 Unlock=32. No executable patch, pixel read or allocation. */
typedef HRESULT (WINAPI *SurfaceGetDCFn)(void *, HDC *);
typedef HRESULT (WINAPI *SurfaceGetDescFn)(void *, LPDDSURFACEDESC2);
typedef HRESULT (WINAPI *SurfaceLockFn)(void *, LPRECT, LPDDSURFACEDESC2, DWORD, HANDLE);
typedef HRESULT (WINAPI *SurfaceReleaseDCFn)(void *, HDC);
typedef HRESULT (WINAPI *SurfaceUnlockFn)(void *, LPRECT);
static CreateExFn original_create;
static CoopFn original_coop;
static ModeFn original_mode;
static SurfaceCreateFn original_surface_create;
static SurfaceBltFn original_blt;
static SurfaceFlipFn original_flip;
static SurfaceGetAttachedFn original_get_attached;
static SurfaceGetDCFn original_get_dc;
static SurfaceGetDescFn original_surface_get_desc;
static SurfaceLockFn original_surface_lock;
static SurfaceReleaseDCFn original_release_dc;
static SurfaceUnlockFn original_surface_unlock;
static volatile LONG f9_trace_enabled=0,f9_trace_keydown=0;
static volatile LONG f9_trace_until=0,f9_trace_events=0,f9_trace_logged=0;
/* Experimental screenshot-only readback redirection; disabled by default.
 * The original game locks PRIMARY at RVA 0x1034D4 on F9, then unlocks
 * PRIMARY at 0x103537. Under Win11 its windowed PRIMARY Lock returns a
 * magenta image despite a successful GPU Blt from the real D3D backbuffer.
 * Redirect that one Lock/Unlock pair to the game-rendered backbuffer.
 * Use one owned COM ref for the duration of the lock to avoid dangling
 * surfaces on shutdown; never touch other Lock callers. */
static volatile LONG f9_backbuffer_fix_enabled=0;
static volatile LONG f9_native_png_enabled=0;
static volatile LONG f9_png_only_enabled=0;
static volatile LONG f9_png_capture_active=0;
static BOOL f9_static_comic_capture=FALSE; /* F9 game thread only */
static volatile LONG f9_backbuffer_lock_active=0;
static DWORD f9_backbuffer_lock_thread=0;
static IDirectDrawSurface7 *f9_backbuffer_locked_surface=NULL;
static void *patched_ddraw_vtable[30];
/* IDirectDrawSurface7 has 49 vtable slots (IUnknown + 46 methods). */
static void *patched_surface_vtable[49];
static volatile LONG ddraw_vtable_ready=0, surface_vtable_ready=0;
static volatile LONG surface_create_count=0, blt_count=0, flip_count=0;
static volatile LONG attached_count=0;
static volatile LONG flip_hooked_surfaces=0;
static volatile LONG flip_hook_mismatches=0;
/* Fullscreen BORDERLESS WINDOWED: real DDSCL_NORMAL cooperative level;
 * the game's requested flipping primary is adapted to a windowed primary and
 * a separate D3D7 offscreen render target. No exclusive fallback is allowed.
 * POC: Wine/DXVK validation of CreateDevice and movie Blts is mandatory. */
static HWND game_hwnd=NULL;
static IDirectDrawSurface7 *borderless_primary=NULL; /* weak, game-owned */
static IDirectDrawSurface7 *borderless_back=NULL; /* one owned reference */
/* Most recent *decoded* DirectDraw source of an intro movie Blt. Video is
 * presented AFTER Flip into PRIMARY, so PRIMARY-DC GDI readback is unreliable
 * (magenta/purple frames under native Windows). Keep a game-thread-only COM
 * reference to the decoded source and read it only on F9. */
static IDirectDrawSurface7 *f9_movie_source=NULL;
static volatile LONG f9_movie_last_flip=-10000;
static void f9_movie_drop_source(void) {
  if(f9_movie_source) {
    IDirectDrawSurface7_Release(f9_movie_source);
    f9_movie_source=NULL;
  }
  InterlockedExchange(&f9_movie_last_flip,-10000);
}

static LONG borderless_monitor_x=0,borderless_monitor_y=0;
static volatile LONG borderless_present_failed=0;
static volatile LONG borderless_legacy_mode_calls=0;

/* Win11 taskbar window icon: extract the FIRST icon group embedded in
 * the supported SpideyPC.exe (already verified by SHA-256). Do not assume
 * its resource ID, and do not depend on an adjacent SpideyPC.ico file.
 * ExtractIconExW uses the group resource's size variants for big/small
 * icons, without hardcoding the resource identifier.
 *
 * Safe default ON; extraction failure keeps the original window icon.
 * SPIDERMAN2K1FIX_WINDOW_ICON=0 disables this for A/B tests.
 * Does not change window flags, Z-order, focus or rendering.
 */
static HICON spidey_icon_small=NULL, spidey_icon_big=NULL;
static BOOL spidey_icon_load_attempted=FALSE;
static int spidey_icon_enabled(void) {
  char value[4]={0};
  DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_WINDOW_ICON",value,sizeof(value));
  return !(n==1 && value[0]=='0');
}
static void spidey_apply_window_icon(HWND hwnd) {
  WCHAR path[MAX_PATH];
  DWORD n,error=0;
  UINT extracted;
  HICON small=NULL,big=NULL;
  char line[200];
  if(!spidey_icon_enabled() || !hwnd || !IsWindow(hwnd)) return;
  if(!spidey_icon_load_attempted) {
    spidey_icon_load_attempted=TRUE;
    n=GetModuleFileNameW(NULL,path,MAX_PATH);
    if(!n || n>=MAX_PATH) {
      log_line("WINDOW-ICON: cannot resolve running EXE path; icon unchanged");
      return;
    }
    /* 0 = first RT_GROUP_ICON in the running executable. No external ICO. */
    SetLastError(0);
    extracted=ExtractIconExW(path,0,&big,&small,1);
    /* Windows may count both the large and small images in the return
     * value (2 with nIcons=1). Nonzero is success unless UINT_MAX.
     * Also demand both valid handles before touching the game's HWND.
     * GetLastError is only meaningful if ExtractIconExW returned UINT_MAX.
     */
    if(extracted==0 || extracted==UINT_MAX || !small || !big) {
      error=(extracted==UINT_MAX)?GetLastError():0;
      if(small) DestroyIcon(small);
      if(big) DestroyIcon(big);
      snprintf(line,sizeof(line),
        "WINDOW-ICON: EXE embedded icon unavailable extracted=%u Win32=%lu; original icon unchanged",
        (unsigned)extracted,(unsigned long)error);
      log_line(line);
      return;
    }
    spidey_icon_small=small;
    spidey_icon_big=big;
    snprintf(line,sizeof(line),
      "WINDOW-ICON: embedded SpideyPC.exe icon loaded small+big extracted=%u",
      (unsigned)extracted);
    log_line(line);
  }
  if(!spidey_icon_small || !spidey_icon_big) return;
  if((HICON)SendMessageW(hwnd,WM_GETICON,ICON_SMALL,0)==spidey_icon_small &&
     (HICON)SendMessageW(hwnd,WM_GETICON,ICON_BIG,0)==spidey_icon_big) return;
  SendMessageW(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)spidey_icon_small);
  SendMessageW(hwnd,WM_SETICON,ICON_BIG,(LPARAM)spidey_icon_big);
  snprintf(line,sizeof(line),"WINDOW-ICON: applied embedded SpideyPC.exe icon hwnd=%p small=%d big=%d",
    (void*)hwnd,
    (HICON)SendMessageW(hwnd,WM_GETICON,ICON_SMALL,0)==spidey_icon_small,
    (HICON)SendMessageW(hwnd,WM_GETICON,ICON_BIG,0)==spidey_icon_big);
  log_line(line);
}
static void borderless_window_apply(void) {
  MONITORINFO mi; HMONITOR mon; LONG_PTR style,exstyle;
  RECT r; char line[240];
  if(!game_hwnd || !IsWindow(game_hwnd)) return;
  memset(&mi,0,sizeof(mi));mi.cbSize=sizeof(mi);
  mon=MonitorFromWindow(game_hwnd,MONITOR_DEFAULTTONEAREST);
  if(!GetMonitorInfoA(mon,&mi)) return;
  r=mi.rcMonitor;
  borderless_monitor_x=r.left;borderless_monitor_y=r.top;
  style=GetWindowLongPtrA(game_hwnd,GWL_STYLE);
  exstyle=GetWindowLongPtrA(game_hwnd,GWL_EXSTYLE);
  SetWindowLongPtrA(game_hwnd,GWL_STYLE,
    (style & (WS_CLIPSIBLINGS|WS_CLIPCHILDREN)) | WS_POPUP | WS_VISIBLE);
  SetWindowLongPtrA(game_hwnd,GWL_EXSTYLE,
    (exstyle & ~(WS_EX_DLGMODALFRAME|WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_STATICEDGE))|WS_EX_APPWINDOW);
  SetWindowPos(game_hwnd,HWND_NOTOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,
    SWP_FRAMECHANGED|SWP_SHOWWINDOW|SWP_NOACTIVATE);
  spidey_apply_window_icon(game_hwnd);
  snprintf(line,sizeof(line),"BORDERLESS: window=%p style=WS_POPUP rect=(%ld,%ld)+%ldx%ld cooperativity=WINDOWED",
    (void*)game_hwnd,(long)r.left,(long)r.top,
    (long)(r.right-r.left),(long)(r.bottom-r.top));
  log_line(line);
}
static HRESULT borderless_get_back(void *surface,LPDDSCAPS2 caps,IDirectDrawSurface7 **out) {
  if(out && caps && (caps->dwCaps&DDSCAPS_BACKBUFFER) &&
     surface==(void*)borderless_primary && borderless_back) {
    *out=borderless_back;
    IDirectDrawSurface7_AddRef(*out);
    return DD_OK;
  }
  return original_get_attached(surface,caps,out);
}
/* Opt-in native-output experiment; 0 keeps the known-good forwarding path.
 * Enabled only for the 640x480 MENU requests; full-resolution gameplay keeps
 * its existing mode and the already-validated 30 FPS hook.
 */
static volatile LONG menu_upscale_enabled=0;
static volatile LONG menu_upscale_active=0;
/* Opt-in 4:3 D3D7 viewport for projected introductory images and FMV quads.
 * The existing 2D vertex mapper continues to use this same destination rect.
 * Does NOT alter DirectDraw blits or the validated 30 FPS limiter.
 */
static volatile LONG media_viewport_4x3_enabled=0;
/* Separate opt-in for DirectDraw presentation copies; do not change other Blt calls. */
static volatile LONG media_native_blt_4x3_enabled=0;
/* Separate opt-in: accept other decoded FMV frame sizes (same final blit path).
 * Do not assume they are 640x480: the four opening movies use multiple
 * source surface sizes. The intended legacy presentation frame remains 4:3.
 * This option extends ONLY the already guarded full-frame PRIMARY/BACKBUFFER
 * copy to the native monitor; never modifies texture/sprite Blts.
 */
static volatile LONG media_native_blt_anysize_enabled=0;
static volatile LONG media_native_blt_knownsizes_enabled=0;
static volatile LONG media_native_blt_4x3_hits=0;
/* Isolated visual trial for the exact padded-BMP full-frame quad identified
 * by COMIC-TRACE. 0 keeps the previous confirmed build byte-for-byte in
 * graphics behavior. Its monitor-dependent 0.75 factor for 16:9 is a test,
 * not yet proof that the framebuffer mapping is correct under all Wine backends. */
/* New opt-in ONLY for native full-screen comic textures, no MENU changes. */
static volatile LONG comic_native_aspect_enabled=0;
static volatile LONG comic_native_aspect_matches=0;
/* Comic stills do not require Flip while visible. */
static volatile LONG f9_comic_last_draw_flip=-100000;

static DWORD desktop_w=0, desktop_h=0;
/* The INI's single resolution target. Display backends still require
 * native-run validation for every non-Auto monitor mode. */
static char display_resolution_setting[64]="Auto";
/* AspectRatio=4:3 keeps a native-output DirectDraw mode but the game
 * computes an original 4:3 logical frame, fitted to a centered rectangle.
 * Do not touch gameplay in AspectRatio=Auto or the validated menu path. */
static char display_aspect_setting[16]="Auto";
static SpiderMenuMapping menu_map,game_map;
static DWORD game_output_w=0,game_output_h=0;
static DWORD game_logical_w=0,game_logical_h=0;
static volatile LONG gameplay_4x3_enabled=0,gameplay_4x3_active=0;
static volatile LONG gameplay_4x3_mapped=0,gameplay_4x3_bar_errors=0;
static volatile LONG menu_scaled_draws=0, menu_unscaled_draws=0;
static volatile LONG menu_clear_failures=0;
static volatile LONG menu_version_draws=0;
static volatile LONG menu_version_movie_blits=0;
static volatile LONG menu_version_last_movie_flip=-1000;
static volatile LONG menu_version_logged=0;

static SpiderFrameLimiter g_frame_limiter;
static BOOL g_frame_limiter_ready=FALSE;

/* At the first DirectDrawCreateEx hook, the game's real window is
 * usually already created, but SetCooperativeLevel has not run yet.
 * Discover ONLY windows belonging to the game main thread: a foreign
 * foreground application's HWND must never choose the output monitor. */
typedef struct {HWND found;} SpiderEarlyWindow;
static BOOL CALLBACK spider_find_early_window(HWND window,LPARAM context) {
  SpiderEarlyWindow *found=(SpiderEarlyWindow *)(uintptr_t)context;
  DWORD pid=0;
  if(!IsWindow(window) || (GetWindowLongPtrA(window,GWL_STYLE)&WS_CHILD) ||
     GetWindow(window,GW_OWNER)!=NULL ||
     GetWindowThreadProcessId(window,&pid)==0 || pid!=GetCurrentProcessId())
    return TRUE;
  if(!found->found || IsWindowVisible(window))
    found->found=window;
  return !IsWindowVisible(window);
}
static HWND spider_find_game_window_before_ddraw(void) {
  SpiderEarlyWindow state={NULL};
  HWND active=GetActiveWindow();
  DWORD pid=0;
  if(active && IsWindow(active) &&
     GetWindowThreadProcessId(active,&pid)!=0 && pid==GetCurrentProcessId() &&
     !(GetWindowLongPtrA(active,GWL_STYLE)&WS_CHILD))
    return active;
  EnumThreadWindows(GetCurrentThreadId(),spider_find_early_window,
                    (LPARAM)(uintptr_t)&state);
  /* Some Wine/Win32 backends create the game HWND on a different thread
   * from the thread performing DirectDrawCreateEx. Search the process's
   * top-level windows as well, never arbitrary windows of other apps. */
  if(!state.found)
    EnumWindows(spider_find_early_window,(LPARAM)(uintptr_t)&state);
  return state.found;
}

/* The desktop mode is probed BEFORE the game's exclusive mode change.
 * Do not replace SetDisplayMode alone: the 640x480 D3D target and viewport
 * would then be inconsistent with the primary/backbuffer dimensions.
 */
static void probe_desktop_mode(HWND hwnd) {
  MONITORINFOEXA info;
  DEVMODEA mode;
  HMONITOR monitor;
  SpiderDisplayRect r;
  char line[260];
  if (!hwnd || !IsWindow(hwnd)) return;
  memset(&info,0,sizeof(info)); info.cbSize=sizeof(info);
  monitor=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
  if (!GetMonitorInfoA(monitor,(MONITORINFO*)&info)) return;
  memset(&mode,0,sizeof(mode)); mode.dmSize=sizeof(mode);
  if (!EnumDisplaySettingsA(info.szDevice,ENUM_CURRENT_SETTINGS,&mode)) return;
  if (mode.dmPelsWidth<640 || mode.dmPelsHeight<480 ||
      mode.dmPelsWidth>16384 || mode.dmPelsHeight>16384) return;
  /* The first DDraw creation is early enough to correct the virtual CFG
   * BEFORE the game's native loader reads it. SetCooperativeLevel supplies
   * the authoritative HWND later. Until the first intercepted game mode,
   * allow this actual HWND to correct an early-window monitor mismatch.
   * After mode acknowledgement, never recapture the desktop dimensions. */
  if ((!desktop_w || !desktop_h) ||
      (InterlockedCompareExchange(&borderless_legacy_mode_calls,0,0)==0 &&
       (desktop_w!=mode.dmPelsWidth || desktop_h!=mode.dmPelsHeight))) {
    unsigned selected_w=0,selected_h=0;
    SpiderMenuMapping mapping;
    /* MENU always uses the captured desktop mode; gameplay may use an
     * independent physical output, while its logical canvas is 4:3. */
    if(!SpiderMenu_MakeMapping(640,480,(int)mode.dmPelsWidth,
                               (int)mode.dmPelsHeight,&mapping)) {
      log_line("DISPLAY-NATIVE: ERROR native monitor mapping unavailable; refusing guessed resolution");
      return;
    }
    desktop_w=mapping.output_w;
    desktop_h=mapping.output_h;
    menu_map=mapping;
    game_output_w=0;game_output_h=0;
    game_logical_w=0;game_logical_h=0;
    InterlockedExchange(&gameplay_4x3_enabled,0);
    if(SpiderDisplayPolicy_ResolveRuntime(display_resolution_setting,display_aspect_setting,
          mode.dmPelsWidth,mode.dmPelsHeight,&selected_w,&selected_h)) {
      SpiderDisplayMenu_SetTarget(selected_w,selected_h);
      if(lstrcmpiA(display_aspect_setting,"4:3")==0) {
        unsigned physical_w=desktop_w,physical_h=desktop_h;
        if(physical_w && physical_h &&
           SpiderMenu_MakeMapping((int)selected_w,(int)selected_h,
                                  (int)physical_w,(int)physical_h,&game_map)) {
          char s[250];
          game_output_w=physical_w;game_output_h=physical_h;
          game_logical_w=selected_w;game_logical_h=selected_h;
          InterlockedExchange(&gameplay_4x3_enabled,1);
          snprintf(s,sizeof(s),"DISPLAY-ASPECT-43: logical=%lux%lu physical=%lux%lu rect=(%d,%d)-(%d,%d) [NATIVE OUTPUT; NO RESOLUTION ENUMERATION]",
               (unsigned long)game_logical_w,(unsigned long)game_logical_h,
               (unsigned long)game_output_w,(unsigned long)game_output_h,
               game_map.left,game_map.top,game_map.right,game_map.bottom);
          log_line(s);
        } else log_line("DISPLAY-ASPECT-43: ERROR presentation map unavailable; mode will not be intercepted");
      }
    } else {
      log_line("DISPLAY: WARNING invalid resolution policy; using native desktop mode");
      SpiderDisplayMenu_SetTarget(mode.dmPelsWidth,mode.dmPelsHeight);
    }
  }

  SpiderCfg_ObserveNativeMonitor(mode.dmPelsWidth,mode.dmPelsHeight);
  snprintf(line,sizeof(line),
    "DISPLAY-POLICY: monitor=%lux%lu requested=%s aspect=%s menu-output=%lux%lu",
    (unsigned long)mode.dmPelsWidth,(unsigned long)mode.dmPelsHeight,
    display_resolution_setting,display_aspect_setting,(unsigned long)desktop_w,(unsigned long)desktop_h);
  log_line(line);
  if(lstrcmpiA(display_aspect_setting,"4:3")==0)
    log_line("BORDERLESS: MENU native 4:3; GAMEPLAY logical 4:3 inside monitor-sized WS_POPUP window");
  snprintf(line,sizeof(line),
    "DISPLAY-PROBE: original monitor=%lux%lu %lu bpp %lu Hz device=%s",
    (unsigned long)mode.dmPelsWidth,(unsigned long)mode.dmPelsHeight,
    (unsigned long)mode.dmBitsPerPel,(unsigned long)mode.dmDisplayFrequency,info.szDevice);
  log_line(line);
  /* The startup inventory lists every active display, but only the real
   * game HWND determines the output monitor. Report that mapping after the
   * early DirectDraw monitor probe, never from the foreground application. */
  {
    static char last_selected[260]="";
    const char *device=strrchr(info.szDevice,'\\');
    snprintf(line,sizeof(line),
      "SYS-DISPLAY-SELECTED: game_window_monitor=%s resolution=%lux%lu refresh=%luHz primary=%s",
      device?device+1:info.szDevice,
      (unsigned long)mode.dmPelsWidth,(unsigned long)mode.dmPelsHeight,
      (unsigned long)mode.dmDisplayFrequency,
      (info.dwFlags&MONITORINFOF_PRIMARY)?"yes":"no");
    /* Probe runs both before and during SetCooperativeLevel. Log the first
     * game monitor, then only meaningful selection/resolution changes. */
    if(debug_log_enabled && strcmp(line,last_selected)!=0) {
      log_line(line);
      strcpy(last_selected,line);
    }
  }
  if (!SpiderDisplay_Fit(640,480,(int)mode.dmPelsWidth,(int)mode.dmPelsHeight,&r)) return;
  snprintf(line,sizeof(line),
    "DISPLAY-PLAN: 4:3 content fitted without distortion to desktop %lux%lu => viewport=(%d,%d)-(%d,%d) [PROBE ONLY]",
    (unsigned long)mode.dmPelsWidth,(unsigned long)mode.dmPelsHeight,
    r.left,r.top,r.right,r.bottom);
  log_line(line);
  snprintf(line,sizeof(line),
    "DISPLAY-PLAN: true native aspect %.5f => horizontal FOV/HUD scale %.6f; viewport/render-surface patch needed [PROBE ONLY]",
    (double)mode.dmPelsWidth/(double)mode.dmPelsHeight,
    SpiderDisplay_NativeHorizontalFactor(640,480,(int)mode.dmPelsWidth,(int)mode.dmPelsHeight));
  log_line(line);
}

static void log_monitor(HWND hwnd) {
  MONITORINFO mi; HMONITOR mon; char line[220];
  if(!hwnd || !IsWindow(hwnd)) return;
  memset(&mi,0,sizeof(mi)); mi.cbSize=sizeof(mi);
  mon=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
  if(!GetMonitorInfoA(mon,&mi)) return;
  snprintf(line,sizeof(line),"DISPLAY: monitor=%ldx%ld origin=(%ld,%ld) game-window=%p",
    (long)(mi.rcMonitor.right-mi.rcMonitor.left),(long)(mi.rcMonitor.bottom-mi.rcMonitor.top),
    (long)mi.rcMonitor.left,(long)mi.rcMonitor.top,(void*)hwnd);
  log_line(line);
  if(desktop_w && desktop_h) {
    const DWORD window_w=(DWORD)(mi.rcMonitor.right-mi.rcMonitor.left);
    const DWORD window_h=(DWORD)(mi.rcMonitor.bottom-mi.rcMonitor.top);
    snprintf(line,sizeof(line),
      "DISPLAY-DPI: DirectDraw physical=%lux%lu HWND monitor=%lux%lu pixels %s",
      (unsigned long)desktop_w,(unsigned long)desktop_h,
      (unsigned long)window_w,(unsigned long)window_h,
      (desktop_w==window_w && desktop_h==window_h)?"MATCH":"MISMATCH (DPI virtualization)");
    log_line(line);
  }
}

/* Direct3D7 hooks for physical viewport and vertex mapping. */
typedef HRESULT (WINAPI *DDQueryFn)(void *, REFIID, void **);
typedef HRESULT (WINAPI *D3DCreateDeviceFn)(void *, REFCLSID, IDirectDrawSurface7 *, IDirect3DDevice7 **);
typedef HRESULT (WINAPI *D3DViewportFn)(void *, LPD3DVIEWPORT7);
/* IDirect3DDevice7::Clear is vtable slot 10; read-only instrumentation. */
/* IDirect3DDevice7: BeginScene slot 5, EndScene slot 6. The witness is
 * emitted while the game's D3D7 scene remains active. */
static DDQueryFn original_dd_query;
static D3DCreateDeviceFn original_d3d_create_device;
static D3DViewportFn original_set_viewport;
typedef HRESULT (WINAPI *D3DSetRenderStateFn)(void *,D3DRENDERSTATETYPE,DWORD);
static D3DSetRenderStateFn original_set_render_state;
/* D3D7 state is recorded only when a successful driver call confirms it. */
static volatile LONG fov_z_state_known=0, fov_z_state_enabled=0;
static volatile LONG fov_game_height=0;
static volatile LONG fov_horplus_scopes_ready=0, fov_horplus_skips_hud=0;
static volatile LONG fov_horplus_calls=0, fov_horplus_skips_unknown=0,
  fov_horplus_skips_depth=0, fov_horplus_skips_shape=0;
static HRESULT WINAPI hooked_set_render_state(void *self,D3DRENDERSTATETYPE state,DWORD value) {
  HRESULT hr=original_set_render_state(self,state,value);
  if(SUCCEEDED(hr) && state==D3DRENDERSTATE_ZENABLE) {
    InterlockedExchange(&fov_z_state_enabled,value!=D3DZB_FALSE);
    InterlockedExchange(&fov_z_state_known,1);
  }
  return hr;
}

static void *patched_d3d_vtable[8], *patched_device_vtable[49];
static volatile LONG d3d_ready=0, device_ready=0, viewport_count=0;
/* Diagnostic-only call boundary: determines whether the crash occurs inside
 * native SetViewport or only after its HRESULT has been returned. Never
 * swallow an exception, never alter success/failure from the real driver.
 */
static HRESULT call_original_set_viewport_traced(void *self,LPD3DVIEWPORT7 view,
                                                 LONG sequence,const char *mode) {
  HRESULT hr;
  if(debug_log_enabled && sequence<=24) {
    MEMORY_BASIC_INFORMATION region;
    SIZE_T queried=0;
    char line[256];
    memset(&region,0,sizeof(region));
    if(original_set_viewport)
      queried=VirtualQuery((LPCVOID)(uintptr_t)original_set_viewport,
                           &region,sizeof(region));
    snprintf(line,sizeof(line),
      "D3D7-VIEWPORT-DIAG: BEFORE #%ld mode=%s device=%p original=%p memory-state=0x%08lX protection=0x%08lX query=%lu rect=%lux%lu+%lu+%lu",
      (long)sequence,mode,self,(void*)(uintptr_t)original_set_viewport,
      (unsigned long)region.State,(unsigned long)region.Protect,(unsigned long)queried,
      (unsigned long)(view?view->dwWidth:0),(unsigned long)(view?view->dwHeight:0),
      (unsigned long)(view?view->dwX:0),(unsigned long)(view?view->dwY:0));
    log_line(line);
  }
  if(!original_set_viewport) {
    log_line("D3D7-VIEWPORT-DIAG: ERROR original SetViewport pointer is NULL");
    return E_POINTER;
  }
  hr=original_set_viewport(self,view);
  if(debug_log_enabled && sequence<=24) {
    char line[160];
    snprintf(line,sizeof(line),"D3D7-VIEWPORT-DIAG: AFTER #%ld mode=%s hr=0x%08lX",
      (long)sequence,mode,(unsigned long)hr);
    log_line(line);
  }
  return hr;
}
static HRESULT WINAPI hooked_set_viewport(void *self,LPD3DVIEWPORT7 view) {
  LONG n=InterlockedIncrement(&viewport_count);
  if(debug_log_enabled && n<=24 && view) {
    char line[240];
    snprintf(line,sizeof(line),"D3D7: viewport #%ld x=%lu y=%lu width=%lu height=%lu z=[%.5g,%.5g] (unchanged)",
      (long)n,(unsigned long)view->dwX,(unsigned long)view->dwY,
      (unsigned long)view->dwWidth,(unsigned long)view->dwHeight,
      (double)view->dvMinZ,(double)view->dvMaxZ);
    log_line(line);
  }
  /* The game still reports its logical 4:3 viewport. The physical viewport
   * covers only the centered presentation area on the native-sized target. */
  if(view && InterlockedCompareExchange(&gameplay_4x3_active,0,0)==1 &&
     view->dwX==0 && view->dwY==0 &&
     view->dwWidth==game_logical_w && view->dwHeight==game_logical_h) {
    D3DVIEWPORT7 physical=*view;
    physical.dwX=(DWORD)game_map.left;
    physical.dwY=(DWORD)game_map.top;
    physical.dwWidth=(DWORD)(game_map.right-game_map.left);
    physical.dwHeight=(DWORD)(game_map.bottom-game_map.top);
    if(debug_log_enabled && n<=24) log_line("DISPLAY-ASPECT-43: gameplay D3D7 viewport centered on native output");
    return call_original_set_viewport_traced(self,&physical,n,"gameplay-4x3");
  }
  if (view && InterlockedCompareExchange(&menu_upscale_active,0,0)==1 &&
      view->dwWidth==640 && view->dwHeight==480 &&
      view->dwX==0 && view->dwY==0) {
    D3DVIEWPORT7 physical=*view;
    /* Temporary A/B test: set in the launching PowerShell session only.
     * Unset variable after test. Default path is the unchanged 4:3 mapping.
     * This bypasses only the viewport remap, NOT the existing proxy/hooks. */
    {
      char diagnostic[4]={0};
      DWORD nbytes=GetEnvironmentVariableA("SPIDERMAN2K1FIX_VIEWPORT_LOGICAL",
                                           diagnostic,sizeof(diagnostic));
      if(nbytes==1 && diagnostic[0]=='1') {
        if(debug_log_enabled && n<=24)
          log_line("D3D7-VIEWPORT-DIAG: A/B logical 640x480 original viewport selected");
        return call_original_set_viewport_traced(self,view,n,"menu-logical-ab");
      }
    }
    if (InterlockedCompareExchange(&media_viewport_4x3_enabled,0,0)==1) {
      char line[228];
      /* A centered 4:3 viewport preserves the aspect of projected geometry.
       * Previous full-screen viewport stretched XYZ visuals to widescreen.
       * XYZRHW menu vertices are already transformed by scale_menu_vertices()
       * into exactly this physical rectangle and need no further offset.
       */
      physical.dwX=(DWORD)menu_map.left;
      physical.dwY=(DWORD)menu_map.top;
      physical.dwWidth=(DWORD)(menu_map.right-menu_map.left);
      physical.dwHeight=(DWORD)(menu_map.bottom-menu_map.top);
      if (n<=24) {
        snprintf(line,sizeof(line),
          "DISPLAY-MEDIA: D3D7 viewport logical=640x480 -> physical=(%lu,%lu) %lux%lu [4:3 ACTIVE]",
          (unsigned long)physical.dwX,(unsigned long)physical.dwY,
          (unsigned long)physical.dwWidth,(unsigned long)physical.dwHeight);
        log_line(line);
      }
    } else {
      physical.dwWidth=desktop_w;
      physical.dwHeight=desktop_h;
      if (n<=24) log_line("DISPLAY-TEST: menu D3D viewport 640x480 -> native monitor size (legacy)");
    }
    return call_original_set_viewport_traced(self,&physical,n,"menu-mapped");
  }
  return call_original_set_viewport_traced(self,view,n,"passthrough");
}
/* Forward-only draw diagnostics. Log only the first 16 calls per method. */
typedef HRESULT (WINAPI *DrawPrimitiveFn)(void *, D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, DWORD);
typedef HRESULT (WINAPI *DrawIndexedPrimitiveFn)(void *, D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, LPWORD, DWORD, DWORD);
static DrawPrimitiveFn original_draw_primitive;
static DrawIndexedPrimitiveFn original_draw_indexed_primitive;
static int geom_rva(void *ret,DWORD *rva) {
  uintptr_t base=(uintptr_t)GetModuleHandleA(NULL),addr=(uintptr_t)ret;
  IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)base;
  IMAGE_NT_HEADERS32 *nt;
  if(!base || !ret || dos->e_magic!=IMAGE_DOS_SIGNATURE) return 0;
  nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE || addr<base || addr-base>=nt->OptionalHeader.SizeOfImage) return 0;
  *rva=(DWORD)(addr-base);return 1;
}
/* The observed menu draw path is 0x144 = XYZRHW|DIFFUSE|TEX1.
 * Never mutate the engine's vertices; graphics drivers may retain pointers.
 * Use a small stack buffer for normal quads, a temporary heap for larger draws.
 */
static const void *scale_menu_vertices(DWORD fvf,const void *vertices,DWORD count,
                                        SpiderMenuVertex *small,DWORD small_count,
                                        SpiderMenuVertex **allocated) {
  SpiderMenuVertex *dst;
  LONG n;
  int menu_active=InterlockedCompareExchange(&menu_upscale_active,0,0)==1;
  int game_active=InterlockedCompareExchange(&gameplay_4x3_active,0,0)==1;
  const SpiderMenuMapping *mapping=menu_active?&menu_map:&game_map;
  *allocated=NULL;
  if(!menu_active && !game_active) return vertices;
  /* Native viewport clips the menu to logical 640x480.
   * Preserve vertices and upscale the composed frame during Flip. */
  if (!vertices || !count || fvf!=(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)) {
    if (debug_log_enabled && (n=InterlockedIncrement(&menu_unscaled_draws))<=4) {
      char line[176];
      snprintf(line,sizeof(line),"DISPLAY-%s: draw NOT scaled: fvf=0x%08lX count=%lu",
               menu_active?"MENU":"ASPECT-43",(unsigned long)fvf,(unsigned long)count);
      log_line(line);
    }
    return vertices;
  }
  if (count>65536u) return vertices;
  dst=small;
  if (count>small_count) {
    dst=(SpiderMenuVertex*)HeapAlloc(GetProcessHeap(),0,(SIZE_T)count*sizeof(*dst));
    if (!dst) return vertices;
    *allocated=dst;
  }
  if (!SpiderMenu_TransformVertices(mapping,(const SpiderMenuVertex*)vertices,dst,count)) {
    if (*allocated) { HeapFree(GetProcessHeap(),0,*allocated); *allocated=NULL; }
    return vertices;
  }
  if (debug_log_enabled && menu_active && (n=InterlockedIncrement(&menu_scaled_draws))==1)
    log_line("DISPLAY-TEST: menu DrawPrimitive screen vertices scaled to centered 4:3");
  if (debug_log_enabled && game_active && InterlockedIncrement(&gameplay_4x3_mapped)==1)
    log_line("DISPLAY-ASPECT-43: gameplay XYZRHW vertices mapped into physical 4:3 rectangle; RHW/Z/UV unchanged");
  return dst;
}
/* Signature guard: do not touch any non-matching sprites, videos,
 * menus, or gameplay. Detect at draw time (rather than cached texture pointer)
 * because DirectDraw may recycle its surfaces. GetTexture reference released. */
/* Native BD artwork has a different callsite/phase from the correct menu
 * title image. The original game-owned vertices remain untouched. */
static const void *maybe_correct_native_comic(void *device,D3DPRIMITIVETYPE type,
              DWORD fvf,const void *source,DWORD count,void *ret,
              SpiderMenuVertex *copy)
{
  DWORD rva=0;
  IDirectDrawSurface7 *texture=NULL;
  DDSURFACEDESC2 desc;
  HRESULT hr;
  LONG hit;
  if(InterlockedCompareExchange(&comic_native_aspect_enabled,0,0)!=1 ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)==1 ||
     !source || !copy || count!=4 || type!=D3DPT_TRIANGLEFAN ||
     fvf!=0x144UL || !geom_rva(ret,&rva) || rva!=0x00102C72UL ||
     desktop_w<640 || desktop_h<480) return source;
  hr=IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&texture);
  if(FAILED(hr) || !texture) return source;
  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  hr=IDirectDrawSurface7_GetSurfaceDesc(texture,&desc);
  IDirectDrawSurface7_Release(texture);
  if(FAILED(hr) || !SpiderComicNative_IsCandidate((const SpiderMenuVertex*)source,
       (unsigned)count,(unsigned)type,(unsigned long)fvf,(unsigned long)rva,
       (unsigned)desc.dwWidth,(unsigned)desc.dwHeight,
       (unsigned)desktop_w,(unsigned)desktop_h)) return source;
  /* Rendering only; never treat a candidate quad as native scene ownership. */
  SpiderQuickExitF12_ObserveComicRender(InterlockedCompareExchange(&flip_count,0,0),
                                        rva,desc.dwWidth,desc.dwHeight);
  if(!SpiderComicNative_Transform((const SpiderMenuVertex*)source,copy,
                (unsigned)count,(unsigned)desktop_w,(unsigned)desktop_h)) return source;
  InterlockedExchange(&f9_comic_last_draw_flip,
                      InterlockedCompareExchange(&flip_count,0,0));
  hit=InterlockedIncrement(&comic_native_aspect_matches);
  if(hit<=8 || hit%120==0) {
    char msg[286];
    snprintf(msg,sizeof(msg),
      "COMIC-NATIVE-FIX: #%ld stage=NATIVE rva=0x%08lX texture=%lux%lu "
      "x-before=(%.1f,%.1f) x-after=(%.1f,%.1f) y-unchanged [NATIVE-ESC-43]",
      (long)hit,(unsigned long)rva,(unsigned long)desc.dwWidth,(unsigned long)desc.dwHeight,
      (double)((const SpiderMenuVertex*)source)[0].x,
      (double)((const SpiderMenuVertex*)source)[1].x,
      (double)copy[0].x,(double)copy[1].x);
    log_line(msg);
  }
  return copy;
}
static const void *maybe_correct_horplus(D3DPRIMITIVETYPE type,DWORD fvf,
                   const void *source,DWORD count,SpiderMenuVertex *copy) {
  float scalar;
  if(!source || !copy || count<3 || count>64 ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL ||
     !SpiderFOV_GetHorPlusScalar(&scalar)) return source;
  /* A Z-buffer is not sufficient to recognize world polys: game HUD glyphs
   * also carry depth. Exclude recognized, SHA-pinned UI scopes instead.
   * Without the classifier, fail closed rather than move UI text.
   */
  if(InterlockedCompareExchange(&fov_horplus_scopes_ready,0,0)!=1)
    return source;
  if(SpiderHudWidescreen_InScope()) {
    InterlockedIncrement(&fov_horplus_skips_hud);
    return source;
  }
  if(InterlockedCompareExchange(&fov_z_state_known,0,0)!=1) {
    if(InterlockedIncrement(&fov_horplus_skips_unknown)==1)
      log_line("FOV-HOR+: ZENABLE unknown; skipping 3D compensation [TEST ONLY]");
    return source;
  }
  if(InterlockedCompareExchange(&fov_z_state_enabled,0,0)!=1) {
    InterlockedIncrement(&fov_horplus_skips_depth); return source;
  }
  if(!SpiderFOV_RestoreVertical((const SpiderMenuVertex*)source,copy,
                        (unsigned)count,scalar,
                        0.5f*(float)InterlockedCompareExchange(&fov_game_height,0,0))) {
    InterlockedIncrement(&fov_horplus_skips_shape); return source;
  }
  if(InterlockedIncrement(&fov_horplus_calls)==1)
    log_line("FOV-HOR+: first depth-tested 3D primitive Y compensated [TEST ONLY]");
  return copy;
}

/* Opt-in common 4:3 gameplay UI canvas, applied only to source
 * screen-space D3D7 quads. No per-level/texture/color HUD rule. */
static volatile LONG ui_canvas_trial_enabled=0;
/* Additional opt-in trial: record native ESC commands, composite at EndScene. */
static volatile LONG ui_compositor_trial_enabled=0;
static volatile LONG ui_compositor_native_backdrop_enabled=1; /* Source-verified outer margins only; centre mesh untouched. */
/* Preserve the native gameplay HUD brightness by replaying its verified
 * D3D7 screen-space sprites AFTER the translucent veil but BEFORE the
 * 4:3 spider and Pause menu. Independent A/B opt-out is available. */
static volatile LONG ui_compositor_hud_over_veil_enabled=0; /* opt-in only; field validation used OFF */
static volatile LONG ui_canvas_trial_hits=0;
static volatile LONG ui_canvas_wipe_edge_hits=0;
/* Final V25: only original V19 polygons and physical source-tooth bounds.
 * Former V6–V24 alternative mask renderers are retired.
 */
static volatile LONG ui_transition_bounds25_enabled=0,ui_transition_sync24_failed=0;
static SpiderNativeSync24Frame ui_transition_sync24_frame;
static SpiderNativeBounds25Frame ui_transition_bounds25_frame;
static DWORD ui_transition_sync24_flags=0u;
static unsigned ui_transition_hold22_scene_native=0u,ui_transition_hold22_scene_gutters=0u;
static unsigned ui_transition_hold22_last_active=0u;
static unsigned ui_transition_bounds25_frames=0u;
static float ui_transition_bounds25_prev_left=0.0f,ui_transition_bounds25_prev_right=0.0f;
static unsigned ui_transition_bounds25_have_prev=0u;
static float ui_transition_bounds25_last_left=0.0f,ui_transition_bounds25_last_right=0.0f;
static unsigned ui_transition_bounds25_last_fallback=0u;
static volatile LONG ui_transition_native43_draws=0,ui_transition_native43_native=0;
static volatile LONG ui_canvas_fullscreen_kept=0;
static const void *maybe_correct_unified_ui_canvas(
                   D3DPRIMITIVETYPE type,DWORD fvf,
                   const void *source,DWORD count,void *caller,
                   SpiderMenuVertex *copy)
{
  DWORD rva=0;
  LONG hit;
  char message[250];
  if(InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)!=1 ||
     !SpiderHudWidescreen_Installed() ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     InterlockedCompareExchange(&fov_game_height,0,0)<=0 ||
     !source || !copy || count!=4 ||
     !geom_rva(caller,&rva))
    return source;
  /* Generic presentation-surface guard. An original quad that spans
   * the display horizontally and covers the frame or an edge letterbox
   * band stays full width, outside the virtual 4:3 HUD canvas. */
  if(SpiderUICanvas_43IsFullScreenEffect(
        (const SpiderMenuVertex*)source,(unsigned)count,
        (unsigned)type,(uint32_t)fvf,(uint32_t)rva,
        (unsigned)desktop_w,(unsigned)desktop_h)) {
    hit=InterlockedIncrement(&ui_canvas_fullscreen_kept);
    if(debug_log_enabled && (hit<=8 || hit%360==0)) {
      const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
      snprintf(message,sizeof(message),
        "UI-CANVAS-FULLSCREEN: keep=%ld x0=%.1f y0=%.1f color=%08lX z=%.5f rhw=%.4f [PRESENTATION; ORIGINAL X]",
        (long)hit,(double)v[0].x,(double)v[0].y,
        (unsigned long)v[0].diffuse,
        (double)v[0].z,(double)v[0].rhw);
      log_line(message);
    }
    return source;
  }
  if(!SpiderUICanvas_43Transform(
        (const SpiderMenuVertex*)source,copy,
        (unsigned)count,(unsigned)type,(uint32_t)fvf,
        (uint32_t)rva,(unsigned)desktop_w,(unsigned)desktop_h))
    return source;
  if(SpiderUICanvas_43IsWipeEdgeMask(
        (const SpiderMenuVertex*)source,(unsigned)count,
        (unsigned)type,(uint32_t)fvf,(uint32_t)rva,
        (unsigned)desktop_w,(unsigned)desktop_h)) {
    LONG edge=InterlockedIncrement(&ui_canvas_wipe_edge_hits);
    if(debug_log_enabled && (edge<=20 || edge%360==0)) {
      const SpiderMenuVertex *a=(const SpiderMenuVertex*)source;
      snprintf(message,sizeof(message),
        "UI-CANVAS-WIPE-EDGE: hit=%ld source-x=%.1f mapped-x=%.1f black=z%.5f rhw=%.4f [CINEMATIC-EDGE]",
        (long)edge,(double)a[0].x,(double)copy[0].x,
        (double)a[0].z,(double)a[0].rhw);
      log_line(message);
    }
  }
  hit=InterlockedIncrement(&ui_canvas_trial_hits);
  if(debug_log_enabled && (hit<=12 || hit%480==0)) {
    const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
    snprintf(message,sizeof(message),
      "UI-CANVAS-43: draw=%ld caller=%08lX source-x=%.1f mapped-x=%.1f z=%.5f rhw=%.4f native-scope=%d [EXPERIMENT; SCREEN-SPACE-CANDIDATE]",
      (long)hit,(unsigned long)rva,
      (double)v[0].x,(double)copy[0].x,
      (double)v[0].z,(double)v[0].rhw,
      (int)SpiderHudWidescreen_InScope());
    log_line(message);
  }
  return copy;
}

/* Source-verified gameplay HUD quads only. Enabled by default unless
 * HUDWidescreenFix=0 is explicitly configured; always SHA-gated and a
 * no-op in original 4:3. No game-owned vertex memory is changed. */
static volatile LONG hud_adaptive_compass_count=0;
static volatile LONG hud_adaptive_instruction_count=0;
static volatile LONG hud_adaptive_boss_count=0;
static const void *maybe_correct_adaptive_hud(void *device,
                    D3DPRIMITIVETYPE type,DWORD fvf,
                    const void *source,DWORD count,void *ret,
                    SpiderMenuVertex *copy)
{
  IDirectDrawSurface7 *texture=NULL;
  DDSURFACEDESC2 desc;
  DWORD rva=0,texw=0,texh=0;
  int region;
  LONG hit;
  char message[236];
  if(!source || !copy || count!=4 || fvf!=0x144UL ||
     type!=D3DPT_TRIANGLEFAN || desktop_w<640 || desktop_h<480 ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&fov_game_height,0,0)<=0 ||
     !SpiderHudWidescreen_Installed() ||
     !SpiderHudAdaptive_MightMatch((const SpiderMenuVertex*)source) ||
     !geom_rva(ret,&rva) || rva!=0x00102C72UL)
    return source;
  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  if(SUCCEEDED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&texture)) && texture) {
    if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(texture,&desc))) {
      texw=(DWORD)desc.dwWidth;texh=(DWORD)desc.dwHeight;
    }
    IDirectDrawSurface7_Release(texture);
  }
  region=SpiderHudAdaptive_Transform((const SpiderMenuVertex*)source,copy,
                                     (unsigned)count,(unsigned)type,(uint32_t)fvf,
                                     (uint32_t)rva,(unsigned)texw,(unsigned)texh,
                                     (unsigned)desktop_w,(unsigned)desktop_h);
  if(region==SPIDER_HUD_ADAPTIVE_NONE)return source;
  hit=InterlockedIncrement(region==SPIDER_HUD_ADAPTIVE_COMPASS?
       &hud_adaptive_compass_count:
       region==SPIDER_HUD_ADAPTIVE_BOSS?
       &hud_adaptive_boss_count:&hud_adaptive_instruction_count);
  if(debug_log_enabled && (hit<=4 || hit%240==0)) {
    snprintf(message,sizeof(message),
       "HUD-ADAPTIVE: region=%s hit=%ld scale=%.5f tex=%lux%lu x-original=%.1f x-mapped=%.1f [EXPERIMENT]",
       region==SPIDER_HUD_ADAPTIVE_COMPASS?"compass":
       region==SPIDER_HUD_ADAPTIVE_BOSS?"boss":"instructions",
       (long)hit,(double)(4.0f*(float)desktop_h/(3.0f*(float)desktop_w)),
       (unsigned long)texw,(unsigned long)texh,
       (double)((const SpiderMenuVertex*)source)[0].x,(double)copy[0].x);
    log_line(message);
  }
  return copy;
}
/* Native ESC subfeatures are hidden HUD settings (default ON).
 * Each can be explicitly set to 0 in [HUD] for debugging. They
 * require the successfully installed *full* HUD correction.
 * Scopes-only Hor+ installs are not enough: HUDWidescreenFix=0
 * must disable ALL dependent native ESC geometry changes.
 */
static int native_esc43_hud_setting_enabled(const char *key,
                                            const char *legacy_env) {
  /* All native ESC/HUD aspect corrections are permanent when the validated
   * HUD native patch is installed; old INI/environment switches are inert. */
  (void)key;
  (void)legacy_env;
  return SpiderHudWidescreen_Installed()!=0;
}

/* Accept native widescreen outputs of any resolution, but only when
 * exact game-owned D3D7 caller/texture/layer signatures ALSO match.
 * The menu's 4:3 proportion is independent of pixel dimensions. */
static int native_esc43_supported_monitor(void) {
  return SpiderNativePause43_Widescreen((unsigned)desktop_w,
                                        (unsigned)desktop_h);
}
static float native_esc43_scale(void) {
  return SpiderNativePause43_Scale((unsigned)desktop_w,
                                   (unsigned)desktop_h);
}
static float native_esc43_left_gutter(void) {
  return ((float)desktop_w-(float)desktop_h*(4.0f/3.0f))*0.5f;
}

/* Native gameplay ESC 4:3 panel correction (HUD-controlled).
 * Evidence: 2026-10-24 Wine traces #737 and #741, two menu states.
 * Anchor on the unusually wide *untextured* purple ribbon's verified
 * screen rectangle plus Z/RHW. Only following 2D glyphs in that SAME
 * Flip are eligible, with their own strict depth and texture gates.
 * Never touch world/HUD vertices or alter any game-owned input/state.
 */
static volatile LONG native_pause43_enabled=0;
static volatile LONG native_pause43_banner_flip=-1;
/* Read-only probe family derived from verified source ribbon RHW, not
 * any game level or scene name. Separate per-family sample budgets allow
 * later ESC variants after earlier level samples are exhausted. */
static volatile LONG native_pause43_banner_probe_family=0;
static volatile LONG native_pause43_banner_hits=0;
static volatile LONG native_pause43_border_hits=0;
static volatile LONG native_pause43_banner_top=0;
static volatile LONG native_pause43_banner_bottom=0;
static volatile LONG native_pause43_glyph_hits=0;
/* Our original game's ESC/pause object at 0x682950 is often transient:
 * runtime logs show pause_ctx=0 while native D3D7 PAUSED menu text is
 * visibly drawn. The SHA-gated exact ribbon detector is a stronger
 * in-frame observation for the two already-validated widescreen modes.
 * Fail closed (no inferred pause) if this exact graphic was not drawn
 * in the current or immediately previous native Flip. */
static int native_pause43_visible_near_flip(LONG now_flip) {
  LONG ribbon_flip=InterlockedCompareExchange(&native_pause43_banner_flip,0,0);
  return native_esc43_supported_monitor() &&
    InterlockedCompareExchange(&menu_upscale_active,0,0)==0 &&
    InterlockedCompareExchange(&native_pause43_enabled,0,0)==1 &&
    ribbon_flip>=0 && now_flip>=ribbon_flip &&
    now_flip-ribbon_flip<=1;
}
static void native_pause43_init(void) {
  int enabled=native_esc43_hud_setting_enabled(
       "NativeESC4x3","SPIDERMAN2K1FIX_NATIVE_ESC_43");
  InterlockedExchange(&native_pause43_enabled,enabled?1:0);
  log_line(enabled?
    "PAUSE-NATIVE-43: 4:3 ribbon+borders+glyphs ON (HUDWidescreenFix master)" :
    "PAUSE-NATIVE-43: OFF (HUD master or [HUD] NativeESC4x3=0)");
}
static const void *maybe_correct_native_pause43(void *device,
            D3DPRIMITIVETYPE type,DWORD fvf,const void *source,
            DWORD count,void *ret,SpiderMenuVertex *copy) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  float left,right,top,bottom,aspect_scale,center;
  float z,rhw;
  DWORD rva=0,tw=0,th=0; unsigned i;
  LONG flip,hit,bt=0,bb=0;
  int ribbon,glyph=0,border=0;
  IDirectDrawSurface7 *tex=NULL;DDSURFACEDESC2 desc;
  if(InterlockedCompareExchange(&native_pause43_enabled,0,0)!=1 ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     !source || !copy || count!=4 || type!=D3DPT_TRIANGLEFAN ||
     fvf!=0x144UL || desktop_h<480 ||
     desktop_w*3<=desktop_h*4 ||
     !geom_rva(ret,&rva) || rva!=0x00102C72UL)
    return source;
  left=right=v[0].x;top=bottom=v[0].y;
  z=v[0].z;rhw=v[0].rhw;
  for(i=1;i<4;i++) {
    if(v[i].x<left)left=v[i].x;
    if(v[i].x>right)right=v[i].x;
    if(v[i].y<top)top=v[i].y;
    if(v[i].y>bottom)bottom=v[i].y;
  }
  flip=InterlockedCompareExchange(&flip_count,0,0);
  /* Banner draw #3166, width ~1.006*desktop_w, y~400..675
   * for native ESC and y~432..634 for QUIT confirmation.
   * The tolerance supports animation, NOT arbitrary full-width draws.
   */
  /* GE-Proton 11-12 / level-2 native ESC: same verified caller, Z,
   * untextured wide ribbon and geometry, but distinct RHW=8.9615.
   * Retain legacy RHW=11.176 exactly; never accept RHW in between.
   * Original 4:3 gameplay remains excluded at function entry. */
  ribbon=SpiderNativePause43_RibbonBounds(
       left,right,top,bottom,(unsigned)desktop_w,(unsigned)desktop_h) &&
       (SpiderNativePause43_RibbonLayer(z,rhw) ||
        (InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 &&
         SpiderNativePause43_CompositorRibbon(z,rhw,v[0].diffuse)));
  if(ribbon) {
    /* The banner has no texture. Recheck the live bound texture here
     * rather than relying solely on depth and broad geometry.
     */
    if(SUCCEEDED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex)) &&
       tex) { IDirectDrawSurface7_Release(tex);return source; }
    InterlockedExchange(&native_pause43_banner_probe_family,
       SpiderNativePause43_Abs(rhw-10.6651f)<0.060f?2:
       SpiderNativePause43_Abs(rhw-11.7760f)<0.060f?1:0);
    InterlockedExchange(&native_pause43_banner_top,(LONG)top);
    InterlockedExchange(&native_pause43_banner_bottom,(LONG)bottom);
    InterlockedExchange(&native_pause43_banner_flip,flip);
  } else {
    if(InterlockedCompareExchange(&native_pause43_banner_flip,0,0)!=flip)
      return source;
    /* The 8x8-textured border pieces immediately follow the untextured
     * banner in the *same Flip*: 2 horizontal strokes, 2 vertical caps,
     * 4 small corners. Validate texture, z/RHW, and shape relative to
     * the banner recorded above; draw indices are not stable.
     * Do not touch the separately matched 32x32 red selection icon.
     */
    bt=InterlockedCompareExchange(&native_pause43_banner_top,0,0);
    bb=InterlockedCompareExchange(&native_pause43_banner_bottom,0,0);
    /* Source-backed strict depth variants: old menu and level-2
     * shadow/foreground glyphs measured in the maintainer's log. */
    if(!SpiderNativePause43_TextLayer(z,rhw) &&
       !(InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 &&
         SpiderNativePause43_CompositorText(z,rhw)))
      return source;
    if(FAILED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex)) ||
       !tex)return source;
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(tex,&desc))) {
      tw=desc.dwWidth;th=desc.dwHeight;
    }
    IDirectDrawSurface7_Release(tex);
    /* Normalize old pixel-sized border tolerances for other output
     * resolutions. Preserve the two empirically verified 1920-wide
     * geometries unchanged, including their 16:10 variation. */
    if(tw==8 && th==8 &&
       bb>bt+55.0f*(desktop_w==1920?1.0f:(float)desktop_h/1080.0f) &&
       /* Borders share the verified ribbon depth, not arbitrary text
        * shadow layers; accept only the two exact ribbon signatures. */
       (SpiderNativePause43_RibbonLayer(z,rhw) ||
        (InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 &&
         SpiderNativePause43_Perspective(z,rhw,0.00122f)))) {
      float sx=(float)desktop_w/1920.0f;
      float sy=desktop_w==1920?1.0f:(float)desktop_h/1080.0f;
      int near_left=(left>=-42.0f*sx && left<=-12.0f*sx &&
                     right>=7.0f*sx && right<=38.0f*sx);
      int near_right=(left>=(float)desktop_w-17.0f*sx &&
                      left<=(float)desktop_w+13.0f*sx &&
                      right>=(float)desktop_w+16.0f*sx &&
                      right<=(float)desktop_w+70.0f*sx);
      int at_top=fabsf(top-((float)bt-4.5f*sy))<5.0f*sy;
      int at_bottom=fabsf(top-((float)bb-9.0f*sy))<5.0f*sy;
      int horizontal=(fabsf(left)<12.0f*sx &&
          fabsf(right-(float)desktop_w)<20.0f*sx &&
          bottom-top>=14.0f*sy && bottom-top<=24.0f*sy &&
          (at_top || fabsf(top-(float)bb)<5.0f*sy));
      int side=(near_left || near_right) &&
          fabsf((bottom-top)-((float)bb-(float)bt-9.0f*sy))<18.0f*sy &&
          fabsf(top-((float)bt+4.5f*sy))<8.0f*sy;
      int corner=(near_left || near_right) &&
          bottom-top>=18.0f*sy && bottom-top<=30.0f*sy &&
          (at_top || at_bottom);
      border=horizontal || side || corner;
    }
    if(!border) {
      /* Title: PAUSED (Y=171), REALLY QUIT? (Y=283).
       * Choices: main menu Y=436/508/580; confirmation Y=468/540.
       * Exclude instructions (Y=877), compass, gameplay HUD, and
       * the separately handled red spider selector. */
      if(!SpiderNativePause43_TextInZone(
             left,right,top,bottom,(float)desktop_w,(float)desktop_h) ||
         top<(float)desktop_h*0.13f ||
         bottom>(float)desktop_h*0.63f ||
         right-left<10.0f*((float)desktop_w/1920.0f) ||
         right-left>84.0f*((float)desktop_w/1920.0f) ||
         bottom-top<24.0f*(desktop_w==1920?1.0f:(float)desktop_h/1080.0f) ||
         bottom-top>83.0f*(desktop_w==1920?1.0f:(float)desktop_h/1080.0f))
        return source;
      glyph=(th==32 && (tw==16 || tw==32 || tw==64));
      if(!glyph)return source;
    }
  }
  center=(float)desktop_w*0.5f;
  aspect_scale=native_esc43_scale();
  memcpy(copy,v,4*sizeof(*copy));
  /* Dreamcast 1920x1080 reference (2026-11-04): native Pause outline
   * x≈223..1702 (1480px). Previous empirical 1439px calibration
   * artificially narrowed only the ribbon and its 8x8 border strokes:
   * Wine screenshot scrn0024.png showed x=241..1679 (1439px).
   * The original 8x8 edge stroke source quads (-22.5..15,
   * 1912.5..1950) mapped with the canonical centered 4:3 factor
   * produce x≈223..1703, matching Dreamcast without special offsets.
   * Text has already been verified with this exact same transform.
   * Do not alter the 3D scene, HUD, movie bars or native 4:3 output. */
  for(i=0;i<4;i++)
    copy[i].x=center+(v[i].x-center)*aspect_scale;
  if(ribbon) hit=InterlockedIncrement(&native_pause43_banner_hits);
  else if(border) hit=InterlockedIncrement(&native_pause43_border_hits);
  else hit=InterlockedIncrement(&native_pause43_glyph_hits);
  if(hit<=4 || (hit%600)==0) {
    char line[230];
    snprintf(line,sizeof(line),
      "PAUSE-NATIVE-43: %s hit=%ld flip=%ld x=(%.1f..%.1f) scale=%.5f tex=%lux%lu z=%.5f rhw=%.4f [NATIVE-ESC-43]",
      ribbon?"ribbon":border?"border":"glyph",(long)hit,(long)flip,
      (double)left,(double)right,(double)aspect_scale,
      (unsigned long)tw,(unsigned long)th,
      (double)z,(double)rhw);
    log_line(line);
  }
  return copy;
}
/* Native ESC translucent geometric motif correction.
 * Pixel evidence: large orange/blue translucent pattern is absent in
 * normal gameplay (F9 scrn0004), appears in ESC (scrn0005) and native
 * QUIT confirmation (scrn0006). Unlike projected world triangles at
 * caller 0x00103389 (varying Z/RHW and textured), these quads have:
 * caller 0x00102C72; fan/FVF 0x144/4 verts; *untextured*;
 * Z=0.00146, RHW=9.988, vertex diffuse ALPHA=0x80.
 * The same geometry recurs in ESC and confirmation frames before the
 * purple ribbon, so cannot use the ribbon's current-frame marker.
 * HUD-controlled by default; 1920x1080 Wine-validated, 1920x1200
 * Windows verification pending.
 */
static volatile LONG native_motif43_enabled=0;
static volatile LONG native_motif43_hits=0;
static void native_motif43_init(void) {
  int enabled=InterlockedCompareExchange(&native_pause43_enabled,0,0)==1 &&
    native_esc43_hud_setting_enabled(
      "NativeMotif4x3","SPIDERMAN2K1FIX_NATIVE_MOTIF_43");
  InterlockedExchange(&native_motif43_enabled,enabled?1:0);
  log_line(enabled?
    "NATIVE-MOTIF-43: 4:3 motif + full-screen original tint ON (HUD master)" :
    "NATIVE-MOTIF-43: OFF (HUD master, NativeESC4x3 or NativeMotif4x3=0)");
}
static const void *maybe_correct_native_motif43(void *device,
    D3DPRIMITIVETYPE type,DWORD fvf,const void *source,DWORD count,
    void *ret,SpiderMenuVertex *copy) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  IDirectDrawSurface7 *tex=NULL;
  DWORD rva=0,color=0;
  LONG hit;
  float l,r,t,b,center;
  unsigned i;
  if(InterlockedCompareExchange(&native_motif43_enabled,0,0)!=1 ||
     !source || !copy || !device ||
     !native_esc43_supported_monitor() ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4 ||
     !geom_rva(ret,&rva) || rva!=0x00102C72UL ||
     /* Legacy verified depth remains intact. When the native ESC
      * ribbon has actually been observed in the current/preceding
      * Flip, the compositor also recognizes new levels through Z,
      * original alpha, geometry and untextured material. RHW may vary
      * continuously, exactly as for the validated target reticles. */
     !(SpiderNativePause43_MotifLayer(v[0].z,v[0].rhw) ||
       (InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 &&
        native_pause43_visible_near_flip(
          InterlockedCompareExchange(&flip_count,0,0)) &&
        SpiderNativePause43_CompositorMotif(
            v[0].z,v[0].rhw,v[0].diffuse)))) return source;
  memcpy(&color,(const BYTE*)source+16,sizeof(color));
  if((color&0xFF000000UL)!=0x80000000UL)return source;
  l=r=v[0].x;t=b=v[0].y;
  for(i=1;i<4;i++) {
    DWORD vertex_color=0;
    memcpy(&vertex_color,(const BYTE*)source+(SIZE_T)i*28u+16u,
           sizeof(vertex_color));
    if((vertex_color&0xFF000000UL)!=0x80000000UL ||
       fabsf(v[i].z-v[0].z)>0.00005f ||
       fabsf(v[i].rhw-v[0].rhw)>0.080f)return source;
    if(v[i].x<l)l=v[i].x;
    if(v[i].x>r)r=v[i].x;
    if(v[i].y<t)t=v[i].y;
    if(v[i].y>b)b=v[i].y;
  }
  /* Do not modify full-frame darkening or thin HUD/line overlays.
   * Bounds may extend beyond the viewport for polygons at screen edges,
   * but should have meaningful visible area. */
  if(r-l<16.0f || b-t<16.0f ||
     (r-l)>(float)desktop_w*0.80f ||
     (b-t)>(float)desktop_h*0.86f ||
     r<0.0f || l>(float)desktop_w ||
     b<0.0f || t>(float)desktop_h)return source;
  /* A stale texture binding on any pass MUST NOT be treated as an
   * untextured motif. COM-owned reference must be released. */
  if(SUCCEEDED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex))
     && tex) {
    IDirectDrawSurface7_Release(tex);
    return source;
  }
  /* The translucent ESC motif is a screen-covering presentation layer,
   * NOT a HUD sprite. In the unified canvas trial keep the source
   * physical X positions so tint / graphics reach the monitor edges.
   * Return a distinct copy to consume the draw in this stage and stop
   * the general canvas from compressing it a second time. No gutter
   * replay in this path: the original full-width quads draw once. */
  if(InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)==1 &&
     InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)!=1) {
    memcpy(copy,v,4*sizeof(*copy));
    hit=InterlockedIncrement(&native_motif43_hits);
    if(debug_log_enabled && (hit<=8 || hit%8000==0)) {
      char line[230];
      snprintf(line,sizeof(line),
        "NATIVE-MOTIF-FULLWIDTH: original X preserved hit=%ld rect=(%.1f..%.1f,%.1f..%.1f) color=%08lX [PRESENTATION, NOT HUD]",
        (long)hit,(double)l,(double)r,(double)t,(double)b,
        (unsigned long)color);
      log_line(line);
    }
    return copy;
  }
  center=(float)desktop_w*0.5f;
  memcpy(copy,v,4*sizeof(*copy));
  for(i=0;i<4;i++)
    copy[i].x=center+(v[i].x-center)*native_esc43_scale();
  hit=InterlockedIncrement(&native_motif43_hits);
  if(hit<=12 || hit%800==0) {
    char line[260];
    snprintf(line,sizeof(line),
      "NATIVE-MOTIF-43: QUAD-MATCH hit=%ld x=(%.1f..%.1f) y=(%.1f..%.1f) diffuse=%08lX z=%.5f rhw=%.3f [X-ONLY; NO WORLD OR FULLSCREEN]",
      (long)hit,(double)l,(double)r,(double)t,(double)b,
      (unsigned long)color,(double)v[0].z,(double)v[0].rhw);
    log_line(line);
  }
  return copy;
}
/* Preserve native full-width scene tint in the two 240px gutters.
 * The same 4:3 geometry compression would narrow native full-screen
 * violet dimming. Preserve its coverage outside the central 4:3 area.
 *
 * Replay the ORIGINAL native 2D quads, clipped STRICTLY to the gutters,
 * using the game's own color/blend state. Then draw the 4:3-corrected
 * motif in the center. Replaying is safer than guessing a purple ARGB
 * layer: the native tint is not a fixed additive color.
 *
 * Split each triangle fan into its original two triangles BEFORE clipping.
 * The original vertex order, Z, RHW, diffuse gradient, UV and state are
 * preserved. Never affect native gameplay world geometry, HUD, cursor,
 * purple ribbon or menu labels.
 */
static volatile LONG native_motif43_gutter_hits=0;
static SpiderMenuVertex native_motif43_interpolate(
    const SpiderMenuVertex *a,const SpiderMenuVertex *b,float t,float x) {
  SpiderMenuVertex r=*a;
  DWORD ca=0,cb=0,c=0;
  float uv_a=0,uv_b=0,uv=0;
  unsigned j;
  if(t<0.0f)t=0.0f;
  if(t>1.0f)t=1.0f;
  r.x=x;
  r.y=a->y+(b->y-a->y)*t;
  r.z=a->z+(b->z-a->z)*t;
  r.rhw=a->rhw+(b->rhw-a->rhw)*t;
  memcpy(&ca,(const BYTE*)a+16u,sizeof(ca));
  memcpy(&cb,(const BYTE*)b+16u,sizeof(cb));
  for(j=0;j<4;j++) {
    unsigned a8=(ca>>(j*8u))&255u,b8=(cb>>(j*8u))&255u;
    unsigned mixed=(unsigned)((float)a8+((float)b8-(float)a8)*t+0.5f);
    if(mixed>255u)mixed=255u;
    c|=(DWORD)(mixed<<(j*8u));
  }
  memcpy((BYTE*)&r+16u,&c,sizeof(c));
  for(j=0;j<2;j++) {
    size_t offs=20u+j*4u;
    memcpy(&uv_a,(const BYTE*)a+offs,sizeof(uv_a));
    memcpy(&uv_b,(const BYTE*)b+offs,sizeof(uv_b));
    uv=uv_a+(uv_b-uv_a)*t;
    memcpy((BYTE*)&r+offs,&uv,sizeof(uv));
  }
  return r;
}
static unsigned native_motif43_clip_triangle(
    const SpiderMenuVertex tri[3],float edge,int keep_left,
    SpiderMenuVertex out[5]) {
  unsigned i,n=0;
  for(i=0;i<3;i++) {
    const SpiderMenuVertex *a=&tri[(i+2u)%3u],*b=&tri[i];
    int inside_a=keep_left ? a->x<=edge : a->x>=edge;
    int inside_b=keep_left ? b->x<=edge : b->x>=edge;
    if(inside_a!=inside_b) {
      float denom=b->x-a->x;
      if(fabsf(denom)<0.00001f)return 0u;
      if(n>=5u)return 0u;
      out[n++]=native_motif43_interpolate(a,b,(edge-a->x)/denom,edge);
    }
    if(inside_b) {
      if(n>=5u)return 0u;
      out[n++]=*b;
    }
  }
  return n;
}
static void native_motif43_restore_gutters(void *device,
    const SpiderMenuVertex *original,DWORD flags) {
  float left_edge,right_edge;
  float left,right;
  SpiderMenuVertex tri[3],out[5];
  unsigned i,n;
  LONG hit;
  int gutter,any=0;
  if(!original || !device || !native_esc43_supported_monitor())return;
  left_edge=native_esc43_left_gutter();
  right_edge=(float)desktop_w-left_edge;
  if(left_edge<=0.0f || right_edge<left_edge)return;
  left=right=original[0].x;
  for(i=1;i<4;i++) {
    if(original[i].x<left)left=original[i].x;
    if(original[i].x>right)right=original[i].x;
  }
  if(left>=left_edge && right<=right_edge)return;
  for(i=0;i<2;i++) {
    tri[0]=original[0];tri[1]=original[i+1u];tri[2]=original[i+2u];
    for(gutter=0;gutter<2;gutter++) {
      float bound=(gutter==0)?left_edge:right_edge;
      if((gutter==0 && left>=bound) ||
         (gutter==1 && right<=bound)) continue;
      n=native_motif43_clip_triangle(tri,bound,gutter==0,out);
      if(n>=3u) {
        /* Direct call bypasses our hook, preserves original render state.
         * A clipped triangle fan is at most four vertices. */
        original_draw_primitive(device,D3DPT_TRIANGLEFAN,0x144UL,
                               (LPVOID)out,(DWORD)n,flags);
        any=1;
      }
    }
  }
  if(any) {
    hit=InterlockedIncrement(&native_motif43_gutter_hits);
    if(hit<=8 || hit%10000==0) {
      char line[240];
      snprintf(line,sizeof(line),
        "NATIVE-MOTIF-43: GUTTER-REPLAY hit=%ld source-x=(%.1f..%.1f) clip=(0..%.1f,%.1f..%lu) [ORIGINAL COLOR/BLEND; CENTER UNTOUCHED]",
        (long)hit,(double)left,(double)right,
        (double)left_edge,(double)right_edge,(unsigned long)desktop_w);
      log_line(line);
    }
  }
}
/* Native gameplay ESC red selector: strict owner/texture signature.
 * A 32x32 texture in an exact screen quad (96x72), z=0, rhw=27.519,
 * diffuse white, native D3D7 caller 0x00102C72 and validated purple
 * ESC banner drawn in the SAME Flip. This is independent of the
 * the original game menu or ordinary gameplay HUD.
 *
 * Enabled by default through HUDWidescreenFix, unless manually disabled in
 * [HUD] NativeCursor4x3=0. Match the validated
 * signature instead of ephemeral texture pointer identities. Limit this
 * strictly to the Wine-tested 1920x1080 mode. Only X vertex coordinates
 * are modified; keep Y, Z, RHW, UV, texture, game input unchanged.
 */
static volatile LONG native_cursor43_enabled=0;
static volatile LONG native_cursor43_hits=0;
static void native_cursor43_init(void) {
  int enabled=InterlockedCompareExchange(&native_pause43_enabled,0,0)==1 &&
    native_esc43_hud_setting_enabled(
      "NativeCursor4x3","SPIDERMAN2K1FIX_NATIVE_CURSOR_43");
  InterlockedExchange(&native_cursor43_enabled,enabled?1:0);
  log_line(enabled?
    "NATIVE-CURSOR-43: 4:3 selector ON (any widescreen; signature-gated)" :
    "NATIVE-CURSOR-43: OFF (HUD master, NativeESC4x3 or NativeCursor4x3=0)");
}
static const void *maybe_correct_native_cursor43(void *device,
    D3DPRIMITIVETYPE type,DWORD fvf,const void *source,DWORD count,
    void *ret,SpiderMenuVertex *copy) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  float left,right,top,bottom,cx,z,rhw,expected_width,expected_height,icon_scale;
  DWORD rva=0,diffuse=0;
  IDirectDrawSurface7 *tex=NULL;
  DDSURFACEDESC2 desc;
  LONG flip,hits;
  unsigned i;
  HRESULT hr;
  if(InterlockedCompareExchange(&native_cursor43_enabled,0,0)!=1 ||
     !source || !copy || !device ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4 ||
     !native_esc43_supported_monitor() ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     !geom_rva(ret,&rva) || rva!=0x00102C72UL)return source;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  if(InterlockedCompareExchange(&native_pause43_banner_flip,0,0)!=flip)
    return source;
  left=right=v[0].x;top=bottom=v[0].y;
  z=v[0].z;rhw=v[0].rhw;
  for(i=1;i<4;i++) {
    if(v[i].x<left)left=v[i].x;
    if(v[i].x>right)right=v[i].x;
    if(v[i].y<top)top=v[i].y;
    if(v[i].y>bottom)bottom=v[i].y;
  }
  /* The selector can legitimately move anywhere on the supported
   * widescreen desktop. Former position gates (left>=450, right<=1480,
   * top>=325, bottom<=800) made the 96x72 sprite revert to its
   * 16:9-stretched aspect near the top and right: observed F9
   * scrn0011=72px, scrn0012 (y<325)=96px,
   * scrn0013 (right>1480)=96px. Do NOT constrain its screen position.
   * Match its stable local 96x72 size, near-foreground Z/RHW,
   * original 32x32 texture format, white diffuse, exact native
   * draw call and active ESC ribbon in this same frame instead.
   * Native font glyphs are different sizes/Z/RHW. */
  /* Allow two possible original height policies at any output size:
   * scaled by screen H, or legacy height scaled by screen W. The same
   * exact owner, Z/RHW, diffuse, texture and same-Flip ribbon guards
   * remain mandatory. Center of the selector never moves. */
  expected_width=96.0f*((float)desktop_w/1920.0f);
  expected_height=72.0f*((float)desktop_h/1080.0f);
  if(fabsf((right-left)-expected_width)>0.8f ||
     (fabsf((bottom-top)-expected_height)>1.1f &&
      fabsf((bottom-top)-72.0f*((float)desktop_w/1920.0f))>0.8f) ||
     fabsf(z)>0.00005f ||
     fabsf(rhw-27.519f)>0.035f)return source;
  memcpy(&diffuse,(const BYTE*)source+16,sizeof(diffuse));
  if(diffuse!=0xFFFFFFFFUL)return source;
  /* Validate this actual bound texture, not just the 32x32
   * candidates' screen positions; do not capture a run-specific
   * pointer value from logs. No texture content is modified. */
  hr=IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex);
  if(FAILED(hr) || !tex)return source;
  memset(&desc,0,sizeof(desc));
  desc.dwSize=sizeof(desc);
  hr=IDirectDrawSurface7_GetSurfaceDesc(tex,&desc);
  IDirectDrawSurface7_Release(tex);
  if(FAILED(hr) || desc.dwWidth!=32 || desc.dwHeight!=32 ||
     desc.ddpfPixelFormat.dwFlags!=0x41UL)return source;
  cx=(left+right)*0.5f;
  icon_scale=(bottom-top)/(right-left);
  memcpy(copy,v,4*sizeof(*copy));
  for(i=0;i<4;i++)
    copy[i].x=cx+icon_scale*(v[i].x-cx);
  hits=InterlockedIncrement(&native_cursor43_hits);
  if(hits<=8 || hits%600==0) {
    char line[230];
    snprintf(line,sizeof(line),
      "NATIVE-CURSOR-43: QUAD-MATCH hit=%ld flip=%ld source=(%.0f..%.0f,%.0f..%.0f) output-width=%.1f scale=%.5f z=%.5f rhw=%.3f [X-ONLY]",
      (long)hits,(long)flip,(double)left,(double)right,
      (double)top,(double)bottom,(double)(right-left)*icon_scale,
      (double)icon_scale,(double)z,(double)rhw);
    log_line(line);
  }
  return copy;
}
/* The original game draws red and green aiming rings through the same
 * general-purpose D3D7 caller used by many other sprites. The maintainer's
 * F9-matched Log=1 trace supplies an unambiguous COM texture, tint, Z,
 * RHW and aspect signature for each ring. No generic HUD compression.
 * This is not an INI feature: it follows the existing installed
 * SHA-verified HUDWidescreenFix master switch.
 */
static volatile LONG hud_aim_red_hits=0;
static volatile LONG hud_aim_green_hits=0;
static const void *maybe_correct_aim_reticle(
       void *device,D3DPRIMITIVETYPE type,DWORD fvf,
       const void *source,DWORD count,void *caller,
       SpiderMenuVertex *copy)
{
  DWORD rva=0,tw=0,th=0;
  IDirectDrawSurface7 *texture=NULL;
  DDSURFACEDESC2 desc;
  int kind;
  LONG hit;
  char message[270];
  if(!device || !source || !copy || count!=4 ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     InterlockedCompareExchange(&fov_game_height,0,0)<=0 ||
     !SpiderHudWidescreen_Installed() ||
     !SpiderHudAimReticle_MightMatch((const SpiderMenuVertex*)source) ||
     !geom_rva(caller,&rva) || rva!=0x00102C72UL)
    return source;

  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  if(SUCCEEDED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&texture))
     && texture) {
    if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(texture,&desc))) {
      tw=desc.dwWidth;th=desc.dwHeight;
    }
    IDirectDrawSurface7_Release(texture);
  }
  if(tw!=64u || th!=64u) return source;
  kind=SpiderHudAimReticle_Transform(
        (const SpiderMenuVertex*)source,copy,(unsigned)count,
        (unsigned)type,(uint32_t)fvf,(uint32_t)rva,
        (unsigned)tw,(unsigned)th,
        (unsigned)desktop_w,(unsigned)desktop_h);
  if(kind==SPIDER_HUD_AIM_NONE)return source;
  hit=InterlockedIncrement(kind==SPIDER_HUD_AIM_RED?
                           &hud_aim_red_hits:&hud_aim_green_hits);
  if(debug_log_enabled && (hit<=4 || hit%240==0)) {
    const SpiderMenuVertex *original=(const SpiderMenuVertex*)source;
    snprintf(message,sizeof(message),
       "HUD-AIM: %s ring hit=%ld x=(%.1f..%.1f) -> (%.1f..%.1f) scale=%.5f z=%.5f rhw=%.4f tex=64x64 [SOURCE-VERIFIED]",
       kind==SPIDER_HUD_AIM_RED?"red":"green",(long)hit,
       (double)original[0].x,(double)original[1].x,
       (double)copy[0].x,(double)copy[1].x,
       (double)SpiderHudAim_AspectFactor((unsigned)desktop_w,
                                         (unsigned)desktop_h),
       (double)original[0].z,(double)original[0].rhw);
    log_line(message);
  }
  return copy;
}

/* Boss bars require their own original D3D7 source evidence.
 * We have a Rhino 32x32 border sample at RHW=17.1292 and a Dreamcast
 * screenshot proving the full bar and portrait need a 4:3 correction.
 * The existing Venom/Scorpion boss fix expects *different* RHW ranges;
 * do not force any bar/portrait geometry without separately verifying
 * the original tint, texture, size and depth of each source layer.
 *
 * This probe never changes vertices, input or render state. It samples
 * at most 18 events of each boss kind, one every 45 native Flips. */
/* Rhino-style boss bar/portrait: ALL three confirmed original D3D7
 * layers map around the SCREEN center, not their individual centers.
 * The observed caller, texture, color, z and RHW distinguish these
 * objects from earlier Venom/Scorpion UI and game-world geometry.
 * Fail closed for unknown black HP mask depths; source vertices intact.
 */
static volatile LONG boss_hud_native_hits[3]={0,0,0};
static const void *maybe_correct_native_boss_hud(void *device,
       D3DPRIMITIVETYPE type,DWORD fvf,const void *source,
       DWORD count,void *caller,SpiderMenuVertex *copy)
{
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  IDirectDrawSurface7 *texture=NULL;
  DDSURFACEDESC2 desc;
  DWORD rva=0,tw=0,th=0;
  HRESULT hr;
  int kind;
  LONG hit;
  char line[300];
  if(!device || !v || !copy || count!=4u ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL ||
     !SpiderHudWidescreen_Installed() ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     !SpiderHudBossNative_MightMatch(v) ||
     !geom_rva(caller,&rva) || rva!=0x00102C72UL)
    return source;

  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  hr=IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&texture);
  if(FAILED(hr)) return source; /* unknown texture is not no texture */
  if(texture){
    hr=IDirectDrawSurface7_GetSurfaceDesc(texture,&desc);
    if(SUCCEEDED(hr)){tw=desc.dwWidth;th=desc.dwHeight;}
    IDirectDrawSurface7_Release(texture);
    if(FAILED(hr))return source;
  }
  kind=SpiderHudBossNative_Transform(v,copy,(unsigned)count,
       (unsigned)type,(uint32_t)fvf,(uint32_t)rva,
       (unsigned)tw,(unsigned)th,
       (unsigned)desktop_w,(unsigned)desktop_h);
  if(kind==SPIDER_HUD_BOSS_PROBE_NONE)return source;
  hit=InterlockedIncrement(&boss_hud_native_hits[kind-1]);
  if(debug_log_enabled && (hit<=4 || hit%240==0)){
    float scale=(4.0f*(float)desktop_h)/(3.0f*(float)desktop_w);
    snprintf(line,sizeof(line),
      "HUD-BOSS-NATIVE: %s hit=%ld flip=%ld x=(%.1f..%.1f) -> (%.1f..%.1f) factor=%.5f z=%.5f rhw=%.4f texture=%lux%lu [ORIGINAL-GEOMETRY]",
      kind==SPIDER_HUD_BOSS_PROBE_FILL?"fill":
      kind==SPIDER_HUD_BOSS_PROBE_FRAME?"frame":"portrait",
      (long)hit,(long)InterlockedCompareExchange(&flip_count,0,0),
      (double)v[0].x,(double)v[1].x,
      (double)copy[0].x,(double)copy[1].x,
      (double)scale,(double)v[0].z,(double)v[0].rhw,
      (unsigned long)tw,(unsigned long)th);
    log_line(line);
  }
  return copy;
}
/* Track the three source bar appearances separately; sampling only the
 * first fill each frame hides the green half and the black damage mask. */
static volatile LONG boss_hud_probe_last_flip[5]={-100,-100,-100,-100,-100};
static volatile LONG boss_hud_probe_hits[5]={0,0,0,0,0};
static void maybe_log_boss_hud_candidate(void *device,
       D3DPRIMITIVETYPE type,DWORD fvf,const void *source,
       DWORD count,void *caller,int already_corrected)
{
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  IDirectDrawSurface7 *texture=NULL;
  DDSURFACEDESC2 desc;
  DWORD rva=0,tw=0,th=0;
  float l,r,t,b;
  unsigned i;
  int kind,k;
  LONG flip,last,hit;
  char message[340];
  if(!debug_log_enabled || !device || !source || count!=4u ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL ||
     !SpiderHudWidescreen_Installed() ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     !geom_rva(caller,&rva) || rva!=0x00102C72UL)
    return;

  l=r=v[0].x;t=b=v[0].y;
  for(i=1;i<4u;i++){
    if(v[i].x<l)l=v[i].x;
    if(v[i].x>r)r=v[i].x;
    if(v[i].y<t)t=v[i].y;
    if(v[i].y>b)b=v[i].y;
  }
  if(!(l>=(float)desktop_w*0.46f &&
       r<=(float)desktop_w+64.0f &&
       t>=(float)desktop_h*0.045f &&
       b<=(float)desktop_h*0.36f &&
       r-l>16.0f &&
       b-t>=(float)desktop_h*0.020f))
    return;

  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  if(SUCCEEDED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,
                                            0,&texture)) && texture){
    if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(texture,&desc))){
      tw=desc.dwWidth;th=desc.dwHeight;
    }
    IDirectDrawSurface7_Release(texture);
  }
  kind=SpiderHudBossProbe_Classify(v,(unsigned)count,(unsigned)type,
                (uint32_t)fvf,(uint32_t)rva,(unsigned)tw,(unsigned)th,
                (unsigned)desktop_w,(unsigned)desktop_h);
  if(kind==SPIDER_HUD_BOSS_PROBE_NONE)return;
  if(kind==SPIDER_HUD_BOSS_PROBE_FILL){
    /* Keep black missing HP, green full HP, and red/yellow color
     * gradient independently visible; the appearance can vary as
     * the boss takes damage. This changes diagnostics only. */
    k=(v[0].diffuse==0xFF000000u)?0:
      (((v[0].diffuse&0x00FF0000u)<0x00800000u &&
        (v[0].diffuse&0x0000FF00u)>=0x00008000u)?1:2);
  }else k=(kind==SPIDER_HUD_BOSS_PROBE_FRAME)?3:4;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  last=InterlockedCompareExchange(&boss_hud_probe_last_flip[k],0,0);
  if(flip-last<45 ||
     InterlockedCompareExchange(&boss_hud_probe_hits[k],0,0)>=18)
    return;
  InterlockedExchange(&boss_hud_probe_last_flip[k],flip);
  hit=InterlockedIncrement(&boss_hud_probe_hits[k]);
  if(hit>18)return;
  snprintf(message,sizeof(message),
      "HUD-BOSS-PROBE: %s sample=%ld flip=%ld corrected=%d caller=%08lX quad=(%.1f..%.1f,%.1f..%.1f) z=%.5f rhw=%.4f color=%08lX tex=%lux%lu uv0=(%.3f,%.3f) [READ-ONLY]",
      kind==SPIDER_HUD_BOSS_PROBE_FILL?
          (k==0?"fill-black":k==1?"fill-green":"fill-other"):
      kind==SPIDER_HUD_BOSS_PROBE_FRAME?"frame":"portrait",
      (long)hit,(long)flip,already_corrected,
      (unsigned long)rva,
      (double)l,(double)r,(double)t,(double)b,
      (double)v[0].z,(double)v[0].rhw,
      (unsigned long)v[0].diffuse,
      (unsigned long)tw,(unsigned long)th,
      (double)v[0].u,(double)v[0].v);
  log_line(message);
}
/* Read-only HUD-visual diagnostics while [Debug] Log=1.
 * Trace only representative UNCORRECTED source quads, never change draw
 * state, game input, texture contents or original vertex memory.
 * The pure heuristic is a CANDIDATE sampler, not an object matcher:
 * collected caller/Z/RHW/color/texture evidence gates future fixes. */
static volatile LONG hud_visual_aim_last_flip=-500;
static volatile LONG hud_visual_pause_last_flip=-500;
static volatile LONG hud_visual_aim_samples=0;
static volatile LONG hud_visual_pause_samples=0;
static void maybe_log_hud_visual_candidate(void *device,
                 D3DPRIMITIVETYPE type,DWORD fvf,const void *source,
                 DWORD count,void *caller) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  IDirectDrawSurface7 *tex=NULL;
  DDSURFACEDESC2 desc;
  DWORD rva=0,texw=0,texh=0;
  float left,right,top,bottom;
  volatile LONG *last_flip,*sample_count;
  LONG flip,last,hit;
  int candidate;
  unsigned i;
  char line[360];
  if(!debug_log_enabled || !device || !v ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     !SpiderHudWidescreen_Installed())return;
  candidate=SpiderHudVisual_Classify(v,(unsigned)count,
               (unsigned)type,(uint32_t)fvf,
               (unsigned)desktop_w,(unsigned)desktop_h);
  if(candidate==SPIDER_HUD_VISUAL_NONE)return;
  last_flip=candidate==SPIDER_HUD_VISUAL_AIM?
            &hud_visual_aim_last_flip:&hud_visual_pause_last_flip;
  sample_count=candidate==SPIDER_HUD_VISUAL_AIM?
               &hud_visual_aim_samples:&hud_visual_pause_samples;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  last=InterlockedCompareExchange(last_flip,0,0);
  /* Sampling is intentionally bounded to avoid flooding the player's log.
   * Aim: one sample / 45 Flips, maximum 200. Native pause: one / 90,
   * maximum 80. Keep the two budgets independent across level transitions. */
  if(flip-last < (candidate==SPIDER_HUD_VISUAL_AIM?45:90))return;
  hit=InterlockedIncrement(sample_count);
  if(hit>(candidate==SPIDER_HUD_VISUAL_AIM?200:80))return;
  InterlockedExchange(last_flip,flip);
  left=right=v[0].x;top=bottom=v[0].y;
  for(i=1;i<4;i++) {
    if(v[i].x<left)left=v[i].x;
    if(v[i].x>right)right=v[i].x;
    if(v[i].y<top)top=v[i].y;
    if(v[i].y>bottom)bottom=v[i].y;
  }
  /* Diagnostic-only COM reference obtained from current texture stage.
   * Never call SetTexture or any game-owned state mutator. */
  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  if(SUCCEEDED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex)) && tex) {
    if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(tex,&desc))) {
      texw=desc.dwWidth;texh=desc.dwHeight;
    }
    IDirectDrawSurface7_Release(tex);
  }
  geom_rva(caller,&rva);
  snprintf(line,sizeof(line),
    "HUD-VISUAL: %s sample=%ld flip=%ld caller=%08lX quad=(%.1f..%.1f,%.1f..%.1f) z=%.5f rhw=%.4f color=%08lX tex=%lux%lu uv0=(%.3f,%.3f) [READ-ONLY; CANDIDATE]",
    candidate==SPIDER_HUD_VISUAL_AIM?"AIM":"PAUSE-RIBBON",
    (long)hit,(long)flip,(unsigned long)rva,
    (double)left,(double)right,(double)top,(double)bottom,
    (double)v[0].z,(double)v[0].rhw,
    (unsigned long)v[0].diffuse,(unsigned long)texw,(unsigned long)texh,
    (double)v[0].u,(double)v[0].v);
  log_line(line);
}
/* Snapshot the unmodified game-owned ESC text/border/selector quads after
 * a verified native ribbon was seen, including quads rejected by our
 * existing exact layer and format gates. This is a read-only investigation:
 * no pointer scanning, texture modification, DrawPrimitive recursion,
 * render-state changes or unbounded per-frame logging. */
static volatile LONG native_pause43_detail_samples=0;
/* The original 80 samples in the later Mysterio scene ended before
 * the violet foreground choices were drawn; keep family=0 at 80,
 * but allow 160 bounded read-only family=1 samples to capture both
 * red heading and all choice colors in a single user test. */
static volatile LONG native_pause43_detail_new_family_samples=0;
/* Independent 160-draw read-only budget for the newly logged
 * Venom (2) original ESC ribbon family. Preserve all other caps. */
static volatile LONG native_pause43_detail_venom2_samples=0;
static void maybe_log_native_pause43_detail(void *device,
                  D3DPRIMITIVETYPE type,DWORD fvf,const void *source,
                  DWORD count,void *caller,int corrected) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  IDirectDrawSurface7 *tex=NULL;
  DDSURFACEDESC2 desc;
  DWORD rva=0,tw=0,th=0,pf=0;
  float l,r,t,b;
  LONG flip,sample,family;
  volatile LONG *sample_budget;
  unsigned i;
  char line[345];
  if(!debug_log_enabled || !device || !v || count!=4 ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL ||
     !native_esc43_supported_monitor() ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0)return;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  if(!native_pause43_visible_near_flip(flip) ||
     !geom_rva(caller,&rva) || rva!=0x00102C72UL)
    return;
  family=InterlockedCompareExchange(&native_pause43_banner_probe_family,0,0);
  sample_budget=family==2?&native_pause43_detail_venom2_samples:
                family==1?&native_pause43_detail_new_family_samples:
                          &native_pause43_detail_samples;
  if(InterlockedCompareExchange(sample_budget,0,0)>=
       (family!=0?160:80))return;
  l=r=v[0].x;t=b=v[0].y;
  for(i=1;i<4;i++) {
    if(v[i].x<l)l=v[i].x;
    if(v[i].x>r)r=v[i].x;
    if(v[i].y<t)t=v[i].y;
    if(v[i].y>b)b=v[i].y;
  }
  /* Focus on native letter glyphs, the narrow 8x8 outlines, and the
   * red 32x32 selector. Exclude large motif/world quads and player HUD. */
  if(!(l>(float)desktop_w*0.15f && r<(float)desktop_w*0.85f &&
       t>(float)desktop_h*0.10f && b<(float)desktop_h*0.70f &&
       r-l>4.0f && r-l<(float)desktop_w*0.10f &&
       b-t>8.0f && b-t<(float)desktop_h*0.11f))
    return;
  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  if(FAILED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex))
     || !tex)return;
  if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(tex,&desc))) {
    tw=desc.dwWidth;th=desc.dwHeight;pf=desc.ddpfPixelFormat.dwFlags;
  }
  IDirectDrawSurface7_Release(tex);
  if(!((th==32u && (tw==16u || tw==32u || tw==64u)) ||
       (tw==8u && th==8u)))return;
  sample=InterlockedIncrement(sample_budget);
  if(sample>(family!=0?160:80))return;
  snprintf(line,sizeof(line),
    "PAUSE-NATIVE-DETAIL: sample=%ld family=%ld flip=%ld corrected=%d quad=(%.1f..%.1f,%.1f..%.1f) z=%.5f rhw=%.4f color=%08lX tex=%lux%lu fmt=%08lX [READ-ONLY]",
    (long)sample,(long)family,(long)flip,corrected,
    (double)l,(double)r,(double)t,(double)b,
    (double)v[0].z,(double)v[0].rhw,
    (unsigned long)v[0].diffuse,(unsigned long)tw,(unsigned long)th,
    (unsigned long)pf);
  log_line(line);
}
/* Separate limited border probe: the first native Pause detail sampler
 * captured font quads but its center-X filter excluded edge strokes.
 * 8x8 border sprites at the extreme left/right are collected only when
 * the verified original game ribbon is present in this Flip. */
static volatile LONG native_pause43_border_samples=0;
static void maybe_log_native_pause43_border(void *device,
              D3DPRIMITIVETYPE type,DWORD fvf,const void *source,
              DWORD count,void *caller,int corrected) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  IDirectDrawSurface7 *tex=NULL;
  DDSURFACEDESC2 desc;
  DWORD rva=0,tw=0,th=0;
  float l,r,t,b;
  LONG flip,n;
  unsigned i;
  char line[310];
  if(!debug_log_enabled || !device || !v ||
     count!=4 || type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL ||
     !native_esc43_supported_monitor() ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     InterlockedCompareExchange(&native_pause43_border_samples,0,0)>=40)
    return;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  if(!native_pause43_visible_near_flip(flip) ||
     !geom_rva(caller,&rva) || rva!=0x00102C72UL)return;
  l=r=v[0].x;t=b=v[0].y;
  for(i=1;i<4;i++) {
    if(v[i].x<l)l=v[i].x;
    if(v[i].x>r)r=v[i].x;
    if(v[i].y<t)t=v[i].y;
    if(v[i].y>b)b=v[i].y;
  }
  if(!((l<(float)desktop_w*0.05f) ||
       (r>(float)desktop_w*0.95f) ||
       (r-l>(float)desktop_w*0.85f)) ||
     t<(float)desktop_h*0.28f || t>(float)desktop_h*0.75f ||
     b-t<4.0f || b-t>(float)desktop_h*0.36f)return;
  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  if(FAILED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex))
     || !tex)return;
  if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(tex,&desc))) {
    tw=desc.dwWidth;th=desc.dwHeight;
  }
  IDirectDrawSurface7_Release(tex);
  if(tw!=8u || th!=8u)return;
  n=InterlockedIncrement(&native_pause43_border_samples);
  if(n>40)return;
  snprintf(line,sizeof(line),
    "PAUSE-NATIVE-BORDER-PROBE: n=%ld flip=%ld corrected=%d box=(%.1f..%.1f,%.1f..%.1f) z=%.5f rhw=%.4f tex=8x8 [READ-ONLY]",
    (long)n,(long)flip,corrected,
    (double)l,(double)r,(double)t,(double)b,
    (double)v[0].z,(double)v[0].rhw);
  log_line(line);
}
/* Does the original game draw world HUD after its ESC ribbon?
 * These read-only samples identify drawing order and Z/blend state before
 * attempting an unsafe menu-depth rewrite or any texture replay. The
 * Dreamcast reference requires the ESC interface to own the foreground.
 * The renderer may use overlapping depth and/or later player HUD draws,
 * so do not guess an occlusion policy from screenshot ordering alone. */
static volatile LONG ui_pause_layer_overlap_samples=0;
static void maybe_probe_pause_hud_layer(void *device,
                                        const SpiderMenuVertex *v)
{
  float l,r,t,b,canvas_left,canvas_right;
  LONG n,flip,bt,bb;
  DWORD ze=0,zw=0,zf=0,ab=0;
  char msg[340];
  unsigned i;
  if(!debug_log_enabled || !device || !v ||
     InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)!=1 ||
     InterlockedCompareExchange(&ui_pause_layer_overlap_samples,0,0)>=64)
    return;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  if(!native_pause43_visible_near_flip(flip)) return;
  bt=InterlockedCompareExchange(&native_pause43_banner_top,0,0);
  bb=InterlockedCompareExchange(&native_pause43_banner_bottom,0,0);
  if(bb<=bt) return;
  l=r=v[0].x;t=b=v[0].y;
  for(i=1;i<4;i++) {
    if(v[i].x<l)l=v[i].x;
    if(v[i].x>r)r=v[i].x;
    if(v[i].y<t)t=v[i].y;
    if(v[i].y>b)b=v[i].y;
  }
  canvas_left=native_esc43_left_gutter()-50.0f;
  canvas_right=(float)desktop_w-canvas_left;
  if(l>=canvas_right || r<=canvas_left || t>=(float)bb || b<=(float)bt)
    return;
  n=InterlockedIncrement(&ui_pause_layer_overlap_samples);
  if(n>64)return;
  IDirect3DDevice7_GetRenderState((IDirect3DDevice7*)device,
                                   D3DRENDERSTATE_ZENABLE,&ze);
  IDirect3DDevice7_GetRenderState((IDirect3DDevice7*)device,
                                   D3DRENDERSTATE_ZWRITEENABLE,&zw);
  IDirect3DDevice7_GetRenderState((IDirect3DDevice7*)device,
                                   D3DRENDERSTATE_ZFUNC,&zf);
  IDirect3DDevice7_GetRenderState((IDirect3DDevice7*)device,
                                   D3DRENDERSTATE_ALPHABLENDENABLE,&ab);
  snprintf(msg,sizeof(msg),
    "PAUSE-HUD-LAYER-PROBE: n=%ld flip=%ld ribbon-flip=%ld OVERLAP-AFTER-RIBBON box=(%.1f..%.1f,%.1f..%.1f) menu-y=(%ld..%ld) z=%.5f rhw=%.4f color=%08lX zen=%lu zw=%lu zfunc=%lu alpha=%lu [READ-ONLY]",
    (long)n,(long)flip,
    (long)InterlockedCompareExchange(&native_pause43_banner_flip,0,0),
    (double)l,(double)r,(double)t,(double)b,(long)bt,(long)bb,
    (double)v[0].z,(double)v[0].rhw,
    (unsigned long)v[0].diffuse,
    (unsigned long)ze,(unsigned long)zw,(unsigned long)zf,
    (unsigned long)ab);
  log_line(msg);
}

static void free_menu_vertices(SpiderMenuVertex *allocated) {
  if (allocated) HeapFree(GetProcessHeap(),0,allocated);
}

/* Opt-in native ESC foreground: use the game's original menu draw
 * and original alpha blend, but force the identified menu primitive to
 * pass depth testing above gameplay HUD (Dreamcast layering reference).
 * No persistent render-state changes, no 3D/UI ownership heuristics.
 * Native ESC draw order still matters; probe any HUD submitted later. */
static volatile LONG ui_esc_front_draw_hits=0,ui_esc_front_draw_fallbacks=0;
static HRESULT draw_native_esc_in_front(void *device,
                  D3DPRIMITIVETYPE type,DWORD fvf,
                  LPVOID vertices,DWORD count,DWORD flags,int native_esc)
{
  IDirect3DDevice7 *dev=(IDirect3DDevice7*)device;
  DWORD zen=0,zfunc=0,zw=0;
  HRESULT hr;
  if(!device || !native_esc ||
     InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)!=1)
    return original_draw_primitive(device,type,fvf,vertices,count,flags);
  if(FAILED(IDirect3DDevice7_GetRenderState(dev,D3DRENDERSTATE_ZENABLE,&zen)) ||
     zen==D3DZB_FALSE ||
     FAILED(IDirect3DDevice7_GetRenderState(dev,D3DRENDERSTATE_ZFUNC,&zfunc)) ||
     FAILED(IDirect3DDevice7_GetRenderState(dev,D3DRENDERSTATE_ZWRITEENABLE,&zw)) ||
     FAILED(IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZFUNC,D3DCMP_ALWAYS))) {
    LONG n=InterlockedIncrement(&ui_esc_front_draw_fallbacks);
    if(debug_log_enabled && n<=3)
      log_line("ESC-FOREGROUND: native Z disabled/unavailable, original draw retained");
    return original_draw_primitive(device,type,fvf,vertices,count,flags);
  }
  if(FAILED(IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZWRITEENABLE,FALSE))) {
    IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZFUNC,zfunc);
    if(debug_log_enabled && InterlockedIncrement(&ui_esc_front_draw_fallbacks)<=3)
      log_line("ESC-FOREGROUND: ZWRITE setup failed, original draw retained");
    return original_draw_primitive(device,type,fvf,vertices,count,flags);
  }
  hr=original_draw_primitive(device,type,fvf,vertices,count,flags);
  {
    HRESULT rw=IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZWRITEENABLE,zw);
    HRESULT rz=IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZFUNC,zfunc);
    if((FAILED(rw)||FAILED(rz)) && debug_log_enabled &&
       InterlockedIncrement(&ui_esc_front_draw_fallbacks)<=3)
      log_line("ESC-FOREGROUND: WARNING native Z state restoration failed");
  }
  {
    LONG n=InterlockedIncrement(&ui_esc_front_draw_hits);
    if(debug_log_enabled && (n<=8 || n%1200==0)) {
      char message[200];
      snprintf(message,sizeof(message),
        "ESC-FOREGROUND: draw=%ld source-ZEN=%lu ZFUNC=%lu ZWRITE=%lu temporary=ALWAYS/NO-ZWRITE [EXPERIMENT]",
        (long)n,(unsigned long)zen,(unsigned long)zfunc,(unsigned long)zw);
      log_line(message);
    }
  }
  return hr;
}

/* Reversible Direct3D7 pause compositor.
 * Snapshot each game-owned ESC draw (vertices + ALL stateblock + texture).
 * Do NOT draw it at its original point. At EndScene, after the HUD and
 * scene have been submitted, draw an optional physical-viewport tint,
 * then replay the original ESC motif in centered 4:3 and the native menu.
 * No boss/level-specific ordering, no GPU render-target readback, no
 * synthetic transparent object identification. Calls are bounded and
 * fail back to the original native draw path if capture is unsupported.
 *
 * D3D7 state blocks capture device states, but the bound texture is held
 * explicitly by one GetTexture COM reference per command. The final
 * stateblock plus texture restores the game's original states on exit.
 * A temporary ZENABLE=FALSE is safe for 2D replay even on a device with
 * an already disabled native depth test.
 */
typedef HRESULT (WINAPI *D3DSceneFn)(void *);
static D3DSceneFn ui_original_begin_scene=NULL,ui_original_end_scene=NULL;
typedef struct {
  SpiderMenuVertex v[4];
  /* Raw native screen-space geometry before the 4:3 canvas mapping.
   * Captured for a single read-only scene-motif diagnostic, never drawn. */
  SpiderMenuVertex source_v[4];
  DWORD fvf,flags,state_token;
  D3DPRIMITIVETYPE type;
  IDirectDrawSurface7 *texture;
  int layer;
} SpiderUICompDraw;
static SpiderUICompDraw ui_comp_draws[SPIDER_UI_COMP_MAX_COMMANDS];
static unsigned ui_comp_count=0;
static int ui_comp_scene_open=0,ui_comp_bailout=0,ui_comp_ribbon=0,ui_comp_motif=0;
static void *ui_comp_device=NULL;
static volatile LONG ui_comp_deferred=0,ui_comp_replayed=0;
static volatile LONG ui_comp_capture_failures=0,ui_comp_no_scene=0,ui_comp_replay_failures=0;
/* Frame counters are independent of quad count; modulo on cumulative
 * DrawPrimitive hits can otherwise miss every EndScene log indefinitely. */
static volatile LONG ui_comp_flushed_frames=0,ui_comp_fullscreen_candidates=0;
static volatile LONG ui_comp_source_dump_once=0;
/* Read-only check for another game-owned HUD pass AFTER our EndScene
 * Pause overlay, within the same original DirectDraw Flip. If detected,
 * an overlay-order change at EndScene alone cannot guarantee final
 * foreground precedence. Never intercept or reorder those draws. */
static volatile LONG ui_comp_last_overlay_flip=-1;
static volatile LONG ui_comp_late_draw_samples=0;

static void ui_comp_release(IDirect3DDevice7 *device) {
  unsigned i;
  for(i=0;i<ui_comp_count;i++){
    if(ui_comp_draws[i].state_token && device)
      IDirect3DDevice7_DeleteStateBlock(device,ui_comp_draws[i].state_token);
    if(ui_comp_draws[i].texture)
      IDirectDrawSurface7_Release(ui_comp_draws[i].texture);
    memset(&ui_comp_draws[i],0,sizeof(ui_comp_draws[i]));
  }
  ui_comp_count=0;
  ui_comp_ribbon=ui_comp_motif=0;
}

/* Original Pause decoration has 160 translucent primitives in two
 * source colour families. The perimeter material reaches all four
 * physical source edges while the inner spider is strictly interior.
 * Verify that relationship every frame using the untouched source
 * geometry, texture and original D3D7 state (no level/RHW/ARGB list).
 *
 * This detector supplies ONLY the exterior gutter colour and material:
 * every native source primitive is replayed intact. We must never
 * replace, recolour or separate the original centre 4:3 mesh because
 * hardware A/B showed that it changes the Dreamcast-like alpha mix.
 */
static int ui_comp_native_backdrop_source(DWORD *native_argb,
    float *inner_left,float *inner_right,unsigned *inner_quads,
    const SpiderUICompDraw **source_material)
{
  SpiderUICompBackdropSample samples[SPIDER_UI_COMP_MAX_COMMANDS];
  unsigned count=0u,i,k,background=0u,inner=0u;
  float linner=(float)desktop_w,rinner=0.0f;
  float w=(float)desktop_w,h=(float)desktop_h;
  if(native_argb)*native_argb=0u;
  if(inner_left)*inner_left=0.0f;
  if(inner_right)*inner_right=0.0f;
  if(inner_quads)*inner_quads=0u;
  if(source_material)*source_material=NULL;
  if(!native_argb || !inner_left || !inner_right || !source_material ||
     !ui_comp_motif ||
     ui_comp_bailout || ui_comp_count>SPIDER_UI_COMP_MAX_COMMANDS)
    return 0;
  for(i=0u;i<ui_comp_count;i++){
    const SpiderUICompDraw *c=&ui_comp_draws[i];
    const SpiderMenuVertex *v=c->source_v;
    float l,r,t,b;
    if(c->layer!=SPIDER_UI_COMP_PAUSE_DECOR)continue;
    if(!c->state_token || c->texture ||
       c->type!=D3DPT_TRIANGLEFAN || c->fvf!=0x144UL ||
       count>=SPIDER_UI_COMP_MAX_COMMANDS)return 0;
    l=r=v[0].x;t=b=v[0].y;
    for(k=1u;k<4u;k++){
      if(v[k].diffuse!=v[0].diffuse ||
         fabsf(v[k].z-v[0].z)>0.00005f ||
         fabsf(v[k].rhw-v[0].rhw)>0.080f)return 0;
      if(v[k].x<l)l=v[k].x;
      if(v[k].x>r)r=v[k].x;
      if(v[k].y<t)t=v[k].y;
      if(v[k].y>b)b=v[k].y;
    }
    samples[count].left=l;samples[count].right=r;
    samples[count].top=t;samples[count].bottom=b;
    samples[count++].color=(unsigned)v[0].diffuse;
  }
  if(!SpiderUIComp_IdentifyNativeBackdrop(samples,count,w,h,&background))
    return 0;
  for(i=0u;i<count;i++){
    if(samples[i].color==background)continue;
    if(samples[i].left<linner)linner=samples[i].left;
    if(samples[i].right>rinner)rinner=samples[i].right;
    inner++;
  }
  if(!inner || linner<=1.0f || rinner>=w-1.0f ||
     !(rinner>linner))return 0;
  for(i=0u;i<ui_comp_count;i++){
    const SpiderUICompDraw *c=&ui_comp_draws[i];
    if(c->layer==SPIDER_UI_COMP_PAUSE_DECOR &&
       c->source_v[0].diffuse==(DWORD)background) {
      *source_material=c;
      break;
    }
  }
  if(!*source_material)return 0;
  *native_argb=(DWORD)background;
  *inner_left=linner;
  *inner_right=rinner;
  if(inner_quads)*inner_quads=inner;
  return 1;
}

/* Source-colour exterior completion only.
 *
 * A/B field evidence (2026-11-06 F9 scrn0103 and scrn0104, same
 * 05d2804 build): source centre keeps correct RGB when ALL 160 native
 * decoration triangles are replayed. Replacing its 84 perimeter
 * triangles with one alpha-blended fullscreen quad causes dull
 * colours, even with HUD replay OFF. Therefore NEVER draw a colour
 * overlay over the already correct 4:3 centre or discard source tiles.
 *
 * Reconstruct only the PHYSICAL exterior [0,gutter) and
 * (width-gutter,width] from the ORIGINAL game's edge-owned material.
 * Source-backed palette and D3D7 blend state are validated upstream
 * every frame. Centre source geometry, order and alpha are preserved
 * byte-for-byte. Independent widescreen sizes use the actual monitor
 * dimensions; no levels, RHW, RGB or alpha constants are hardcoded.
 *
 * The render state block is saved by ui_comp_flush() before this
 * operation and restored after the native Pause replay. This helper
 * restores the precise original viewport on all paths.
 */
static int ui_comp_draw_native_exterior_gutters(
    IDirect3DDevice7 *device,const SpiderUICompDraw *sample,DWORD argb,
    unsigned *quads_drawn)
{
  SpiderMenuVertex v[4];
  D3DVIEWPORT7 previous,physical;
  HRESULT draw_hr=S_OK,restore_hr=S_OK;
  DWORD w=(DWORD)desktop_w,h=(DWORD)desktop_h;
  DWORD center_w,margin,right_begin;
  unsigned k,side,done=0u;
  int changed=0;
  LONG frame=InterlockedCompareExchange(&ui_comp_flushed_frames,0,0);
  if(quads_drawn)*quads_drawn=0u;
  if(!quads_drawn || !device || !sample ||
     !sample->state_token || sample->texture ||
     sample->fvf!=0x144UL || sample->type!=D3DPT_TRIANGLEFAN ||
     sample->source_v[0].diffuse!=argb ||
     (argb>>24)==0u || !original_set_viewport ||
     w<640u || h<480u || w>16384u || h>16384u)
    return 0;
  center_w=(DWORD)((float)h*(4.0f/3.0f)+0.5f);
  if(center_w>=w)return 0;
  /* Integer 4:3 fitting is not always symmetric for odd-sized
   * desktops. Keep the original centred viewport and allow the
   * right margin to be exactly one pixel wider if required. */
  margin=(w-center_w)/2u;
  right_begin=margin+center_w;
  if(margin==0u || right_begin>=w)return 0;
  if(FAILED(IDirect3DDevice7_ApplyStateBlock(device,sample->state_token)) ||
     FAILED(IDirect3DDevice7_GetViewport(device,&previous)))
    return 0;
  physical=previous;
  if(previous.dwX==0u && previous.dwY==0u &&
     previous.dwWidth==w && previous.dwHeight==h) {
    /* Already drawing to the full physical backbuffer. */
  } else if(previous.dwX==margin && previous.dwY==0u &&
            previous.dwWidth==center_w && previous.dwHeight==h &&
            previous.dvMinZ<=previous.dvMaxZ) {
    physical.dwX=0u;physical.dwY=0u;
    physical.dwWidth=w;physical.dwHeight=h;
    if(FAILED(original_set_viewport(device,&physical)))
      return 0;
    changed=1;
  } else {
    if(debug_log_enabled && frame<4){
      char msg[215];
      snprintf(msg,sizeof(msg),
        "UI-COMPOSITOR-BACKDROP-VIEWPORT: gutter-only rejected unexpected %lux%lu+%lu+%lu",
        (unsigned long)previous.dwWidth,(unsigned long)previous.dwHeight,
        (unsigned long)previous.dwX,(unsigned long)previous.dwY);
      log_line(msg);
    }
    return 0;
  }
  /* Original material is restored, and only the two side gutters
   * receive a single alpha pass. All original 4:3 decoration is still
   * replayed AFTER this call, untouched and without double blending. */
  for(k=0u;k<4u;k++)v[k]=sample->source_v[0];
  if(FAILED(IDirect3DDevice7_SetTexture(device,0,NULL)) ||
     FAILED(IDirect3DDevice7_SetRenderState(device,
                               D3DRENDERSTATE_ZENABLE,D3DZB_FALSE)) ||
     FAILED(IDirect3DDevice7_SetRenderState(device,
                               D3DRENDERSTATE_ZWRITEENABLE,FALSE)))
    draw_hr=E_FAIL;
  for(side=0u;side<2u && SUCCEEDED(draw_hr);side++){
    float left=side==0u ? 0.0f:(float)right_begin;
    float right=side==0u ? (float)margin:(float)w;
    v[0].x=left;v[0].y=0.0f;
    v[1].x=right;v[1].y=0.0f;
    v[2].x=right;v[2].y=(float)h;
    v[3].x=left;v[3].y=(float)h;
    draw_hr=original_draw_primitive(device,
                     D3DPT_TRIANGLEFAN,0x144UL,v,4,sample->flags);
    if(SUCCEEDED(draw_hr))done++;
  }
  if(changed)
    restore_hr=original_set_viewport(device,&previous);
  if(debug_log_enabled && (frame<4 || frame%180==0 ||
                            FAILED(draw_hr) || FAILED(restore_hr))){
    char msg[300];
    snprintf(msg,sizeof(msg),
      "UI-COMPOSITOR-BACKDROP-VIEWPORT: gutter-only viewport=%lux%lu+%lu+%lu physical=%lux%lu temporary-switch=%d draw=0x%08lX restore=0x%08lX quads=%u",
      (unsigned long)previous.dwWidth,(unsigned long)previous.dwHeight,
      (unsigned long)previous.dwX,(unsigned long)previous.dwY,
      (unsigned long)w,(unsigned long)h,
      changed,(unsigned long)draw_hr,(unsigned long)restore_hr,done);
    log_line(msg);
  }
  if(FAILED(restore_hr)) {
    log_line("UI-COMPOSITOR-BACKDROP-VIEWPORT: ERROR original viewport not restored");
    return 0;
  }
  if(FAILED(draw_hr) || done!=2u)return 0;
  *quads_drawn=done;
  return 1;
}

/* Flush before native EndScene, not at DirectDraw Flip, so the entire
 * replay remains inside the original legal BeginScene/EndScene pair. */
static void ui_comp_flush(IDirect3DDevice7 *device) {
  DWORD saved=0;
  IDirectDrawSurface7 *saved_texture=NULL;
  unsigned i,layer,pass,inner_quads=0u;
  unsigned paint_hud=0u,paint_spider=0u,paint_menu=0u;
  unsigned exterior_quads=0u;
  DWORD native_backdrop_argb=0u;
  float spider_source_left=0.0f,spider_source_right=0.0f;
  const SpiderUICompDraw *native_backdrop_material=NULL;
  LONG n;
  int all_ok=1,native_backdrop_verified=0,native_backdrop_drawn=0;
  if(ui_comp_count==0)return;
  if(!device || ui_comp_device!=device){
    log_line("UI-COMPOSITOR: scene owner mismatch; discarding deferred commands");
    ui_comp_release(device);ui_comp_bailout=1;return;
  }
  if(FAILED(IDirect3DDevice7_CreateStateBlock(device,D3DSBT_ALL,&saved)) ||
     !saved) {
    log_line("UI-COMPOSITOR: cannot save end-scene state; disabling deferred path");
    ui_comp_release(device);ui_comp_bailout=1;
    InterlockedIncrement(&ui_comp_capture_failures);
    return;
  }
  /* GetTexture() supplies one owned COM reference when non-NULL. */
  if(FAILED(IDirect3DDevice7_GetTexture(device,0,&saved_texture))) {
    IDirect3DDevice7_DeleteStateBlock(device,saved);
    ui_comp_release(device);ui_comp_bailout=1;
    log_line("UI-COMPOSITOR: cannot save bound end-scene texture; fallback");
    return;
  }
  /* Validate the original perimeter material but NEVER replace its
   * native 84 decoration tiles. The A/B test proved that a fullscreen
   * substitute destroys the central source colour and blend fidelity.
   * Fill just the physical side margins, retaining all 160 source
   * triangles over the original 4:3 centre. */
  if(ui_comp_motif &&
     InterlockedCompareExchange(&ui_compositor_native_backdrop_enabled,0,0)==1)
    native_backdrop_verified=ui_comp_native_backdrop_source(
      &native_backdrop_argb,&spider_source_left,&spider_source_right,
      &inner_quads,&native_backdrop_material);
  if(native_backdrop_verified)
    native_backdrop_drawn=ui_comp_draw_native_exterior_gutters(
      device,native_backdrop_material,native_backdrop_argb,&exterior_quads);
  if(debug_log_enabled && ui_comp_motif &&
     (InterlockedCompareExchange(&ui_comp_flushed_frames,0,0)<4 ||
      InterlockedCompareExchange(&ui_comp_flushed_frames,0,0)%180==0)){
    char msg[300];
    snprintf(msg,sizeof(msg),
      "UI-COMPOSITOR-NATIVE-BACKDROP: %s verified=%d drawn=%d quads=%u argb=%08lX spider-bounds=(%.1f..%.1f) inner-quads=%u physical=%ux%u [SOURCE CENTRE UNCHANGED: ALL 160 NATIVE TRIANGLES REPLAYED]",
      native_backdrop_drawn?"gutter-only":"native-only",
      native_backdrop_verified,native_backdrop_drawn,exterior_quads,
      (unsigned long)native_backdrop_argb,
      (double)spider_source_left,(double)spider_source_right,
      inner_quads,(unsigned)desktop_w,(unsigned)desktop_h);
    log_line(msg);
  }
  /* This is the native Dreamcast-style painter order:
   *   WORLD (game-owned)
   *   -> source-derived side gutter completion OUTSIDE centre 4:3
   *   -> source-verified game HUD, replayed ONCE if explicitly enabled
   *   -> original native 160-triangle translucent 4:3 decoration
   *   -> PAUSED title, purple ribbon, text and native selector.
   * 
   * HUD deferral is experimental and defaults OFF. Both hardware
   * validated scenes replayed zero HUD quads and retained the game's
   * original HUD drawing order. No extra colour correction is needed
   * when only the physical gutters are painted.
   *
   * Preserve the exact original native geometry and ordering for all
   * 160 translucent decoration triangles. The Dreamcast reference and
   * A/B F9 capture show that replacing or separating the native 84
   * backdrop tiles modifies the visible central colour mix. Two new
   * gutter-only primitives cover exterior pixels but NEVER centre.
   * Source colours and alpha are unmodified for every native triangle.
   */
  for(pass=SPIDER_UI_COMP_REPLAY_HUD;
      pass<=SPIDER_UI_COMP_REPLAY_MENU;pass++){
    for(layer=SPIDER_UI_COMP_HUD;
        layer<=SPIDER_UI_COMP_PAUSE_MENU;layer++){
      for(i=0;i<ui_comp_count;i++) {
        SpiderUICompDraw *c=&ui_comp_draws[i];
        int owner;
        if((unsigned)c->layer!=layer)continue;
        /* The complete native 160-tile motif (including its background)
         * is one contiguous draw pass. Source colours are NEVER filtered
         * or discarded; the separate gutters never cover centre 4:3. */
        owner=SpiderUIComp_ReplayPassForDraw(c->layer);
        if(owner!=(int)pass)continue;
        /* The native capture's full material and texture are restored
         * for every primitive. Disable Z only for the composited
         * screen-space layers, so they remain above the 3D/HUD. */
        if(FAILED(IDirect3DDevice7_ApplyStateBlock(device,c->state_token)) ||
           FAILED(IDirect3DDevice7_SetTexture(device,0,c->texture)) ||
           FAILED(IDirect3DDevice7_SetRenderState(device,D3DRENDERSTATE_ZENABLE,D3DZB_FALSE)) ||
           FAILED(IDirect3DDevice7_SetRenderState(device,D3DRENDERSTATE_ZWRITEENABLE,FALSE)) ||
           FAILED(original_draw_primitive(device,c->type,c->fvf,c->v,4,c->flags))) {
          all_ok=0;
          InterlockedIncrement(&ui_comp_replay_failures);
        } else {
          InterlockedIncrement(&ui_comp_replayed);
          if(pass==SPIDER_UI_COMP_REPLAY_HUD)paint_hud++;
          else if(pass==SPIDER_UI_COMP_REPLAY_MOTIF)paint_spider++;
          else paint_menu++;
        }
      }
    }
  }
  if(paint_spider && paint_menu && all_ok)
    InterlockedExchange(&ui_comp_last_overlay_flip,
       InterlockedCompareExchange(&flip_count,0,0));
  if(debug_log_enabled && ui_comp_motif &&
     (InterlockedCompareExchange(&ui_comp_flushed_frames,0,0)<4 ||
      InterlockedCompareExchange(&ui_comp_flushed_frames,0,0)%180==0 ||
      !all_ok)) {
    char msg[245];
    snprintf(msg,sizeof(msg),
      "UI-COMPOSITOR-ORDER: exterior-gutter-quads=%u native-decor=%u hud=%u menu=%u [WORLD -> GUTTERS OUTSIDE 4:3 -> HUD -> 160 SOURCE TRIANGLES -> PAUSE; NO FULLSCREEN OVERLAY]",
      exterior_quads,paint_spider,paint_hud,paint_menu);
    log_line(msg);
  }
  /* One-shot read-only 4:3 source/mapped comparison after a stable
   * native pause frame. Do not make a new render pass or infer a tint.
   * This tells us whether 160 native motif pieces contain background
   * coverage, colour gradients, or only decorative geometry. */
  if(debug_log_enabled && ui_comp_ribbon && ui_comp_motif &&
     InterlockedCompareExchange(&ui_comp_flushed_frames,0,0)>=59 &&
     InterlockedCompareExchange(&ui_comp_source_dump_once,1,0)==0) {
    unsigned q,k,found=0,source_left=0,source_right=0,source_full=0;
    float width=(float)desktop_w,height=(float)desktop_h;
    float raw_min=width,raw_max=0.0f,raw_top=height,raw_bottom=0.0f;
    char msg[340];
    for(q=0;q<ui_comp_count;q++) {
      const SpiderUICompDraw *c=&ui_comp_draws[q];
      float l,r,t,b;
      if(c->layer!=SPIDER_UI_COMP_PAUSE_DECOR)continue;
      found++;
      l=r=c->source_v[0].x;t=b=c->source_v[0].y;
      for(k=1;k<4;k++) {
        if(c->source_v[k].x<l)l=c->source_v[k].x;
        if(c->source_v[k].x>r)r=c->source_v[k].x;
        if(c->source_v[k].y<t)t=c->source_v[k].y;
        if(c->source_v[k].y>b)b=c->source_v[k].y;
      }
      if(l<raw_min)raw_min=l;
      if(r>raw_max)raw_max=r;
      if(t<raw_top)raw_top=t;
      if(b>raw_bottom)raw_bottom=b;
      if(l<=width*.03f)source_left++;
      if(r>=width*.97f)source_right++;
      if(l<=width*.03f && r>=width*.97f)source_full++;
      /* Capture ALL vertex diffuse colours: important for native
       * translucent gradients which cannot be reconstructed from
       * the first vertex of the first motif quad. */
      snprintf(msg,sizeof(msg),
        "UI-COMPOSITOR-MOTIF-SOURCE: q=%u raw=(%.1f..%.1f,%.1f..%.1f) mapped-x=(%.1f..%.1f) ARGB=%08lX/%08lX/%08lX/%08lX z=%.5f rhw=%.3f [READ-ONLY]",
        q,(double)l,(double)r,(double)t,(double)b,
        (double)c->v[0].x,(double)c->v[1].x,
        (unsigned long)c->source_v[0].diffuse,
        (unsigned long)c->source_v[1].diffuse,
        (unsigned long)c->source_v[2].diffuse,
        (unsigned long)c->source_v[3].diffuse,
        (double)c->source_v[0].z,(double)c->source_v[0].rhw);
      log_line(msg);
    }
    snprintf(msg,sizeof(msg),
      "UI-COMPOSITOR-MOTIF-SUMMARY: quads=%u raw-extents=(%.1f..%.1f,%.1f..%.1f) edge-left=%u edge-right=%u fullwidth-quads=%u physical=%ux%u [SOURCE BEFORE 4:3 TRANSFORM]",
      found,(double)raw_min,(double)raw_max,
      (double)raw_top,(double)raw_bottom,
      source_left,source_right,source_full,
      (unsigned)desktop_w,(unsigned)desktop_h);
    log_line(msg);
  }
  if(FAILED(IDirect3DDevice7_ApplyStateBlock(device,saved))) {
    all_ok=0;log_line("UI-COMPOSITOR: WARNING end-scene state restore failed");
  }
  /* ApplyStateBlock bypasses hooked_set_render_state. Resample the
   * actual restored Z state so the existing Hor+ 3D FOV guard does
   * not accidentally retain our temporary 2D ZENABLE=FALSE. */
  {
    DWORD restored_z=0;
    if(SUCCEEDED(IDirect3DDevice7_GetRenderState(device,
                                                 D3DRENDERSTATE_ZENABLE,&restored_z))) {
      InterlockedExchange(&fov_z_state_enabled,restored_z!=D3DZB_FALSE);
      InterlockedExchange(&fov_z_state_known,1);
    }
  }
  if(FAILED(IDirect3DDevice7_SetTexture(device,0,saved_texture))) {
    all_ok=0;log_line("UI-COMPOSITOR: WARNING end-scene texture restore failed");
  }
  if(saved_texture)IDirectDrawSurface7_Release(saved_texture);
  IDirect3DDevice7_DeleteStateBlock(device,saved);
  n=InterlockedCompareExchange(&ui_comp_replayed,0,0);
  {
    LONG frame=InterlockedIncrement(&ui_comp_flushed_frames);
    /* Source-only layer coverage, not a per-sprite material heuristic.
     * On a 1920x1080 screen, a decoration bounded by x≈240..1680
     * cannot physically colour the 240px gutters at either side. */
    if(debug_log_enabled && (frame<=3 || frame%180==0 || !all_ok)) {
      unsigned decor=0,menu=0,k;
      float dl=(float)desktop_w,dr=0.0f,dt=(float)desktop_h,db=0.0f;
      char msg[350];
      for(i=0;i<ui_comp_count;i++) {
        const SpiderUICompDraw *c=&ui_comp_draws[i];
        if(c->layer==SPIDER_UI_COMP_PAUSE_DECOR) {
          ++decor;
          for(k=0;k<4;k++){
            if(c->v[k].x<dl)dl=c->v[k].x;
            if(c->v[k].x>dr)dr=c->v[k].x;
            if(c->v[k].y<dt)dt=c->v[k].y;
            if(c->v[k].y>db)db=c->v[k].y;
          }
        } else if(c->layer==SPIDER_UI_COMP_PAUSE_MENU) ++menu;
      }
      snprintf(msg,sizeof(msg),
        "UI-COMPOSITOR-FRAME: frame=%ld commands=%u decor=%u menu=%u decor-box=(%.1f..%.1f,%.1f..%.1f) canvas=(%.1f..%.1f) candidate-coverage=%ld replay-total=%ld all-ok=%d",
        (long)frame,ui_comp_count,decor,menu,
        (double)dl,(double)dr,(double)dt,(double)db,
        (double)((float)desktop_w-(float)desktop_h*4.0f/3.0f)*0.5,
        (double)((float)desktop_w+(float)desktop_h*4.0f/3.0f)*0.5,
        (long)InterlockedCompareExchange(&ui_comp_fullscreen_candidates,0,0),
        (long)n,all_ok);
      log_line(msg);
    }
  }
  ui_comp_release(device);
}

/* Called from DrawPrimitive only for verified native ESC-owned quads.
 * Any unsupported capture falls through to the original immediate draw
 * without changing global device state; no silent missing menu on
 * unsupported stateblocks. */
static int ui_comp_capture(IDirect3DDevice7 *device,D3DPRIMITIVETYPE type,
             DWORD fvf,const void *vertices,const void *source_vertices,
             DWORD count,DWORD flags,int layer)
{
  SpiderUICompDraw *c;
  DWORD block=0;
  IDirectDrawSurface7 *texture=NULL;
  if(!SpiderUIComp_ShouldDefer(
       InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1,
       ui_comp_scene_open && ui_comp_device==device,ui_comp_bailout,
       layer,(unsigned)count,(unsigned)fvf)) {
    if(debug_log_enabled && ui_comp_scene_open==0 &&
       InterlockedIncrement(&ui_comp_no_scene)<=4)
      log_line("UI-COMPOSITOR: menu draw outside BeginScene, original draw retained");
    return 0;
  }
  if(ui_comp_count>=SPIDER_UI_COMP_MAX_COMMANDS) {
    log_line("UI-COMPOSITOR: frame budget exceeded, flushing safely and bypassing");
    ui_comp_flush(device);ui_comp_bailout=1;return 0;
  }
  if(FAILED(IDirect3DDevice7_CreateStateBlock(device,D3DSBT_ALL,&block)) || !block)
    goto capture_failed;
  if(FAILED(IDirect3DDevice7_GetTexture(device,0,&texture)))
    goto capture_failed;
  c=&ui_comp_draws[ui_comp_count++];
  memset(c,0,sizeof(*c));
  memcpy(c->v,vertices,4u*sizeof(SpiderMenuVertex));
  /* This is a passive snapshot; only the corrected c->v is replayed. */
  if(source_vertices)
    memcpy(c->source_v,source_vertices,4u*sizeof(SpiderMenuVertex));
  c->fvf=fvf;c->flags=flags;c->type=type;c->layer=layer;
  c->state_token=block;c->texture=texture;
  if(layer==SPIDER_UI_COMP_PAUSE_DECOR)
    ui_comp_motif=1;
  else if(layer==SPIDER_UI_COMP_PAUSE_MENU)
    ui_comp_ribbon=1;
  if(InterlockedIncrement(&ui_comp_deferred)<=8 && debug_log_enabled)
    log_line("UI-COMPOSITOR: native ESC primitive captured, deferred until EndScene");
  return 1;
capture_failed:
  if(block)IDirect3DDevice7_DeleteStateBlock(device,block);
  if(texture)IDirectDrawSurface7_Release(texture);
  if(debug_log_enabled && InterlockedIncrement(&ui_comp_capture_failures)<=5)
    log_line("UI-COMPOSITOR: D3D7 stateblock/texture capture failed, native draw fallback");
  /* Draw already deferred commands in their same original scene, then
   * disable captures until next BeginScene. */
  ui_comp_flush(device);ui_comp_bailout=1;
  return 0;
}

static HRESULT WINAPI ui_compositor_scene_start(void *self) {
  HRESULT hr=ui_original_begin_scene(self);
  if(SUCCEEDED(hr) &&
     InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)==1){
    ui_transition_hold22_scene_native=0u;
    ui_transition_hold22_scene_gutters=0u;
    SpiderNativeSync24_Reset(&ui_transition_sync24_frame);
    SpiderNativeBounds25_Reset(&ui_transition_bounds25_frame);
    ui_transition_sync24_flags=0u;
  }
  if(SUCCEEDED(hr) &&
     (InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 ||
      InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)==1)){
    if(ui_comp_count){
      log_line("UI-COMPOSITOR: stale scene discarded on next BeginScene");
      ui_comp_release((IDirect3DDevice7*)ui_comp_device);
    }
    ui_comp_scene_open=1;ui_comp_device=self;ui_comp_bailout=0;
  }
  return hr;
}
/* V24 EndScene black physical-gutters-only overlay. Source aperture,
 * teeth, black 4:3 triangles and their original D3D7 draw order were
 * already rendered by the game; they are NEVER redrawn or replaced.
 *
 * D3D7 state is saved/restored around these TWO opaque black strips,
 * so EndScene's camera/material/HUD/ESC state cannot leak into or out
 * of this narrow supplemental rectangle pair.
 */
static void ui_transition_sync24_finish(IDirect3DDevice7 *device)
{
  SpiderMenuVertex sides[2][4];
  float band=0.0f,left=0.0f,right=0.0f;
  DWORD saved=0u,restored_z=0u;
  IDirectDrawSurface7 *bound=NULL;
  const int bounds25=
    InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)==1;
  int built,ok=1,draw_left=1,draw_right=1;
  unsigned drawn=0u;
  HRESULT hr;
  unsigned total=ui_transition_sync24_frame.native_fans;
  if(InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)!=1 ||
     !device || !total)return;
  /* This must be the same verified original source family already
   * counted by the old V22 diagnostics. A mismatch is not a reason to
   * invent any side geometry. */
  if(total!=ui_transition_hold22_scene_native){
    if(debug_log_enabled)
      log_line("UI-TRANSITION-SYNC24: SOURCE COUNT MISMATCH; no side draw (native V19 kept)");
    return;
  }
  if(bounds25){
    unsigned w=(unsigned)desktop_w,h=(unsigned)desktop_h;
    int measured=0;
    /* 4:3 and portrait displays have NO horizontal gutters at all. */
    if((uint64_t)w*3u<=(uint64_t)h*4u){
      built=2;
    }else{
      measured=SpiderNativeBounds25_Widths(
          &ui_transition_bounds25_frame,w,h,&left,&right);
      if(!measured){
        /* V24 remains a strictly separate safety fallback for a
         * source family with no visible eligible tooth triangles. */
        built=SpiderNativeSync24_Finish(
              &ui_transition_sync24_frame,w,h,sides,&band);
        left=right=band;
      }else{
        built=1;
      }
      if(built==1){
        if(ui_transition_bounds25_have_prev){
          if(left>ui_transition_bounds25_prev_left)
            left=ui_transition_bounds25_prev_left;
          if(right>ui_transition_bounds25_prev_right)
            right=ui_transition_bounds25_prev_right;
        }
        /* Both side margins use a real one-PHYSICAL-pixel overlap at
         * the original 4:3 seam; never extend beyond a visible tooth.
         * When V24 fallback is involved, clamp its pixels to the same
         * previously proved nonincreasing source-envelope bounds. */
        if(!SpiderNativeBounds25_Build(
               ui_transition_sync24_frame.prototype,w,h,
               left,right,sides))
          built=-1;
        else if(left<.5f && right<.5f)
          built=2;
      }
      if(built==1 || built==2){
        ui_transition_bounds25_have_prev=1u;
        ui_transition_bounds25_prev_left=left;
        ui_transition_bounds25_prev_right=right;
      }
    }
    draw_left=left>=.5f;
    draw_right=right>=.5f;
    ui_transition_bounds25_last_left=left;
    ui_transition_bounds25_last_right=right;
    ui_transition_bounds25_last_fallback=!measured;
  }else{
    built=SpiderNativeSync24_Finish(&ui_transition_sync24_frame,
           (unsigned)desktop_w,(unsigned)desktop_h,sides,&band);
  }
  /* A valid zero-width curtain is the desired last-phase state. */
  if(built==2)return;
  if(built==0){
    if(debug_log_enabled)
      log_line("UI-TRANSITION-SYNC24: NO NONDEGENERATE SOURCE; skip supplemental strips this scene");
    return;
  }
  if(built<0 ||
     InterlockedCompareExchange(&ui_transition_sync24_failed,0,0)==1){
    if(built<0 && debug_log_enabled)
      log_line("UI-TRANSITION-SYNC24: INVALID PHYSICAL GUTTER GEOMETRY; native V19 unchanged");
    return;
  }
  if(FAILED(IDirect3DDevice7_CreateStateBlock(device,D3DSBT_ALL,&saved)) ||
     !saved){ok=0;goto done;}
  if(FAILED(IDirect3DDevice7_GetTexture(device,0,&bound))){
    ok=0;goto done;
  }
#define SYNC24_REQUIRE(call) do {if(FAILED(call))ok=0;} while(0)
  SYNC24_REQUIRE(IDirect3DDevice7_SetTexture(device,0,NULL));
  SYNC24_REQUIRE(IDirect3DDevice7_SetRenderState(device,D3DRENDERSTATE_ZENABLE,D3DZB_FALSE));
  SYNC24_REQUIRE(IDirect3DDevice7_SetRenderState(device,D3DRENDERSTATE_ZWRITEENABLE,FALSE));
  SYNC24_REQUIRE(IDirect3DDevice7_SetRenderState(device,D3DRENDERSTATE_ALPHABLENDENABLE,FALSE));
  SYNC24_REQUIRE(IDirect3DDevice7_SetRenderState(device,D3DRENDERSTATE_ALPHATESTENABLE,FALSE));
  SYNC24_REQUIRE(IDirect3DDevice7_SetRenderState(device,D3DRENDERSTATE_FOGENABLE,FALSE));
  SYNC24_REQUIRE(IDirect3DDevice7_SetRenderState(device,D3DRENDERSTATE_CULLMODE,D3DCULL_NONE));
  SYNC24_REQUIRE(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));
  SYNC24_REQUIRE(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));
  SYNC24_REQUIRE(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));
  SYNC24_REQUIRE(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));
  if(ok && draw_left){
    HRESULT hleft=original_draw_primitive(device,D3DPT_TRIANGLEFAN,
                      0x144UL,sides[0],4u,ui_transition_sync24_flags);
    if(SUCCEEDED(hleft))++drawn;
    else ok=0;
  }
  if(ok && draw_right){
    HRESULT hright=original_draw_primitive(device,D3DPT_TRIANGLEFAN,
                      0x144UL,sides[1],4u,ui_transition_sync24_flags);
    if(SUCCEEDED(hright))++drawn;
    else ok=0;
  }
#undef SYNC24_REQUIRE
done:
  if(saved){
    hr=IDirect3DDevice7_ApplyStateBlock(device,saved);
    if(FAILED(hr))ok=0;
    /* Match the proven earlier EndScene compositors: explicitly
     * rebind the previous texture after ApplyStateBlock. */
    if(bound){
      hr=IDirect3DDevice7_SetTexture(device,0,bound);
      if(FAILED(hr))ok=0;
    }
    IDirect3DDevice7_DeleteStateBlock(device,saved);
  }
  if(bound)IDirectDrawSurface7_Release(bound);
  if(SUCCEEDED(IDirect3DDevice7_GetRenderState(
       device,D3DRENDERSTATE_ZENABLE,&restored_z))){
    InterlockedExchange(&fov_z_state_enabled,restored_z!=D3DZB_FALSE);
    InterlockedExchange(&fov_z_state_known,1);
  }
  if(ok){
    ui_transition_hold22_scene_gutters=drawn;
  }else{
    InterlockedExchange(&ui_transition_sync24_failed,1);
    log_line("UI-TRANSITION-SYNC24: END-SCENE D3D7 DRAW/STATE ERROR, side overlay disabled, native V19 intact");
  }
}

static HRESULT WINAPI ui_compositor_scene_finish(void *self) {
  if(InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)==1){
    unsigned active=ui_transition_hold22_scene_native>0u;
    if(active){
      unsigned frame=++ui_transition_bounds25_frames;
      ui_transition_sync24_finish((IDirect3DDevice7*)self);
      if(debug_log_enabled && (frame<=10u || frame%4u==0u ||
          ui_transition_bounds25_last_left<.5f ||
          ui_transition_bounds25_last_right<.5f)){
        char msg[345];
        snprintf(msg,sizeof(msg),
          "UI-TRANSITION-BOUNDS25: FRAME=%u native=%u eligible-tri=%u matte-skip=%u inner-left=%.2f inner-right=%.2f left-curtain=%.2f right-curtain=%.2f overlap=1.0 fallback=%u drawn=%u [V19 ORIGINAL]",
          frame,ui_transition_sync24_frame.native_fans,
          ui_transition_bounds25_frame.eligible,
          ui_transition_bounds25_frame.excluded_matte,
          (double)ui_transition_bounds25_frame.min_x,
          (double)ui_transition_bounds25_frame.max_x,
          (double)ui_transition_bounds25_last_left,
          (double)ui_transition_bounds25_last_right,
          ui_transition_bounds25_last_fallback,
          ui_transition_hold22_scene_gutters);
        log_line(msg);
      }
    }else if(ui_transition_hold22_last_active){
      ui_transition_bounds25_prev_left=0.0f;
      ui_transition_bounds25_prev_right=0.0f;
      ui_transition_bounds25_have_prev=0u;
      log_line("UI-TRANSITION-BOUNDS25: RELEASE - reset per-transition left/right bounds and black side widths");
    }
    ui_transition_hold22_last_active=active;
  }
  if(InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 &&
     ui_comp_scene_open && ui_comp_device==self)
    ui_comp_flush((IDirect3DDevice7*)self);
  ui_comp_scene_open=0;ui_comp_device=NULL;
  return ui_original_end_scene(self);
}
/* Read-only candidate scan: looks for native full-viewport screen-space
 * quads DURING ESC, excluding known game-owned ESC menu layers. This tests
 * whether the game supplies a distinct screen-covering tint command.
 * Never stretches or recolours any quad, and never assumes a material
 * ID/level/name. A zero count means "none met THIS bound", not "none
 * exists" (it may be tiled or rendered by another API).
 */
static void ui_comp_probe_fullscreen_source(void *device,
                   D3DPRIMITIVETYPE type,DWORD fvf,
                   const void *source,DWORD count,int native_esc)
{
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  float l,r,t,b,w=(float)desktop_w,h=(float)desktop_h;
  unsigned i;
  LONG n;
  DWORD blend=0;
  char msg[276];
  if(!debug_log_enabled || !device || !source ||
     InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)!=1 ||
     !ui_comp_scene_open || ui_comp_bailout || native_esc ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4 ||
     w<640.0f || h<480.0f ||
     !native_pause43_visible_near_flip(
        InterlockedCompareExchange(&flip_count,0,0)))
    return;
  l=r=v[0].x;t=b=v[0].y;
  for(i=0;i<4;i++) {
    if(!(v[i].x>=-w*.1f && v[i].x<=w*1.1f &&
         v[i].y>=-h*.1f && v[i].y<=h*1.1f &&
         v[i].z>=0.0f && v[i].z<.02f))
      return;
    if(v[i].x<l)l=v[i].x;
    if(v[i].x>r)r=v[i].x;
    if(v[i].y<t)t=v[i].y;
    if(v[i].y>b)b=v[i].y;
  }
  if(l>w*.12f || r<w*.88f || t>h*.12f || b<h*.88f)
    return;
  n=InterlockedIncrement(&ui_comp_fullscreen_candidates);
  if(n>16 && n%180!=0)return;
  IDirect3DDevice7_GetRenderState((IDirect3DDevice7*)device,
                                   D3DRENDERSTATE_ALPHABLENDENABLE,&blend);
  snprintf(msg,sizeof(msg),
    "UI-COMPOSITOR-COVERAGE-PROBE: count=%ld source-box=(%.1f..%.1f,%.1f..%.1f) color=%08lX z=%.5f rhw=%.4f alpha-blend=%lu [READ-ONLY; NON-ESC]",
    (long)n,(double)l,(double)r,(double)t,(double)b,
    (unsigned long)v[0].diffuse,(double)v[0].z,(double)v[0].rhw,
    (unsigned long)blend);
  log_line(msg);
}

/* V18 high priority: play back the ORIGINAL game's black iris draws,
 * in ORIGINAL order, with ORIGINAL vertex animation and render states.
 * The only intervention is centering their 4:3 X coordinates within the
 * physical 16:9 world. The original game remains the renderer/animator.
 * Any unexpected material is passed through UNMODIFIED (not V12/V17).
 */
/* Final V19/V25: one immutable game-owned iris fan per real
 * DrawPrimitive, centered 4:3 X transform only. The two side curtains
 * are calculated at EndScene from CURRENT mapped original polygon bounds.
 * Unsafe/unrecognised material passes to its original draw unchanged.
 */
static int ui_transition_native43_handle_draw(
    void *device,D3DPRIMITIVETYPE type,DWORD fvf,LPVOID verts,
    DWORD count,DWORD flags,void *caller,HRESULT *result)
{
  DWORD rva=0,blend=1,atest=1;
  IDirectDrawSurface7 *tex=NULL;
  HRESULT a,b,c;
  SpiderMenuVertex mapped[4];
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)verts;
  LONG n;
  unsigned i;
  int valid=1,reject=0;
  if(InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)!=1 ||
     !device || !verts || !result || type!=D3DPT_TRIANGLEFAN ||
     fvf!=0x144UL || count!=4u || !geom_rva(caller,&rva) ||
     rva!=SPIDER_TRANSITION_CALLER_RVA ||
     v[0].diffuse!=0x00000000UL)
    return 0;
  if(!ui_comp_scene_open || ui_comp_device!=device ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     InterlockedCompareExchange(&fov_game_height,0,0)<=0){
    valid=0;reject=1;
  }
  for(i=0u;i<4u;i++)
    if(v[i].diffuse!=0x00000000UL ||
       !(v[i].z>=-.00004f && v[i].z<=.00004f &&
         v[i].rhw>=27.52f && v[i].rhw<=27.68f)){
      valid=0;reject=2;
    }
  if(valid && !SpiderNative43_MapBounded(v,mapped,4u,
                    (unsigned)desktop_w,(unsigned)desktop_h,32.0f)){
    valid=0;reject=3;
  }
  if(valid){
    a=IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex);
    b=IDirect3DDevice7_GetRenderState((IDirect3DDevice7*)device,
                     D3DRENDERSTATE_ALPHABLENDENABLE,&blend);
    c=IDirect3DDevice7_GetRenderState((IDirect3DDevice7*)device,
                     D3DRENDERSTATE_ALPHATESTENABLE,&atest);
    if(FAILED(a)||FAILED(b)||FAILED(c)||tex||blend||atest){
      valid=0;reject=4;
    }
    if(tex)IDirectDrawSurface7_Release(tex);
  }
  *result=original_draw_primitive(device,type,fvf,
                 valid?(LPVOID)mapped:verts,count,flags);
  if(valid && SUCCEEDED(*result)){
    unsigned was_ready=ui_transition_sync24_frame.ready;
    SpiderNativeSync24_Observe(&ui_transition_sync24_frame,v,
             (unsigned)desktop_w,(unsigned)desktop_h);
    SpiderNativeBounds25_Observe(&ui_transition_bounds25_frame,mapped,
             (unsigned)desktop_w,(unsigned)desktop_h);
    if(!was_ready && ui_transition_sync24_frame.ready)
      ui_transition_sync24_flags=flags;
    ++ui_transition_hold22_scene_native;
    n=InterlockedIncrement(&ui_transition_native43_draws);
    if(debug_log_enabled && (n<=12 || n%360==0)){
      char msg[210];
      snprintf(msg,sizeof(msg),
        "UI-TRANSITION-NATIVE43: DRAW=%ld x=(%.2f -> %.2f) y=%.2f [ORIGINAL NATIVE GAME; NO RECOMPOSITION]",
        (long)n,(double)v[0].x,(double)mapped[0].x,(double)v[0].y);
      log_line(msg);
    }
  }else if(!valid){
    n=InterlockedIncrement(&ui_transition_native43_native);
    if(debug_log_enabled && (n<=12 || n%360==0)){
      char msg[240];
      snprintf(msg,sizeof(msg),
        "UI-TRANSITION-NATIVE43: NATIVE-UNCHANGED=%ld reason=%d [1=scene 2=vertex 3=extents 4=material] x=%.1f y=%.1f [UNMODIFIED ORIGINAL]",
        (long)n,reject,(double)v[0].x,(double)v[0].y);
      log_line(msg);
    }
  }
  return 1;
}

static HRESULT WINAPI hooked_draw_primitive(void *self,D3DPRIMITIVETYPE type,DWORD fvf,LPVOID verts,DWORD count,DWORD flags) {
  HRESULT native43_result;
  if(ui_transition_native43_handle_draw(self,type,fvf,verts,count,flags,
              __builtin_return_address(0),&native43_result))
    return native43_result;
  if(InterlockedCompareExchange(&menu_upscale_active,0,0)==1 &&
     fvf==0x144UL && verts && count>=3 && count<=128)
    InterlockedIncrement(&menu_version_draws);
  {
    SpiderMenuVertex small[64],*allocated=NULL;
    int native_esc_primitive=0;
    const void *v=scale_menu_vertices(fvf,verts,count,small,64,&allocated);
    if(v==verts && count==4) {
      v=maybe_correct_native_comic(self,type,fvf,verts,count,
                                   __builtin_return_address(0),small);
    }
    int verified_hud_canvas=0;
    /* In the unified trial, bypass older fragment-level HUD scaling. */
    if(InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)!=1) {
    /* BD has precedence and a separate signature; never rescale it. */
    if(v==verts && count==4)
      v=maybe_correct_adaptive_hud(self,type,fvf,verts,count,
                                   __builtin_return_address(0),small);
    /* A second, independently observed boss-HUD variant: right-side
     * 4:3 UI corrected about screen center. The previous adaptive
     * variant retains precedence and its old right-edge policy. */
    if(v==verts && count==4)
      v=maybe_correct_native_boss_hud(self,type,fvf,verts,count,
                                      __builtin_return_address(0),small);
    }
    /* Correct the native translucent motif. Where these same alpha
     * primitives previously tinted the entire 16:9 frame, replay the
     * original triangles ONLY into the two outer 240px gutters first.
     * Central motif and original banner/cursor stay unchanged. */
    if(v==verts && count==4) {
      v=maybe_correct_native_motif43(self,type,fvf,verts,count,
                                    __builtin_return_address(0),small);
      if(v!=verts) native_esc_primitive=SPIDER_UI_COMP_PAUSE_DECOR;
      if(v!=verts && InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)!=1)
        native_motif43_restore_gutters(self,(const SpiderMenuVertex*)verts,flags);
    }
    /* Native red selector uses exact texture and quad signature.
     * The older Wine stack-owner hypothesis has been retired. */
    if(v==verts && count==4) {
      v=maybe_correct_native_cursor43(self,type,fvf,verts,count,
                                      __builtin_return_address(0),small);
      if(v!=verts) native_esc_primitive=SPIDER_UI_COMP_PAUSE_MENU;
    }
    /* Native ESC after known comic/HUD precedence. */
    if(v==verts && count==4) {
      v=maybe_correct_native_pause43(self,type,fvf,verts,count,
                                     __builtin_return_address(0),small);
      if(v!=verts) native_esc_primitive=SPIDER_UI_COMP_PAUSE_MENU;
    }
    /* The original native Pause glyphs can have different per-level RHW:
     * capture original data regardless of whether the strict HUD path
     * corrected this draw, once a verified ribbon is present. */
    if(count==4) {
      maybe_log_native_pause43_detail(self,type,fvf,verts,count,
                       __builtin_return_address(0),v!=verts);
      maybe_log_native_pause43_border(self,type,fvf,verts,count,
                       __builtin_return_address(0),v!=verts);
    }
    /* Source-only boss UI probe remains active whether or not the
     * previous adaptive HUD handler already corrected a known boss. */
    if(count==4)
      maybe_log_boss_hud_candidate(self,type,fvf,verts,count,
                                  __builtin_return_address(0),v!=verts);
    /* Uncorrected suspected aim/ribbon draws only. No rendering changes. */
    if(v==verts && count==4)
      maybe_log_hud_visual_candidate(self,type,fvf,verts,count,
                                     __builtin_return_address(0));
    /* Strict user-verified 64x64 red/green aim sprites only, after
     * compass/native ESC precedence and before depth-tested Hor+. */
    if(v==verts && count==4)
      v=maybe_correct_aim_reticle(self,type,fvf,verts,count,
                                  __builtin_return_address(0),small);
    /* The game-owned iris is handled before other HUD/ESC draws. */
    if(v==verts && count==4) {
      const void *candidate=maybe_correct_unified_ui_canvas(
        type,fvf,verts,count,__builtin_return_address(0),small);
      if(candidate!=verts) {
        maybe_probe_pause_hud_layer(self,(const SpiderMenuVertex*)candidate);
        /* The generic screen-space classifier already excluded
         * fullscreen effects, cinematics and native ESC elements.
         * It identifies the same gameplay HUD quads whose colours were
         * unintentionally altered by the EndScene veil. */
        verified_hud_canvas=1;
      }
      v=candidate;
    }
    if(v==verts) v=maybe_correct_horplus(type,fvf,verts,count,small);
    /* Probe any genuine independent screen-size native tint candidate.
     * Log original source geometry, never a corrected copy. */
    ui_comp_probe_fullscreen_source(self,type,fvf,verts,count,native_esc_primitive);
    /* Check for a separate native HUD pass submitted after our
     * EndScene replay but before the next Flip. Such draws could appear
     * above the spider regardless of the compositor's pass order.
     * This probe logs ONLY; it never changes their game-owned drawing. */
    if(debug_log_enabled &&
       InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 &&
       InterlockedCompareExchange(&ui_comp_last_overlay_flip,0,0)==
         InterlockedCompareExchange(&flip_count,0,0) &&
       count==4 && fvf==0x144UL && type==D3DPT_TRIANGLEFAN &&
       !native_esc_primitive && verts) {
      DWORD late_rva=0;
      const SpiderMenuVertex *candidate=(const SpiderMenuVertex*)verts;
      if(candidate[0].z>=0.0f && candidate[0].z<0.02f &&
         geom_rva(__builtin_return_address(0),&late_rva) &&
         late_rva==0x00102C72UL) {
        LONG sample=InterlockedIncrement(&ui_comp_late_draw_samples);
        if(sample<=16) {
          char msg[270];
          snprintf(msg,sizeof(msg),
            "UI-COMPOSITOR-LATE-DRAW: n=%ld flip=%ld x=%.1f y=%.1f z=%.5f rhw=%.4f ARGB=%08lX [ORIGINAL POST-PAUSE PRIMITIVE; DIAGNOSTIC ONLY]",
            (long)sample,(long)InterlockedCompareExchange(&flip_count,0,0),
            (double)candidate[0].x,(double)candidate[0].y,
            (double)candidate[0].z,(double)candidate[0].rhw,
            (unsigned long)candidate[0].diffuse);
          log_line(msg);
        }
      }
    }
    /* Deferred ESC composition is active ONLY under both opt-in flags,
     * inside a confirmed game-owned BeginScene/EndScene pair. Otherwise
     * keep the original draw path (even if state blocks are unsupported).
     * The old depth-only experiment stays disabled in compositor mode. */
    HRESULT hr;
    if(!native_esc_primitive && verified_hud_canvas &&
       InterlockedCompareExchange(&ui_compositor_hud_over_veil_enabled,0,0)==1 &&
       native_pause43_visible_near_flip(
          InterlockedCompareExchange(&flip_count,0,0)) &&
       type==D3DPT_TRIANGLEFAN && fvf==0x144UL &&
       ui_comp_capture((IDirect3DDevice7*)self,type,fvf,v,verts,count,flags,
                       SPIDER_UI_COMP_HUD)) {
      /* HUD pixels must NOT be drawn before the translucent backdrop,
       * or they will be dimmed and then double-blended on replay. */
      hr=D3D_OK;
    } else if(native_esc_primitive && v!=verts &&
       ui_comp_capture((IDirect3DDevice7*)self,type,fvf,v,verts,count,flags,
                       native_esc_primitive)) {
      hr=D3D_OK;
    } else if(native_esc_primitive &&
       InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)!=1)
      hr=draw_native_esc_in_front(self,type,fvf,(LPVOID)v,
                                 count,flags,native_esc_primitive);
    else
      hr=original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags);
    free_menu_vertices(allocated);
    return hr;
  }
}
static HRESULT WINAPI hooked_draw_indexed_primitive(void *self,D3DPRIMITIVETYPE type,DWORD fvf,LPVOID verts,DWORD count,LPWORD indices,DWORD icount,DWORD flags) {
  if(InterlockedCompareExchange(&menu_upscale_active,0,0)==1 &&
     fvf==0x144UL && verts && count>=3 && count<=128)
    InterlockedIncrement(&menu_version_draws);
  {
    SpiderMenuVertex small[64],*allocated=NULL;
    const void *v=scale_menu_vertices(fvf,verts,count,small,64,&allocated);
    /* No indexed comic path was observed: keep it unchanged. */
    HRESULT hr=original_draw_indexed_primitive(self,type,fvf,(LPVOID)v,count,indices,icount,flags);
    free_menu_vertices(allocated);
    return hr;
  }
}
/* Diagnostic: native SetViewport while preserving render-state/draw hooks. */
static void instrument_d3d_device(IDirect3DDevice7 *device) {
  void ***obj=(void ***)device; void **vt;
  /* Native Windows startup A/B test. Only when explicitly requested, leave
   * the COM device's ORIGINAL vtable in place. This disables all of our
   * device-level viewport, draw and render-state hooks while preserving the
   * external DirectDraw hooks. Never change the default rendering path. */
  {
    char value[4]={0};
    DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE",value,sizeof(value));
    if(n==1 && value[0]=='1') {
      log_line("D3D7-DIAG: native device vtable, viewport/draw/render-state hooks bypassed");
      return;
    }
  }
  if(!obj || !(vt=*obj)) return;
  if(InterlockedCompareExchange(&device_ready,1,0)==0) {
    memcpy(patched_device_vtable,vt,sizeof(patched_device_vtable));
    original_set_viewport=(D3DViewportFn)vt[13];
    /* DirectX 7 SDK IDirect3DDevice7 vtable: BeginScene slot 5,
     * EndScene slot 6. Hook only under explicit compositor trial. */
    ui_original_begin_scene=(D3DSceneFn)vt[5];
    ui_original_end_scene=(D3DSceneFn)vt[6];
    original_set_render_state=(D3DSetRenderStateFn)vt[20];
    {
      DWORD zmode=0;
      if(SUCCEEDED(IDirect3DDevice7_GetRenderState(device,D3DRENDERSTATE_ZENABLE,&zmode))) {
        InterlockedExchange(&fov_z_state_enabled,zmode!=D3DZB_FALSE);
        InterlockedExchange(&fov_z_state_known,1);
      }
    }
    patched_device_vtable[13]=(void*)hooked_set_viewport;
    patched_device_vtable[20]=(void*)hooked_set_render_state;
    /* IDirect3DDevice7 vtable positions verified against DirectX d3d.h. */
    original_draw_primitive=(DrawPrimitiveFn)vt[25];
    original_draw_indexed_primitive=(DrawIndexedPrimitiveFn)vt[26];
    patched_device_vtable[25]=(void*)hooked_draw_primitive;
    patched_device_vtable[26]=(void*)hooked_draw_indexed_primitive;
    log_line("D3D7: device vtable captured; viewport and draw hooks enabled");
    InterlockedExchange(&device_ready,2);
  }
  /* On native Windows, a cloned 49-slot COM vtable caused a null
   * indirect call within d3dim700!SetViewport. Keep the original vtable
   * pointer and all unmodified entries; replace just the four hooks.
   * This is the default rendering path on Windows and Wine. Do NOT fall
   * back to the copied vtable if the native vtable cannot be patched.
   */
  if(device_ready==2 && vt[13]==(void*)original_set_viewport &&
     vt[20]==(void*)original_set_render_state &&
     vt[25]==(void*)original_draw_primitive &&
     vt[26]==(void*)original_draw_indexed_primitive &&
     vt[5]==(void*)ui_original_begin_scene &&
     vt[6]==(void*)ui_original_end_scene) {
    DWORD protection=0,unused=0;
    if(VirtualProtect(vt,sizeof(patched_device_vtable),
                      PAGE_READWRITE,&protection)) {
      vt[13]=(void*)hooked_set_viewport;
      vt[20]=(void*)hooked_set_render_state;
      vt[25]=(void*)hooked_draw_primitive;
      vt[26]=(void*)hooked_draw_indexed_primitive;
      if(InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 ||
         InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)==1) {
        vt[5]=(void*)ui_compositor_scene_start;
        vt[6]=(void*)ui_compositor_scene_finish;
      }
      if(!VirtualProtect(vt,sizeof(patched_device_vtable),protection,&unused))
        log_line("D3D7: WARNING native vtable protection restore failed");
      log_line((InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)==1 ||
          InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)==1) ?
          "D3D7: native device vtable patched in place (four original hooks + scoped BeginScene/EndScene for V25/compositor)" :
          "D3D7: native device vtable patched in place (default; all four hooks ON)");
    } else {
      char line[160];
      snprintf(line,sizeof(line),
        "D3D7: ERROR native vtable VirtualProtect failed Win32=%lu; leaving device unmodified",
        (unsigned long)GetLastError());
      log_line(line);
    }
  } else if(device_ready==2 &&
            vt[13]==(void*)hooked_set_viewport &&
            vt[20]==(void*)hooked_set_render_state &&
            vt[25]==(void*)hooked_draw_primitive &&
            vt[26]==(void*)hooked_draw_indexed_primitive &&
            ((InterlockedCompareExchange(&ui_compositor_trial_enabled,0,0)!=1 &&
              InterlockedCompareExchange(&ui_transition_bounds25_enabled,0,0)!=1) ||
              (vt[5]==(void*)ui_compositor_scene_start &&
               vt[6]==(void*)ui_compositor_scene_finish))) {
    /* COM object shares a vtable that was already patched by this fix. */
  } else {
    log_line("D3D7: WARNING device vtable differs from captured hooks; leaving pointer untouched");
  }
}
static HRESULT WINAPI hooked_d3d_create_device(void *self,REFCLSID clsid,IDirectDrawSurface7 *target,IDirect3DDevice7 **out) {
  HRESULT hr=original_d3d_create_device(self,clsid,target,out);
  { char line[180]; snprintf(line,sizeof(line),"D3D7: CreateDevice hr=0x%08lX device=%p",(unsigned long)hr,(void*)(out?*out:NULL)); log_line(line); }
  if(SUCCEEDED(hr) && out && *out) instrument_d3d_device(*out);
  return hr;
}
static void instrument_d3d(IDirect3D7 *d3d) {
  void ***obj=(void ***)d3d;void **vt;
  if(!obj || !(vt=*obj)) return;
  if(InterlockedCompareExchange(&d3d_ready,1,0)==0) {
    memcpy(patched_d3d_vtable,vt,sizeof(patched_d3d_vtable));
    original_d3d_create_device=(D3DCreateDeviceFn)vt[4];
    patched_d3d_vtable[4]=(void*)hooked_d3d_create_device;
    log_line("D3D7: interface captured; CreateDevice hook enabled");
    InterlockedExchange(&d3d_ready,2);
  }
  if(d3d_ready==2 && vt[4]==(void*)original_d3d_create_device) *obj=patched_d3d_vtable;
}
static HRESULT WINAPI hooked_dd_query(void *self,REFIID iid,void **out) {
  HRESULT hr=original_dd_query(self,iid,out);
  if(SUCCEEDED(hr) && out && *out && iid && IsEqualGUID(iid,&IID_IDirect3D7)) {
    log_line("D3D7: IDirectDraw7::QueryInterface(IDirect3D7) succeeded");
    instrument_d3d((IDirect3D7*)*out);
  }
  return hr;
}

static void log_surface_desc(const DDSURFACEDESC2 *d) {
  char line[320];
  if(!d) { log_line("DDRAW: CreateSurface descriptor=NULL"); return; }
  snprintf(line,sizeof(line),
    "DDRAW: CreateSurface dwSize=%lu flags=0x%08lX caps=0x%08lX caps2=0x%08lX %lux%lu backbuffers=%lu bpp=%lu",
    (unsigned long)d->dwSize,(unsigned long)d->dwFlags,
    (unsigned long)d->ddsCaps.dwCaps,(unsigned long)d->ddsCaps.dwCaps2,
    (unsigned long)d->dwWidth,(unsigned long)d->dwHeight,
    (unsigned long)d->dwBackBufferCount,(unsigned long)d->ddpfPixelFormat.dwRGBBitCount);
  log_line(line);
}
/* Windowed PRIMARY is not necessarily described with DDSCAPS_PRIMARYSURFACE
 * by Wine/DirectDraw7 (observed: 0x10006040). Match our actual surfaces
 * instead of the obsolete exclusive flip-chain caps mask. */
static int borderless_intro_output_surface(const void *surface) {
  return surface &&
    (surface==(const void *)borderless_primary ||
     surface==(const void *)borderless_back);
}
/* Apply real pillarbox/letterbox margins, not a 1-pixel edge cover-up.
 * This is only called for a recognised full-frame opening movie Blt.
 * DirectDraw PRIMARY coordinates are desktop-based, unlike BACKBUFFER. */
static void borderless_intro_fill_black_bars(void *surface,const RECT *image) {
  RECT bars[4]; unsigned count=0,i;
  DDBLTFX fill;
  if(!surface || !image || !original_blt || !desktop_w || !desktop_h) return;
  if(image->left>0) {
    RECT r={0,0,image->left,(LONG)desktop_h};bars[count++]=r;
  }
  if(image->right<(LONG)desktop_w) {
    RECT r={image->right,0,(LONG)desktop_w,(LONG)desktop_h};bars[count++]=r;
  }
  if(image->top>0) {
    RECT r={image->left,0,image->right,image->top};bars[count++]=r;
  }
  if(image->bottom<(LONG)desktop_h) {
    RECT r={image->left,image->bottom,image->right,(LONG)desktop_h};bars[count++]=r;
  }
  memset(&fill,0,sizeof(fill));fill.dwSize=sizeof(fill);fill.dwFillColor=0;
  for(i=0;i<count;i++) {
    HRESULT hr;
    RECT r=bars[i];
    if(surface==(void*)borderless_primary)
      OffsetRect(&r,borderless_monitor_x,borderless_monitor_y);
    hr=original_blt(surface,&r,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fill);
    if(FAILED(hr)) {
      static volatile LONG errors=0;
      if(InterlockedIncrement(&errors)<=8) {
        char msg[180];
        snprintf(msg,sizeof(msg),
          "BORDERLESS-INTRO: WARNING black bar fill failed hr=0x%08lX",
          (unsigned long)hr);
        log_line(msg);
      }
    }
  }
}
/* Log bounded, read-only presentation heartbeats independently on Blt and
 * Flip, with local wall-clock timestamps supplied by log_line(). During
 * 3D cinematic stalls, these samples show whether only Flip stops or
 * whether both original DirectDraw callbacks stop. The source/destination
 * surfaces are NEVER queried, locked or modified here.
 *
 * Last-sample fields are atomic: Wine may invoke Blt on a different
 * thread from Flip. This observational probe is OFF by default. */
static void focus_timeline_pulse(int is_flip,LONG seq,void *surface) {
  static volatile LONG last_blt_call=0,last_flip_call=0;
  static volatile LONG last_blt_sample=0,last_flip_sample=0;
  volatile LONG *last_call=is_flip?&last_flip_call:&last_blt_call;
  volatile LONG *last_sample=is_flip?&last_flip_sample:&last_blt_sample;
  DWORD now,prior,sampled,gap;
  HWND fg;
  DWORD pid=0;
  char line[260];
  if(!focus_timeline_enabled)return;
  now=GetTickCount();
  prior=(DWORD)InterlockedExchange(last_call,(LONG)now);
  gap=prior?(DWORD)(now-prior):0;
  sampled=(DWORD)InterlockedCompareExchange(last_sample,0,0);
  /* Avoid calling Win32 foreground APIs on every hot-path movie Blt.
   * A new foreground can be observed at the next <=250 ms sample;
   * the separate WndProc probe records precise event times. */
  if(prior && gap<750u && sampled && (DWORD)(now-sampled)<250u)return;
  InterlockedExchange(last_sample,(LONG)now);
  fg=GetForegroundWindow();
  if(fg)GetWindowThreadProcessId(fg,&pid);
  snprintf(line,sizeof(line),
    "FOCUS-TIMELINE: callback=%s seq=%ld gap-ms=%lu foreground=%p "
    "foreign-pid=%lu game=%p fg-is-game=%d surface-is-primary=%d "
    "menu-active=%d movie-active=%d thread=%lu",
    is_flip?"Flip":"Blt",(long)seq,(unsigned long)gap,(void*)fg,
    (unsigned long)pid,(void*)game_hwnd,(int)(fg==game_hwnd),
    (int)(surface==(void*)borderless_primary),
    (int)(InterlockedCompareExchange(&menu_upscale_active,0,0)==1),
    (int)((*(const volatile DWORD *)(uintptr_t)0x006151F8u)!=0u),
    (unsigned long)GetCurrentThreadId());
  log_line(line);
}
static HRESULT WINAPI hooked_blt(void *self, LPRECT dest, IDirectDrawSurface7 *source,
                                  LPRECT source_rect, DWORD flags, LPDDBLTFX fx) {
  HRESULT hr; LONG count=InterlockedIncrement(&blt_count);
  RECT dest_mapped;
  LPRECT presented_dest=dest;
  BOOL f9_movie_present=FALSE;

  focus_timeline_pulse(0,count,self);
  if(debug_log_enabled && count<=24) {
    char line[240];
    snprintf(line,sizeof(line),"DDRAW: Surface7::Blt #%ld dst=%p src=%p dstRect=%s srcRect=%s flags=0x%08lX",
             (long)count,self,(void*)source,dest?"set":"NULL",source_rect?"set":"NULL",(unsigned long)flags);
    log_line(line);
  }
  /* Mandatory opening-FMV presentation. Always 4:3, in both Auto and 4:3
   * display modes. Exactly the observed 640x480, 320x192 and 320x240
   * decoded surfaces, only during the legacy menu/intro stage. */
  if (source && flip_count>0 &&
      InterlockedCompareExchange(&menu_upscale_active,0,0)==1 &&
      InterlockedCompareExchange(&media_viewport_4x3_enabled,0,0)==1) {
    DDSURFACEDESC2 sd,dd; HRESULT shr,dhr;
    memset(&sd,0,sizeof(sd)); sd.dwSize=sizeof(sd);
    memset(&dd,0,sizeof(dd)); dd.dwSize=sizeof(dd);
    shr=IDirectDrawSurface7_GetSurfaceDesc(source,&sd);
    dhr=IDirectDrawSurface7_GetSurfaceDesc((IDirectDrawSurface7*)self,&dd);

    if (InterlockedCompareExchange(&media_native_blt_4x3_enabled,0,0)==1 &&
        !dest && !source_rect && SUCCEEDED(shr) && SUCCEEDED(dhr) &&
        ((sd.dwWidth==640 && sd.dwHeight==480) ||
         (InterlockedCompareExchange(&media_native_blt_knownsizes_enabled,0,0)==1 &&
          sd.dwWidth==320 && (sd.dwHeight==192 || sd.dwHeight==240)) ||
         (InterlockedCompareExchange(&media_native_blt_anysize_enabled,0,0)==1 &&
          sd.dwWidth>=160 && sd.dwHeight>=120 &&
          sd.dwWidth<=4096 && sd.dwHeight<=4096 &&
          (sd.dwWidth!=desktop_w || sd.dwHeight!=desktop_h))) &&
        dd.dwWidth==desktop_w && dd.dwHeight==desktop_h &&
        borderless_intro_output_surface(self) &&
        menu_map.left>=0 && menu_map.top>=0 &&
        menu_map.right<=(int)desktop_w && menu_map.bottom<=(int)desktop_h &&
        menu_map.right>menu_map.left && menu_map.bottom>menu_map.top) {
      LONG hits=InterlockedIncrement(&media_native_blt_4x3_hits);
      InterlockedIncrement(&menu_version_movie_blits);
      InterlockedExchange(&menu_version_last_movie_flip,
        InterlockedCompareExchange(&flip_count,0,0));
      dest_mapped.left=menu_map.left;
      dest_mapped.top=menu_map.top;
      dest_mapped.right=menu_map.right;
      dest_mapped.bottom=menu_map.bottom;
      /* The source pixel-count ratio 320:192 is 5:3; the requested
       * opening-video presentation is nonetheless always 4:3. */
      borderless_intro_fill_black_bars(self,&dest_mapped);
      presented_dest=&dest_mapped;
      f9_movie_present=TRUE;
      if (debug_log_enabled && (hits<=8 || hits%120==0 ||
          ((sd.dwWidth!=640 || sd.dwHeight!=480) && hits<=1024 && hits%30==0))) {
        char msg[260];
        snprintf(msg,sizeof(msg),
          "DISPLAY-MEDIA: FINAL BLT REMAP #%ld: src=%lux%lu -> native dest (%ld,%ld)-(%ld,%ld) [ACTIVE%s]",
          (long)hits,(unsigned long)sd.dwWidth,(unsigned long)sd.dwHeight,
          (long)dest_mapped.left,(long)dest_mapped.top,
          (long)dest_mapped.right,(long)dest_mapped.bottom,
          (sd.dwWidth==640 && sd.dwHeight==480)?" BORDERLESS-4:3":
             " BORDERLESS-4:3 KNOWN-SIZE");
        log_line(msg);
      }
    }
  }
  if(self==(void*)borderless_primary && presented_dest) {
    dest_mapped=*presented_dest;
    OffsetRect(&dest_mapped,borderless_monitor_x,borderless_monitor_y);
    presented_dest=&dest_mapped;
  }
  hr=original_blt(self,presented_dest,source,source_rect,flags,fx);
  /* Only retain surfaces actually presented as recognized intro video.
   * Same game thread as F9; no worker accesses DirectDraw objects. */
  if(SUCCEEDED(hr) && f9_movie_present && source) {
    /* A decoded intro video can continue even with the original
     * 0x006151F8 'movie active' bit zero. Always record the actual
     * successful movie presentation for F12 shutdown safety. */
    SpiderQuickExitF12_ObserveMovieFrame();
    if(source!=f9_movie_source) {
      IDirectDrawSurface7_AddRef(source);
      if(f9_movie_source) IDirectDrawSurface7_Release(f9_movie_source);
      f9_movie_source=source;
    }
    InterlockedExchange(&f9_movie_last_flip,
                        InterlockedCompareExchange(&flip_count,0,0));
  }
  if(debug_log_enabled && count<=24) {
    char line[100]; snprintf(line,sizeof(line),"DDRAW: Surface7::Blt result=0x%08lX",(unsigned long)hr); log_line(line);
  }
  return hr;
}
/* The surface-creation hook remains responsible for installing the shared vtable. */
/* Capture the F9 window without changing the game keyboard path. Holding
 * the real F9 key for a moment starts six seconds of detailed readback logs.
 * First 24 events are also logged to identify the normal baseline.
 * Do not use the F12 HUD sampler here; this diagnostic is independent. */
static int f9_readback_window(void) {
  DWORD tick;
  LONG deadline;
  if(InterlockedCompareExchange(&f9_trace_enabled,0,0)!=1) return 0;
  tick=GetTickCount();
  if(GetAsyncKeyState(VK_F9)&0x8000) {
    if(InterlockedCompareExchange(&f9_trace_keydown,1,0)==0) {
      InterlockedExchange(&f9_trace_until,(LONG)(tick+6000u));
      InterlockedExchange(&f9_trace_logged,0);
      log_line("F9-TRACE: F9 key pressed; readback trace window OPEN for six seconds");
    }
  } else InterlockedExchange(&f9_trace_keydown,0);
  deadline=InterlockedCompareExchange(&f9_trace_until,0,0);
  return deadline!=0 && (LONG)((DWORD)deadline-tick)>0;
}
static int f9_trace_report(void) {
  LONG event;
  int during;
  if(InterlockedCompareExchange(&f9_trace_enabled,0,0)!=1) return 0;
  during=f9_readback_window();
  event=InterlockedIncrement(&f9_trace_events);
  if(during) return InterlockedIncrement(&f9_trace_logged)<=200;
  return event<=24; /* Baseline: first few reads after game startup. */
}
static const char *f9_surface_kind(const void *surface) {
  if(surface==(const void *)borderless_primary) return "PRIMARY";
  if(surface==(const void *)borderless_back) return "3D-BACKBUFFER";
  return "OTHER";
}
static HRESULT WINAPI hooked_f9_surface_get_dc(void *self,HDC *dc) {
  HRESULT hr=original_get_dc(self,dc);
  if(f9_trace_report()) {
    DWORD rva=0; char msg[265];
    geom_rva(__builtin_return_address(0),&rva);
    snprintf(msg,sizeof(msg),
      "F9-TRACE: GetDC owner=%s surface=%p caller=0x%08lX hr=0x%08lX hdc=%p",
      f9_surface_kind(self),self,(unsigned long)rva,(unsigned long)hr,
      (void *)(dc?*dc:NULL));
    log_line(msg);
  }
  return hr;
}
static HRESULT WINAPI hooked_f9_surface_get_desc(void *self,LPDDSURFACEDESC2 desc) {
  HRESULT hr=original_surface_get_desc(self,desc);
  if(f9_trace_report()) {
    DWORD rva=0;char msg[310];
    geom_rva(__builtin_return_address(0),&rva);
    snprintf(msg,sizeof(msg),
      "F9-TRACE: GetSurfaceDesc owner=%s surface=%p caller=0x%08lX hr=0x%08lX "
      "size=%lux%lu bpp=%lu caps=0x%08lX",
      f9_surface_kind(self),self,(unsigned long)rva,(unsigned long)hr,
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->dwWidth:0),
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->dwHeight:0),
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->ddpfPixelFormat.dwRGBBitCount:0),
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->ddsCaps.dwCaps:0));
    log_line(msg);
  }
  return hr;
}
static HRESULT WINAPI hooked_f9_surface_lock(void *self,LPRECT rect,
                                              LPDDSURFACEDESC2 desc,
                                              DWORD flags,HANDLE event) {
  HRESULT hr;
  DWORD rva=0;
  geom_rva(__builtin_return_address(0),&rva);
  if(InterlockedCompareExchange(&f9_backbuffer_fix_enabled,0,0)==1 &&
     self==(void *)borderless_primary && borderless_back &&
     rva==0x001034D4u && (flags&DDLOCK_READONLY)!=0 &&
     rect==NULL && desc && desc->dwSize==sizeof(*desc) &&
     desktop_w>=640 && desktop_h>=480 &&
     InterlockedCompareExchange(&f9_backbuffer_lock_active,1,0)==0) {
    IDirectDrawSurface7 *back=borderless_back;
    DDSURFACEDESC2 requested=*desc;
    HRESULT back_hr;
    char msg[300];
    IDirectDrawSurface7_AddRef(back);
    /* Same native 32-bit XY layout and full-screen size expected from
     * the original PRIMARY readback. No extra Blt or frame clearing. */
    back_hr=original_surface_lock(back,rect,desc,flags,event);
    if(SUCCEEDED(back_hr) &&
       desc->dwWidth==desktop_w && desc->dwHeight==desktop_h &&
       desc->ddpfPixelFormat.dwRGBBitCount==32 &&
       desc->lPitch>=(LONG)(desktop_w*4u) &&
       desc->lpSurface!=NULL) {
      f9_backbuffer_locked_surface=back;
      f9_backbuffer_lock_thread=GetCurrentThreadId();
      snprintf(msg,sizeof(msg),
        "F9-FIX: BACKBUFFER Lock success primary=%p back=%p "
        "caller=0x%08lX size=%lux%lu pitch=%ld; redirect one Unlock",
        self,(void *)back,(unsigned long)rva,
        (unsigned long)desc->dwWidth,(unsigned long)desc->dwHeight,
        (long)desc->lPitch);
      log_line(msg);
      /* Historical readback-comparison code is inert in normal play.
       * The always-on F9 PNG path bypasses the original BMP writer. */
      if(InterlockedCompareExchange(&f9_native_png_enabled,0,0)==1)
        SpiderF9PNG_Queue(desc,log_line);
      return back_hr;
    }
    if(SUCCEEDED(back_hr))
      original_surface_unlock(back,rect);
    IDirectDrawSurface7_Release(back);
    InterlockedExchange(&f9_backbuffer_lock_active,0);
    *desc=requested; /* The fallback PRIMARY Lock owns its descriptor. */
    snprintf(msg,sizeof(msg),
      "F9-FIX: backbuffer Lock/format rejected hr=0x%08lX; "
      "falling back to untouched PRIMARY screenshot",
      (unsigned long)back_hr);
    log_line(msg);
  }
  hr=original_surface_lock(self,rect,desc,flags,event);
  if(f9_trace_report()) {
    DWORD rva=0;char msg[430];
    geom_rva(__builtin_return_address(0),&rva);
    snprintf(msg,sizeof(msg),
      "F9-TRACE: Lock owner=%s surface=%p caller=0x%08lX hr=0x%08lX flags=0x%08lX "
      "rect=%s size=%lux%lu pitch=%ld ptr=%p bpp=%lu masks=%08lX/%08lX/%08lX",
      f9_surface_kind(self),self,(unsigned long)rva,
      (unsigned long)hr,(unsigned long)flags,rect?"set":"NULL",
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->dwWidth:0),
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->dwHeight:0),
      (long)((SUCCEEDED(hr)&&desc)?desc->lPitch:0),
      (void *)((SUCCEEDED(hr)&&desc)?desc->lpSurface:NULL),
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->ddpfPixelFormat.dwRGBBitCount:0),
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->ddpfPixelFormat.dwRBitMask:0),
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->ddpfPixelFormat.dwGBitMask:0),
      (unsigned long)((SUCCEEDED(hr)&&desc)?desc->ddpfPixelFormat.dwBBitMask:0));
    log_line(msg);
  }
  return hr;
}
static HRESULT WINAPI hooked_f9_surface_release_dc(void *self,HDC dc) {
  HRESULT hr=original_release_dc(self,dc);
  if(f9_trace_report()) {
    DWORD rva=0;char msg[250];
    geom_rva(__builtin_return_address(0),&rva);
    snprintf(msg,sizeof(msg),
      "F9-TRACE: ReleaseDC owner=%s surface=%p caller=0x%08lX hr=0x%08lX dc=%p",
      f9_surface_kind(self),self,(unsigned long)rva,(unsigned long)hr,(void*)dc);
    log_line(msg);
  }
  return hr;
}
static HRESULT WINAPI hooked_f9_surface_unlock(void *self,LPRECT rect) {
  HRESULT hr;
  DWORD rva=0;
  geom_rva(__builtin_return_address(0),&rva);
  if(self==(void *)borderless_primary &&
     rva==0x00103537u && rect==NULL &&
     InterlockedCompareExchange(&f9_backbuffer_fix_enabled,0,0)==1 &&
     InterlockedCompareExchange(&f9_backbuffer_lock_active,0,0)==1 &&
     f9_backbuffer_lock_thread==GetCurrentThreadId() &&
     f9_backbuffer_locked_surface!=NULL) {
    IDirectDrawSurface7 *back=f9_backbuffer_locked_surface;
    char msg[230];
    hr=original_surface_unlock(back,rect);
    snprintf(msg,sizeof(msg),
      "F9-FIX: BACKBUFFER Unlock hr=0x%08lX "
      "caller=0x%08lX; original PRIMARY remains unlocked",
      (unsigned long)hr,(unsigned long)rva);
    log_line(msg);
    f9_backbuffer_locked_surface=NULL;
    f9_backbuffer_lock_thread=0;
    InterlockedExchange(&f9_backbuffer_lock_active,0);
    IDirectDrawSurface7_Release(back);
  } else {
    hr=original_surface_unlock(self,rect);
  }
  if(f9_trace_report()) {
    DWORD rva=0;char msg[240];
    geom_rva(__builtin_return_address(0),&rva);
    snprintf(msg,sizeof(msg),
      "F9-TRACE: Unlock owner=%s surface=%p caller=0x%08lX hr=0x%08lX rect=%s",
      f9_surface_kind(self),self,(unsigned long)rva,(unsigned long)hr,
      rect?"set":"NULL");
    log_line(msg);
  }
  return hr;
}
/* The original game's SHA-verified F9 dispatch calls its BMP writer at
 * exactly TWO CALL rel32 sites (RVA 0x10A1DC and 0x10A62B). The calls
 * follow the native DIK_F9 (0x43) polling and the game owns its 500ms
 * debounce. Replace only those 5-byte CALL operands, not DirectInput.
 *
 * Actual original file SHA256: 0a11a49f...4689dea
 * Original writer entry RVA: 0x1033E0 (no parameters, __cdecl).
 * Its internal COM Lock return site is RVA 0x1034D4, but that is NOT
 * a safe entry point for skipping the BMP writer. */
/* Main-menu F9 requests are fulfilled at the NEXT completed DirectDraw
 * Flip, before the backbuffer is presented and cleared. Reading PRIMARY via
 * GDI returned purple pixels, while reading BACK immediately on F9 sometimes
 * returned black because Flip clears the menu backbuffer each frame.
 * Only menu screenshots are deferred; video and 3D stay as validated. */
static volatile LONG f9_menu_png_pending=0;
static volatile LONG f9_menu_png_requested_flip=0;

static void f9_capture_completed_menu_flip(LONG flip,BOOL rendered_menu) {
  IDirectDrawSurface7 *back;
  DDSURFACEDESC2 desc;
  HRESULT hr,unlock_hr;
  LONG requested;
  int queued;
  char line[220];
  if(InterlockedCompareExchange(&f9_menu_png_pending,0,0)!=1)return;
  requested=InterlockedCompareExchange(&f9_menu_png_requested_flip,0,0);
  if(InterlockedCompareExchange(&menu_upscale_active,0,0)!=1 ||
     flip-requested>120) {
    InterlockedExchange(&f9_menu_png_pending,0);
    log_line("F9-PNG-MENU: no completed menu frame within 120 flips; capture cancelled");
    SpiderF9Toast_PublishError(L"Menu frame unavailable for screenshot");
    return;
  }
  /* Intro splash stills use ONE D3D primitive per frame. The original
   * menu-version overlay requires three+ primitives, but PNG capture should
   * accept one as soon as there is no active 4:3 movie Blt. */
  if(!rendered_menu)return;
  back=borderless_back;
  if(!back) {
    InterlockedExchange(&f9_menu_png_pending,0);
    SpiderF9Toast_PublishError(L"Menu backbuffer unavailable");
    return;
  }
  IDirectDrawSurface7_AddRef(back);
  memset(&desc,0,sizeof(desc));
  desc.dwSize=sizeof(desc);
  hr=IDirectDrawSurface7_Lock(back,NULL,&desc,DDLOCK_READONLY|DDLOCK_WAIT,NULL);
  if(SUCCEEDED(hr)) {
    /* Queue duplicates pixels before unlocking; the worker never touches
     * the DirectDraw surface and the on-screen toast is excluded. */
    queued=SpiderF9PNG_Queue(&desc,log_line);
    unlock_hr=IDirectDrawSurface7_Unlock(back,NULL);
    snprintf(line,sizeof(line),
      "F9-PNG-MENU: completed Flip=%ld BACKBUFFER %lux%lu capture=%s Lock=OK Unlock=0x%08lX",
      (long)flip,(unsigned long)desc.dwWidth,(unsigned long)desc.dwHeight,
      queued?"queued":"skipped",(unsigned long)unlock_hr);
    log_line(line);
  } else {
    snprintf(line,sizeof(line),
      "F9-PNG-MENU: completed BACKBUFFER Lock failed hr=0x%08lX",
      (unsigned long)hr);
    log_line(line);
    SpiderF9Toast_PublishError(L"Cannot read the completed menu frame");
  }
  IDirectDrawSurface7_Release(back);
  InterlockedExchange(&f9_menu_png_pending,0);
}

/* Source-frame capture for intro FMVs only. A PRIMARY GetDC/BitBlt call
 * can report success while returning purple pixels unrelated to the movie.
 * Instead, obtain the decoded RGB pixels from the source of the latest
 * successful native 4:3 media Blt. Convert RGB555/RGB565 or XRGB8888
 * explicitly, fit the whole frame to the same 4:3 presentation rectangle,
 * and pass it to the already-tested PNG writer. Do not touch gameplay,
 * comics, menu GDI, DirectInput, PRIMARY pixels or Bink internals. */
static BOOL f9_movie_prepare_mask(DWORD mask,unsigned *shift,DWORD *maxval) {
  unsigned n=0;
  DWORD v=mask;
  if(!mask)return FALSE;
  while((v&1u)==0 && n<32) {v>>=1; ++n;}
  if(!v || (v&(v+1u))!=0)return FALSE; /* contiguous RGB mask */
  *shift=n;
  *maxval=v;
  return TRUE;
}
static BYTE f9_movie_expand(DWORD pixel,DWORD mask,unsigned shift,DWORD maximum) {
  DWORD component=(pixel&mask)>>shift;
  return (BYTE)((component*255u+maximum/2u)/maximum);
}
/*  0: no current movie (use existing menu/BD capture);
 *  1: decoded movie queued;
 * -1: movie readback attempted but failed; never emit a false success. */
static int f9_capture_decoded_movie_frame(void) {
  IDirectDrawSurface7 *source=f9_movie_source;
  DDSURFACEDESC2 sd,frame;
  DWORD current_flip=(DWORD)InterlockedCompareExchange(&flip_count,0,0);
  DWORD movie_flip=(DWORD)InterlockedCompareExchange(&f9_movie_last_flip,0,0);
  DWORD rm,gm,bm,sw,sh,dw=desktop_w,dh=desktop_h;
  unsigned rs,gs,bs,bpp,x,y;
  DWORD rmax,gmax,bmax;
  BYTE *pixels=NULL;
  DWORD *output=NULL;
  LONG dx=menu_map.left,dy=menu_map.top;
  LONG vw=menu_map.right-menu_map.left,vh=menu_map.bottom-menu_map.top;
  HRESULT hr,unlock_hr=DD_OK;
  int queued=0,locked=0;
  char msg[270];
  if(!source || (current_flip-movie_flip)>8u)return 0;
  if(dw>8192u || dh>8192u || dw<640u || dh<480u ||
     vw<=0 || vh<=0 || dx<0 || dy<0 ||
     dx+vw>(LONG)dw || dy+vh>(LONG)dh) {
    log_line("F9-PNG-MOVIE: invalid displayed movie dimensions");
    SpiderF9Toast_PublishError(L"Invalid movie screenshot geometry");
    return -1;
  }
  memset(&sd,0,sizeof(sd));
  sd.dwSize=sizeof(sd);
  hr=IDirectDrawSurface7_Lock(source,NULL,&sd,
                              DDLOCK_READONLY|DDLOCK_WAIT,NULL);
  if(FAILED(hr)) {
    snprintf(msg,sizeof(msg),"F9-PNG-MOVIE: decoder source Lock failed hr=0x%08lX",
             (unsigned long)hr);
    log_line(msg);
    SpiderF9Toast_PublishError(L"Cannot read the decoded movie frame");
    return -1;
  }
  locked=1;
  sw=sd.dwWidth;sh=sd.dwHeight;
  bpp=sd.ddpfPixelFormat.dwRGBBitCount;
  rm=sd.ddpfPixelFormat.dwRBitMask;
  gm=sd.ddpfPixelFormat.dwGBitMask;
  bm=sd.ddpfPixelFormat.dwBBitMask;
  snprintf(msg,sizeof(msg),
    "F9-PNG-MOVIE: decoded source=%lux%lu bpp=%u pitch=%ld RGB=%08lX/%08lX/%08lX",
    (unsigned long)sw,(unsigned long)sh,bpp,(long)sd.lPitch,
    (unsigned long)rm,(unsigned long)gm,(unsigned long)bm);
  log_line(msg);
  if(!sd.lpSurface || !sw || !sh || sw>4096u || sh>4096u ||
     (bpp!=16 && bpp!=32) ||
     !(sd.ddpfPixelFormat.dwFlags&DDPF_RGB) ||
     sd.lPitch<(LONG)(sw*(bpp/8u)) ||
     (rm&gm) || (rm&bm) || (gm&bm) ||
     !f9_movie_prepare_mask(rm,&rs,&rmax) ||
     !f9_movie_prepare_mask(gm,&gs,&gmax) ||
     !f9_movie_prepare_mask(bm,&bs,&bmax)) {
    log_line("F9-PNG-MOVIE: unsupported decoded movie RGB pixel format");
    SpiderF9Toast_PublishError(L"Unsupported decoded movie pixel format");
    goto done;
  }
  pixels=(BYTE*)calloc((size_t)dw*(size_t)dh,4u);
  if(!pixels) {
    SpiderF9Toast_PublishError(L"Cannot allocate movie screenshot buffer");
    goto done;
  }
  output=(DWORD*)pixels;
  for(y=0;y<(unsigned)vh;y++) {
    unsigned sy=(unsigned)(((unsigned long long)y*(unsigned long long)sh)/
                            (unsigned long long)(unsigned)vh);
    const BYTE *src=(const BYTE*)sd.lpSurface+(size_t)sy*(size_t)sd.lPitch;
    DWORD *dst=output+(size_t)(dy+(LONG)y)*(size_t)dw+(size_t)dx;
    for(x=0;x<(unsigned)vw;x++) {
      unsigned sx=(unsigned)(((unsigned long long)x*(unsigned long long)sw)/
                             (unsigned long long)(unsigned)vw);
      DWORD pixel;
      if(bpp==16) {
        WORD rgb16;
        memcpy(&rgb16,src+(size_t)sx*2u,sizeof(rgb16));
        pixel=(DWORD)rgb16;
      } else memcpy(&pixel,src+(size_t)sx*4u,sizeof(pixel));
      dst[x]=((DWORD)f9_movie_expand(pixel,rm,rs,rmax)<<16) |
             ((DWORD)f9_movie_expand(pixel,gm,gs,gmax)<<8) |
              (DWORD)f9_movie_expand(pixel,bm,bs,bmax);
    }
  }
done:
  if(locked) {
    unlock_hr=IDirectDrawSurface7_Unlock(source,NULL);
    if(FAILED(unlock_hr)) {
      snprintf(msg,sizeof(msg),"F9-PNG-MOVIE: source Unlock failed hr=0x%08lX",
               (unsigned long)unlock_hr);
      log_line(msg);
      SpiderF9Toast_PublishError(L"Cannot release decoded movie pixels");
    }
  }
  if(pixels && SUCCEEDED(unlock_hr)) {
    memset(&frame,0,sizeof(frame));
    frame.dwSize=sizeof(frame);
    frame.dwFlags=DDSD_WIDTH|DDSD_HEIGHT|DDSD_PITCH|DDSD_PIXELFORMAT;
    frame.dwWidth=dw;frame.dwHeight=dh;
    frame.lPitch=(LONG)(dw*4u);
    frame.lpSurface=pixels;
    frame.ddpfPixelFormat.dwSize=sizeof(frame.ddpfPixelFormat);
    frame.ddpfPixelFormat.dwFlags=DDPF_RGB;
    frame.ddpfPixelFormat.dwRGBBitCount=32;
    frame.ddpfPixelFormat.dwRBitMask=0x00ff0000u;
    frame.ddpfPixelFormat.dwGBitMask=0x0000ff00u;
    frame.ddpfPixelFormat.dwBBitMask=0x000000ffu;
    queued=SpiderF9PNG_Queue(&frame,log_line);
    snprintf(msg,sizeof(msg),
      "F9-PNG-MOVIE: decoded RGB screenshot %lux%lu queued=%d [no PRIMARY-DC]",
      (unsigned long)dw,(unsigned long)dh,queued);
    log_line(msg);
  }
  if(pixels)free(pixels);
  return queued?1:-1;
}

static void __cdecl hooked_f9_png_only(void) {
  IDirectDrawSurface7 *back;
  DDSURFACEDESC2 desc;
  HRESULT hr,unlock_hr;
  char msg[210];
  int queued=0;
  if(InterlockedCompareExchange(&f9_png_only_enabled,0,0)!=1) return;
  if(InterlockedCompareExchange(&f9_png_capture_active,1,0)!=0) {
    log_line("F9-PNG-ONLY: duplicate F9 capture while backbuffer busy; skipped");
    SpiderF9Toast_PublishError(L"Screenshot capture is busy");
    return;
  }
  /* During intro FMVs/menus, the 3D backbuffer is often entirely black
   * even when DirectDraw visibly presents a complete picture. */
  if(InterlockedCompareExchange(&menu_upscale_active,0,0)==1) {
    int movie=f9_capture_decoded_movie_frame();
    if(movie==0) {
      /* Do not read PRIMARY-DC and do not read BACK mid-frame: it is
       * cleared after every menu Flip. Capture its NEXT complete frame. */
      if(InterlockedCompareExchange(&f9_menu_png_pending,1,0)==0) {
        InterlockedExchange(&f9_menu_png_requested_flip,
                            InterlockedCompareExchange(&flip_count,0,0));
        log_line("F9-PNG-MENU: deferred until next completed menu Flip");
      } else {
        log_line("F9-PNG-MENU: prior menu screenshot still pending");
        SpiderF9Toast_PublishError(L"Menu screenshot is still pending");
      }
    }
    /* If a decoded movie was detected but unreadable, do not fall back
     * to any PRIMARY GDI readback: native Windows returned purple pixels. */
    goto done;
  }
  /* Comic screenshots must not bake the toast into a frozen DirectDraw frame. */
  {
    LONG flip=InterlockedCompareExchange(&flip_count,0,0);
    LONG last=InterlockedCompareExchange(&f9_comic_last_draw_flip,0,0);
    f9_static_comic_capture=(last>=0 && flip-last<=2);
    if(f9_static_comic_capture)
      log_line("F9-TOAST-COMIC: static comic recognized; popup-only notification");
  }
  back=borderless_back;
  if(!back || desktop_w<640 || desktop_h<480) {
    log_line("F9-PNG-ONLY: no valid 3D backbuffer; PNG capture skipped");
    SpiderF9Toast_PublishError(L"3D backbuffer unavailable");
    goto done;
  }
  IDirectDrawSurface7_AddRef(back);
  memset(&desc,0,sizeof(desc));
  desc.dwSize=sizeof(desc);
  hr=IDirectDrawSurface7_Lock(back,NULL,&desc,
                              DDLOCK_READONLY|DDLOCK_WAIT,NULL);
  if(SUCCEEDED(hr)) {
    /* SpiderF9PNG_Queue copies pixels synchronously. Its worker receives
     * only private memory, so the DirectDraw surface is unlocked before
     * any PNG compression and before continuing native gameplay. */
    queued=f9_static_comic_capture?
           SpiderF9PNG_QueueComic(&desc,log_line):
           SpiderF9PNG_Queue(&desc,log_line);
    unlock_hr=IDirectDrawSurface7_Unlock(back,NULL);
    snprintf(msg,sizeof(msg),
      "F9-PNG-ONLY: backbuffer capture=%s %lux%lu Lock=OK Unlock=0x%08lX",
      queued?"queued":"skipped",
      (unsigned long)desc.dwWidth,(unsigned long)desc.dwHeight,
      (unsigned long)unlock_hr);
    log_line(msg);
  } else {
    snprintf(msg,sizeof(msg),
      "F9-PNG-ONLY: backbuffer Lock failed hr=0x%08lX; no BMP created",
      (unsigned long)hr);
    log_line(msg);
    SpiderF9Toast_PublishError(L"Cannot read the 3D backbuffer");
  }
  IDirectDrawSurface7_Release(back);
done:
  InterlockedExchange(&f9_png_capture_active,0);
}

/* Install the default PNG game call replacement only at exact verified CALL
 * sites; preflight BOTH original signatures before writing either. The
 * game is single-threaded at startup, well before F9 can be triggered.
 * A 32-bit x86 CALL rel32 wraps modulo 2^32, so it reaches any DLL
 * address within this process without an executable trampoline. */
static void install_f9_png_only_dispatch(void) {
  static const struct {
    DWORD rva;
    BYTE original[5];
  } calls[2]={
    {0x0010A1DCu,{0xE8,0xFF,0x91,0xFF,0xFF}},
    {0x0010A62Bu,{0xE8,0xB0,0x8D,0xFF,0xFF}}
  };
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  BYTE *p[2];
  DWORD previous[2]={0,0},ignored;
  unsigned i;
  char msg[192];
  if(!base || (uintptr_t)base!=0x00400000u) {
    log_line("F9-PNG: unsupported EXE layout; original game calls remain unchanged");
    return;
  }
  for(i=0;i<2;i++) {
    p[i]=base+calls[i].rva;
    if(memcmp(p[i],calls[i].original,sizeof(calls[i].original))!=0) {
      snprintf(msg,sizeof(msg),
        "F9-PNG-ONLY: signature mismatch at RVA 0x%08lX; leaving original game intact",
        (unsigned long)calls[i].rva);
      log_line(msg);
      return;
    }
  }
  if(!VirtualProtect(p[0],5,PAGE_EXECUTE_READWRITE,&previous[0])) {
    log_line("F9-PNG-ONLY: cannot make first CALL writable; original unchanged");
    return;
  }
  if(!VirtualProtect(p[1],5,PAGE_EXECUTE_READWRITE,&previous[1])) {
    VirtualProtect(p[0],5,previous[0],&ignored);
    log_line("F9-PNG-ONLY: cannot make second CALL writable; original unchanged");
    return;
  }
  for(i=0;i<2;i++) {
    uintptr_t target=(uintptr_t)&hooked_f9_png_only;
    DWORD relative=(DWORD)(target-((uintptr_t)p[i]+5u));
    p[i][0]=0xE8; /* CALL rel32; the original game handles the F9 key. */
    memcpy(p[i]+1,&relative,sizeof(relative));
  }
  FlushInstructionCache(GetCurrentProcess(),p[0],5);
  FlushInstructionCache(GetCurrentProcess(),p[1],5);
  if(!VirtualProtect(p[1],5,previous[1],&ignored))
    log_line("F9-PNG-ONLY: WARNING failed to restore second code-page protection");
  if(!VirtualProtect(p[0],5,previous[0],&ignored))
    log_line("F9-PNG-ONLY: WARNING failed to restore first code-page protection");
  /* Flag active only after both native callsites have been rewritten.
   * No user setting can change the screenshot format. */
  InterlockedExchange(&f9_native_png_enabled,1);
  InterlockedExchange(&f9_png_only_enabled,1);
  log_line("F9-PNG: always ON; both SHA-verified F9 calls redirected to PNG; native BMP bypassed");
}
static void instrument_surface(IDirectDrawSurface7 *surface);
static HRESULT WINAPI hooked_get_attached(void *self,LPDDSCAPS2 caps,
                                          IDirectDrawSurface7 **out) {
  HRESULT hr=borderless_get_back(self,caps,out);
  LONG n=InterlockedIncrement(&attached_count);
  if(debug_log_enabled && n<=40) {
    char line[240];
    snprintf(line,sizeof(line),
      "DDRAW-PRESENT: GetAttachedSurface #%ld self=%p caps=0x%08lX hr=0x%08lX surface=%p",
      (long)n,self,(unsigned long)(caps?caps->dwCaps:0),
      (unsigned long)hr,(void*)(out?*out:NULL));
    log_line(line);
  }
  if(SUCCEEDED(hr) && out && *out) instrument_surface(*out);
  return hr;
}
/* Cooperative DirectDraw Flip focus pause (opt-in).
 * Bink and game-owned DirectSound voices can be paused separately by their
 * native APIs; the Flip pause itself does not suspend every game thread.
 * Keep pumping messages and never force application activation. */
static volatile LONG pause_on_focus_loss=0;
static LONG pause_ever_foreground=0;
static LONG pause_in_progress=0;
static LONG pause_latched=0;
static DWORD pause_background_since=0;
/* Bounded READ-ONLY diagnostics for Alt+Tab losses which do not
 * produce the native PAUSED ribbon. No extra WndProc/DI hooks. */
static unsigned pause_focus_probe_reports=0;
static unsigned pause_flip_gap_reports=0;
static DWORD pause_previous_flip_tick=0;
static unsigned pause_native_ui_focus_reports=0;
static DWORD pause_native_ui_focus_sample_tick=0;
static void pause_probe_report(const char *event, HWND game, HWND fg,
                               DWORD other_pid, DWORD elapsed) {
  char line[230];
  if(!debug_log_enabled || pause_focus_probe_reports>=24u)return;
  ++pause_focus_probe_reports;
  snprintf(line,sizeof(line),
    "FOCUS-PROBE: %s game=%p foreground=%p other-pid=%lu elapsed-ms=%lu "
    "(Flip-sampled only; no focus mutation)",
    event,(void*)game,(void*)fg,(unsigned long)other_pid,
    (unsigned long)elapsed);
  log_line(line);
}

/* Diagnostic only: report WHICH other executable owns the foreground when
 * Windows reports sustained focus loss. No activation/focus operations.
 * Log the program filename (not its path or potentially private title). */
static void pause_log_foreground_owner(HWND game,HWND foreground,const char *why) {
  DWORD game_pid=0,fg_pid=0,game_tid=0,fg_tid=0;
  char fg_class[96]="?",fg_program[128]="?",line[460];
  HANDLE process=NULL;
  DWORD len;
  char full[MAX_PATH],*basename;
  if(!debug_log_enabled)return;
  if(game && IsWindow(game))game_tid=GetWindowThreadProcessId(game,&game_pid);
  if(foreground && IsWindow(foreground)) {
    fg_tid=GetWindowThreadProcessId(foreground,&fg_pid);
    GetClassNameA(foreground,fg_class,sizeof(fg_class));
  }
  if(fg_pid) {
    process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,fg_pid);
    if(process) {
      len=sizeof(full);
      if(QueryFullProcessImageNameA(process,0,full,&len)) {
        basename=strrchr(full,'\\');
        basename=basename?basename+1:full;
        snprintf(fg_program,sizeof(fg_program),"%s",basename);
      }
      CloseHandle(process);
    }
  }
  snprintf(line,sizeof(line),
    "FOCUS-PAUSE-OWNER: %s game=%p pid=%lu tid=%lu "
    "foreground=%p pid=%lu tid=%lu class=%s exe=%s "
    "visible=%d iconic=%d paused-after-ms=%lu",
    why,(void*)game,(unsigned long)game_pid,(unsigned long)game_tid,
    (void*)foreground,(unsigned long)fg_pid,(unsigned long)fg_tid,
    fg_class,fg_program,(int)(foreground?IsWindowVisible(foreground):0),
    (int)(foreground?IsIconic(foreground):0),
    (unsigned long)(pause_background_since?
     (DWORD)(GetTickCount()-pause_background_since):0));
  log_line(line);
}
/* While the original game's native Pause UI is open, sample the
 * foreground independently of the 150-ms debounce decision. Under Wine,
 * visible focus change is not always accompanied by a foreground HWND
 * transition at a Flip. This probe is read-only and does not subclass
 * the game WndProc, perform activation or intercept DirectInput. */
static void pause_probe_native_menu_focus(LONG flip) {
  HWND game=game_hwnd,fg;
  DWORD now,fg_pid=0,game_tid;
  GUITHREADINFO fg_info,game_info;
  BOOL fg_ok,game_ok;
  char line[330];
  if(!debug_log_enabled || !game || !IsWindow(game) ||
     pause_native_ui_focus_reports>=120u ||
     ((*(const volatile DWORD *)(uintptr_t)0x00682950u)==0u &&
      !native_pause43_visible_near_flip(flip)))return;
  now=GetTickCount();
  if(pause_native_ui_focus_sample_tick &&
     (DWORD)(now-pause_native_ui_focus_sample_tick)<800u)return;
  pause_native_ui_focus_sample_tick=now;
  ++pause_native_ui_focus_reports;
  fg=GetForegroundWindow();
  if(fg)GetWindowThreadProcessId(fg,&fg_pid);
  game_tid=GetWindowThreadProcessId(game,NULL);
  memset(&fg_info,0,sizeof(fg_info));fg_info.cbSize=sizeof(fg_info);
  memset(&game_info,0,sizeof(game_info));game_info.cbSize=sizeof(game_info);
  fg_ok=GetGUIThreadInfo(0,&fg_info);
  game_ok=game_tid?GetGUIThreadInfo(game_tid,&game_info):FALSE;
  snprintf(line,sizeof(line),
    "FOCUS-PROBE: native-pause-snapshot flip=%ld fg-is-game=%d "
    "fg=%p pid=%lu fg-ui-ok=%d fg-ui-active=%p fg-ui-focus=%p "
    "game-ui-ok=%d game-ui-active=%p game-ui-focus=%p",
    (long)flip,(int)(fg==game),(void*)fg,(unsigned long)fg_pid,
    (int)fg_ok,(void*)fg_info.hwndActive,(void*)fg_info.hwndFocus,
    (int)game_ok,(void*)game_info.hwndActive,
    (void*)game_info.hwndFocus);
  log_line(line);
}
/* Under Wine/X11, the foreground HWND can stay NULL after a real
 * Alt+Tab to a native Linux window. Do NOT treat NULL alone as focus loss:
 * require the game GUI thread to have neither an active nor a focused HWND,
 * then use the same 150-ms debounce as the normal foreign-process path.
 * An unavailable GUI-thread snapshot fails closed (no automatic pause). */
static BOOL pause_confirmed_null_foreground_loss(HWND hwnd) {
  GUITHREADINFO gi;
  DWORD tid=GetWindowThreadProcessId(hwnd,NULL);
  if(!tid)return FALSE;
  memset(&gi,0,sizeof(gi));
  gi.cbSize=sizeof(gi);
  if(!GetGUIThreadInfo(tid,&gi))return FALSE;
  return gi.hwndActive==NULL && gi.hwndFocus==NULL;
}
/* Ignore brief foreground changes (e.g. mouse activation quirks).
 * A foreign-process foreground or a *verified sustained* NULL foreground
 * must survive the same 150-ms debounce before pausing the game. */
static BOOL pause_confirmed_focus_loss(HWND hwnd) {
  HWND fg=GetForegroundWindow();
  DWORD game_pid=0,fg_pid=0,now;
  if(fg==hwnd) {
    if(pause_background_since)
      pause_probe_report("candidate-cancelled-return-before-150ms",
        hwnd,fg,0,GetTickCount()-pause_background_since);
    pause_background_since=0;
    return FALSE;
  }
  GetWindowThreadProcessId(hwnd,&game_pid);
  if(fg) GetWindowThreadProcessId(fg,&fg_pid);
  /* Reject same-process windows and vanished/unreadable foreign HWNDs.
   * A NULL foreground is acceptable only with an independently verified
   * inactive game GUI thread, and only after the ordinary debounce. */
  if(!game_pid || (fg && (!fg_pid || fg_pid==game_pid)) ||
     (!fg && !pause_confirmed_null_foreground_loss(hwnd))) {
    if(pause_background_since)
      pause_probe_report("candidate-cancelled-null-or-own-process",
        hwnd,fg,fg_pid,GetTickCount()-pause_background_since);
    pause_background_since=0;
    return FALSE;
  }
  now=GetTickCount();
  if(!pause_background_since) {
    pause_background_since=now?now:1;
    pause_probe_report(fg?"external-candidate-first-flip":
      "null-foreground-candidate-first-flip",hwnd,fg,fg_pid,0);
    return FALSE;
  }
  if((DWORD)(now-pause_background_since)>=150) {
    pause_probe_report(fg?"external-candidate-confirmed-after-150ms":
      "null-foreground-confirmed-after-150ms",
      hwnd,fg,fg_pid,(DWORD)(now-pause_background_since));
    return TRUE;
  }
  return FALSE;
}


/* Wake only with the foreground Any-key hook. */
/* Focus-only snapshot: no DirectInput hooks or synthetic activation. */
static void pause_log_resume_focus(HWND hwnd) {
  GUITHREADINFO gi; DWORD tid;char line[280];
  if(!debug_log_enabled || !hwnd || !IsWindow(hwnd)) return;
  tid=GetWindowThreadProcessId(hwnd,NULL);
  memset(&gi,0,sizeof(gi));gi.cbSize=sizeof(gi);
  if(GetGUIThreadInfo(tid,&gi)) {
    snprintf(line,sizeof(line),
      "FOCUS-PAUSE: resume-focus hwnd=%p foreground=%p active=%p focus=%p capture=%p flags=0x%08lX thread=%lu",
      (void*)hwnd,(void*)GetForegroundWindow(),(void*)gi.hwndActive,
      (void*)gi.hwndFocus,(void*)gi.hwndCapture,(unsigned long)gi.flags,(unsigned long)tid);
  } else {
    snprintf(line,sizeof(line),"FOCUS-PAUSE: resume-focus GetGUIThreadInfo failed Win32=%lu",
      (unsigned long)GetLastError());
  }
  log_line(line);
}
/* Wine/X11 opt-in host focus bridge. The Linux observer writes a tiny,
 * frequently updated state file next to SpideyPC.exe. Win32 cannot be
 * trusted to notice a native Firefox focus switch on some Wine versions:
 * GetForegroundWindow() can remain the game HWND for many seconds even
 * though XGetInputFocus/_NET_ACTIVE_WINDOW name another Linux process.
 *
 * This bridge never activates or resizes a window. It fails closed if the
 * host observer stops, the file is stale (>1200ms), or the state is UNKNOWN.
 * File reading occurs only on the original game Flip/pause thread. */
static int pause_x11_bridge_config=-1; /* -1 uninitialized; 0 off; 1 on */
static char pause_x11_bridge_path[MAX_PATH];
static DWORD pause_x11_bridge_poll_tick=0;
static int pause_x11_bridge_last_state=0; /* 0 unknown, 1 game, 2 other */
static unsigned pause_x11_bridge_reports=0;
static DWORD pause_x11_bridge_loss_since=0;
static int pause_x11_bridge_enabled(void) {
  /* The auto-started ELF publishes a fresh X11 handshake asynchronously:
   * on the first Flip this may be disabled, and on the next it can turn
   * on. Recheck ONLY while disabled to allow this safe 0 -> 1 transition.
   * Existing manual =1 / explicit =0 behavior is preserved. */
  if(pause_x11_bridge_config<=0) {
    char value[4]={0};
    DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE",
                                     value,sizeof(value));
    if(pause_x11_bridge_config==0 && !(n==1 && value[0]=='1'))
      return 0;
    pause_x11_bridge_config=n==1 && value[0]=='1';
    if(pause_x11_bridge_config) {
      char *sep;
      n=GetModuleFileNameA(NULL,pause_x11_bridge_path,
                             sizeof(pause_x11_bridge_path));
      sep=n && n<sizeof(pause_x11_bridge_path)?
        strrchr(pause_x11_bridge_path,'\\'):NULL;
      if(!sep || (size_t)(sep-pause_x11_bridge_path)+
         sizeof("SpiderMan2k1Fix.x11-focus")>=sizeof(pause_x11_bridge_path)) {
        pause_x11_bridge_config=0;
        log_line("FOCUS-X11: opt-in disabled: cannot resolve game folder");
      } else {
        strcpy(sep+1,"SpiderMan2k1Fix.x11-focus");
        log_line("FOCUS-X11: opt-in Linux X11 bridge enabled; requires fresh host observer state and 150ms debounce");
      }
    }
  }
  return pause_x11_bridge_config;
}
static int pause_x11_bridge_state(void) {
  DWORD tick=GetTickCount();
  int state=0;
  WIN32_FILE_ATTRIBUTE_DATA attr;
  FILETIME ft;
  ULARGE_INTEGER modified,current;
  HANDLE h;
  char line[64]={0};
  DWORD read=0;
  if(!pause_x11_bridge_enabled())return 0;
  if(pause_x11_bridge_poll_tick &&
     (DWORD)(tick-pause_x11_bridge_poll_tick)<80u)
    return pause_x11_bridge_last_state;
  pause_x11_bridge_poll_tick=tick;
  if(GetFileAttributesExA(pause_x11_bridge_path,
                          GetFileExInfoStandard,&attr)) {
    GetSystemTimeAsFileTime(&ft);
    modified.LowPart=attr.ftLastWriteTime.dwLowDateTime;
    modified.HighPart=attr.ftLastWriteTime.dwHighDateTime;
    current.LowPart=ft.dwLowDateTime;
    current.HighPart=ft.dwHighDateTime;
    if(current.QuadPart>=modified.QuadPart &&
       current.QuadPart-modified.QuadPart<=1200ull*10000ull) {
      h=CreateFileA(pause_x11_bridge_path,GENERIC_READ,
                    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                    NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
      if(h!=INVALID_HANDLE_VALUE) {
        if(ReadFile(h,line,sizeof(line)-1,&read,NULL) && read) {
          line[read]=0;
          if(!strcmp(line,"SPIDEY_X11_FOCUS_V1 GAME\n"))state=1;
          else if(!strcmp(line,"SPIDEY_X11_FOCUS_V1 OTHER\n"))state=2;
        }
        CloseHandle(h);
      }
    }
  }
  if(state!=pause_x11_bridge_last_state &&
     pause_x11_bridge_reports<32u) {
    char report[150];
    ++pause_x11_bridge_reports;
    snprintf(report,sizeof(report),
      "FOCUS-X11: host-focus-state=%s win32-game-foreground=%d",
      state==1?"GAME":state==2?"OTHER":"UNKNOWN",
      game_hwnd && GetForegroundWindow()==game_hwnd);
    log_line(report);
  }
  pause_x11_bridge_last_state=state;
  return state;
}
static BOOL pause_x11_bridge_confirmed_loss(void) {
  DWORD tick=GetTickCount();
  if(!pause_x11_bridge_loss_since) {
    pause_x11_bridge_loss_since=tick?tick:1u;
    log_line("FOCUS-X11: external native-X11 focus candidate; waiting 150ms");
    return FALSE;
  }
  if((DWORD)(tick-pause_x11_bridge_loss_since)>=150u) {
    log_line("FOCUS-X11: confirmed native-X11 focus loss after 150ms (Win32 foreground stale)");
    pause_x11_bridge_loss_since=0;
    return TRUE;
  }
  return FALSE;
}
static void pause_at_flip_if_background(void) {
  HWND hwnd=game_hwnd;
  MSG msg;
  RECT pause_present_rect,pause_content_rect;
  int pause_gamepad_available=0;
  int pause_sdl3_available=0;
  int pause_previous_foreground=0;
  /* Terminal observer failure: allow a manual keyboard/gamepad wake only
   * after we actually observed Win32 switch AWAY from the game and
   * switch BACK after the two native restart attempts are exhausted.
   * A permanently stale Wine GetForegroundWindow()==game without an
   * observed external phase is NOT sufficient evidence. */
  int fallback_saw_external=0;
  int fallback_wake_armed=0;
  DWORD fallback_return_since=0;
  int x11_state,x11_external,win32_game_foreground;
  if(InterlockedCompareExchange(&pause_on_focus_loss,0,0)!=1 || !hwnd || !IsWindow(hwnd)) return;
  x11_state=pause_x11_bridge_state();
  x11_external=pause_x11_bridge_enabled() && x11_state==2;
  win32_game_foreground=GetForegroundWindow()==hwnd;
  /* Native host focus is also authoritative for the selected SDL3
   * universal keyboard adapter. Previously PAUSED correctly spotted
   * Firefox via X11, but SDL3 still trusted the stale Wine HWND and
   * left synthetic directions held. Release them before the blocking
   * PAUSED loop, and block any new input while the host says OTHER
   * or UNKNOWN. The initial no-helper Win32 fallback is unchanged. */
  SpiderSDL3Universal_SetHostFocusBlocked(
    pause_x11_bridge_enabled() && x11_state!=1);
  if(!x11_external)pause_x11_bridge_loss_since=0;
  if(win32_game_foreground && !x11_external) {
    /* No deferred/return-only pause: native GAME focus returns to its
     * normal Flip thread without surprising extra PAUSED panels. */
    /* This path bypasses pause_confirmed_focus_loss(), so report
     * a short external focus candidate cancelled on the next Flip. */
    if(pause_background_since)
      pause_probe_report("candidate-cancelled-return-to-game",hwnd,
        GetForegroundWindow(),0,GetTickCount()-pause_background_since);
    pause_background_since=0;
    InterlockedExchange(&pause_ever_foreground,1);
  } else if(InterlockedCompareExchange(&pause_ever_foreground,0,0)==1 &&
            ((win32_game_foreground && x11_external &&
              pause_x11_bridge_confirmed_loss()) ||
             (!win32_game_foreground && pause_confirmed_focus_loss(hwnd))) &&
            InterlockedCompareExchange(&pause_latched,1,0)==0) {
    /* Native X11 can prove Firefox has focus while Wine still reports
     * Spider-Man as foreground. Before entering blocking PAUSED, force
     * SDL3 universal's held direction release and native DI restoration.
     * The normal BeforeUI(HWND) Win32 check would otherwise miss this
     * loss and preserve a stale original-game buffered movement latch. */
    SpiderSDL3Universal_OnConfirmedFocusPause();
    SpiderAltLatch_OnConfirmedExternalLoss();
    log_line("FOCUS-PAUSE: confirmed external focus loss; pause latched until manual resume");
    pause_log_foreground_owner(hwnd,GetForegroundWindow(),"external-foreground-latched");
    /* The candidate is now *consumed*. Keeping its old start timestamp
     * across the blocking custom PAUSED loop produced a false
     * 'candidate-cancelled-return-to-game elapsed-ms=5303' trace after
     * a successful wake. Clear it only after recording elapsed time. */
    pause_background_since=0;
  }
  if(InterlockedCompareExchange(&pause_latched,0,0)!=1) return;
  if(InterlockedCompareExchange(&pause_in_progress,1,0)!=0) return;
  /* Wine may still claim this HWND is foreground while native Firefox
   * actually has X11 focus. Never swallow Firefox keystrokes. */
  SpiderPauseAny_SetHostFocusBlocked(pause_x11_bridge_enabled() &&
                                    x11_state!=1);
  /* Arm Any-key wake BEFORE stopping the game. Never fall back to F9 or
   * Space: if the low-level hook fails, skip this pause rather than hang. */
  if(!SpiderPauseAny_Begin(hwnd)) {
    char line[160];DWORD err=GetLastError();
    snprintf(line,sizeof(line),
      "FOCUS-PAUSE: Any-key hook failed Win32=%lu; skip focus pause (no fallback)",
      (unsigned long)err);
    log_line(line);
    InterlockedExchange(&pause_latched,0);
    InterlockedExchange(&pause_in_progress,0);
    return;
  }
  log_line("FOCUS-PAUSE: Any keyboard resume ARMED; consumes press and release");
  /* While Flip is blocked, the regular SDL3 OnFlip and UI poll cannot run.
   * Prefer the selected SDL3 handle (same game thread, validated native
   * owner), not the secondary DirectInput handle that can become INPUTLOST
   * on Alt+Tab. Retain the old DI8 wake for disabled/unavailable SDL3.
   * Both require neutral->fresh digital press->release in foreground. */
  pause_sdl3_available=SpiderSDL3Probe_PauseWake_Begin();
  pause_gamepad_available=pause_sdl3_available?1:SpiderGamepadUI_PauseWake_Begin();
  if(pause_sdl3_available)
    log_line("FOCUS-PAUSE: SDL3 selected-gamepad wake ARMED; foreground-only, release-gated, no injected key (keyboard always available)");
  else if(pause_gamepad_available)
    log_line("FOCUS-PAUSE: DS4 DirectInput digital-button fallback ARMED; foreground-only, release-gated, no injected key (keyboard always available)");
  else
    log_line("FOCUS-PAUSE: no verified gamepad wake device; PAUSED hint remains keyboard-only");
  /* The Flip pause alone does not suspend Bink or secondary DirectSound. */
  SpiderFocusMedia_Pause();
  SpiderFocusDSound_Pause();
  /* The frame currently rendered in borderless_back is the image which
   * would have been presented at this Flip boundary. Compose the PAUSE UI
   * into that very game surface and Blt it exactly once to PRIMARY before
   * waiting; the desktop window manager then sees an ordinary game frame,
   * not any of our old floating overlay HWNDs. */
  pause_present_rect.left=borderless_monitor_x;
  pause_present_rect.top=borderless_monitor_y;
  pause_present_rect.right=borderless_monitor_x+(LONG)desktop_w;
  pause_present_rect.bottom=borderless_monitor_y+(LONG)desktop_h;
  /* Keep the fine grid within the actual game image: the menu and intro
   * use a centered 4:3 viewport, while Auto gameplay can be widescreen.
   * The black pillarboxes should stay clean, not display a desktop grid. */
  pause_content_rect.left=0;
  pause_content_rect.top=0;
  pause_content_rect.right=(LONG)desktop_w;
  pause_content_rect.bottom=(LONG)desktop_h;
  if(InterlockedCompareExchange(&menu_upscale_active,0,0)==1 &&
     menu_map.right>menu_map.left && menu_map.bottom>menu_map.top) {
    pause_content_rect.left=menu_map.left;
    pause_content_rect.top=menu_map.top;
    pause_content_rect.right=menu_map.right;
    pause_content_rect.bottom=menu_map.bottom;
  } else if(InterlockedCompareExchange(&gameplay_4x3_active,0,0)==1 &&
            game_map.right>game_map.left && game_map.bottom>game_map.top) {
    pause_content_rect.left=game_map.left;
    pause_content_rect.top=game_map.top;
    pause_content_rect.right=game_map.right;
    pause_content_rect.bottom=game_map.bottom;
  }
  if(borderless_back && borderless_primary && original_blt)
    SpiderPauseSurface_Begin(borderless_back,borderless_primary,
                             &pause_present_rect,&pause_content_rect,
                             (SpiderPauseBltFn)original_blt,
                             pause_gamepad_available);
  pause_previous_foreground=(GetForegroundWindow()==hwnd);
  while(IsWindow(hwnd) && InterlockedCompareExchange(&pause_latched,0,0)==1) {
    while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) {
      if(msg.message==WM_QUIT) {
        PostQuitMessage((int)msg.wParam);
        SpiderPauseAny_End();
        SpiderGamepadUI_PauseWake_End();
        SpiderSDL3Probe_PauseWake_End();
        SpiderPauseSurface_End();
        SpiderFocusDSound_Resume();
        SpiderFocusMedia_Resume();
        log_line("FOCUS-PAUSE: WM_QUIT observed, aborting wait");
        InterlockedExchange(&pause_in_progress,0);
        return;
      }
      if(SpiderPauseAny_DiscardGameKeyMessage(&msg,hwnd)) continue;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    if(!IsWindow(hwnd)) break;
    /* This also runs while regular Flip is blocked by the PAUSED loop.
     * Fail closed: UNKNOWN / stale bridge must never swallow a key from
     * an unrelated native Linux application. */
    x11_state=pause_x11_bridge_state();
    /* A killed auto-started observer must not wake Spider-Man (or its
     * Bink/DirectSound voices) behind Firefox. Restart the owned native
     * observer, verify a fresh X11 sample, then require an actual user
     * key/gamepad wake after native GAME focus returns. Never synthesize
     * a key or trust Wine's possibly stale foreground HWND. */
    SpiderWineFocusAutostart_PollReady(log_line);
    SpiderWineFocusAutostart_TryRecover(log_line);
    if(pause_x11_bridge_enabled() && x11_state==0 &&
       SpiderWineFocusAutostart_RecoveryExhausted()) {
      const int win32_confirmed_game=
        GetForegroundWindow()==hwnd && GetActiveWindow()==hwnd &&
        GetFocus()==hwnd;
      if(!win32_confirmed_game) {
        fallback_saw_external=1;
        fallback_return_since=0;
        fallback_wake_armed=0;
      } else if(fallback_saw_external && !fallback_wake_armed) {
        DWORD tick=GetTickCount();
        if(!fallback_return_since)fallback_return_since=tick?tick:1;
        else if((DWORD)(tick-fallback_return_since)>=150u) {
          fallback_wake_armed=1;
          log_line("FOCUS-X11: observer recovery exhausted; fresh Win32 external->game focus cycle confirmed; manual key/gamepad wake allowed");
        }
      }
      /* A valid explicit user key is still required. No automatic wake
       * and no stealing from Firefox while Win32 reports background. */
      SpiderPauseAny_SetHostFocusBlocked(!fallback_wake_armed);
    } else {
      /* Native X11 returns authoritative information when available.
       * A fresh UNKNOWN from a live helper must not weaken the gate. */
      fallback_saw_external=0;
      fallback_return_since=0;
      fallback_wake_armed=0;
      SpiderPauseAny_SetHostFocusBlocked(pause_x11_bridge_enabled() &&
                                        x11_state!=1);
    }
    /* The normal Flip-based focus monitor cannot run until this custom
     * PAUSED wait ends. On return to game foreground, balance its temporary
     * background ShowCursor(TRUE) NOW, before any wake input. Otherwise the
     * desktop cursor lingers over PAUSED until the game resumes.
     * Call the already-existing focus transition monitor exactly once
     * per foreground change. Do not touch the game's own mouse gate. */
    {
      int pause_now_foreground=GetForegroundWindow()==hwnd;
      if(pause_now_foreground!=pause_previous_foreground) {
        SpiderInputFocus_OnFlip(InterlockedCompareExchange(&flip_count,0,0));
        pause_previous_foreground=pause_now_foreground;
      }
    }
    if(SpiderPauseAny_Completed())
      InterlockedExchange(&pause_latched,0);
    else {
      unsigned resumed_buttons=
        (pause_x11_bridge_enabled() && x11_state!=1 &&
         !fallback_wake_armed)?0:
        (pause_sdl3_available?
          SpiderSDL3Probe_PauseWake_Poll(hwnd):
          SpiderGamepadUI_PauseWake_Poll(hwnd));
      if(resumed_buttons) {
        char line[155];
        snprintf(line,sizeof(line),
          "FOCUS-PAUSE: gamepad digital press/release consumed buttons=0x%03X (selected SDL3 or verified DI8; no native key injection)",
          resumed_buttons);
        log_line(line);
        InterlockedExchange(&pause_latched,0);
      }
    }
    if(InterlockedCompareExchange(&pause_latched,0,0)!=1) break;
    /* No show/hide transitions: a frozen DirectDraw frame naturally
     * stays behind unrelated Xfce/Windows applications during Alt+Tab. */
    MsgWaitForMultipleObjectsEx(0,NULL,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
  }
  if(SpiderPauseAny_FirstKey()!=0) {
    char line[128];
    snprintf(line,sizeof(line),
      "FOCUS-PAUSE: Any-key resume swallowed virtual-key=0x%02lX",
      (unsigned long)SpiderPauseAny_FirstKey());
    log_line(line);
  }
  SpiderPauseAny_End();
  SpiderGamepadUI_PauseWake_End();
  SpiderSDL3Probe_PauseWake_End();
  SpiderPauseSurface_End();
  SpiderFocusDSound_Resume();
  SpiderFocusMedia_Resume();
  log_line("FOCUS-PAUSE: explicit resume or window destroyed");
  pause_log_foreground_owner(hwnd,GetForegroundWindow(),"resume-foreground-state");
  pause_log_resume_focus(hwnd);
  SpiderAltLatch_OnPauseReturn(hwnd);
  InterlockedExchange(&pause_in_progress,0);
}
static HRESULT WINAPI hooked_flip(void *self, IDirectDrawSurface7 *target, DWORD flags) {
  HRESULT hr; LONG count=InterlockedIncrement(&flip_count);
  focus_timeline_pulse(1,count,self);
  if(debug_log_enabled &&
     InterlockedCompareExchange(&pause_on_focus_loss,0,0)==1) {
    DWORD now=GetTickCount();
    if(pause_previous_flip_tick &&
       (DWORD)(now-pause_previous_flip_tick)>=750u &&
       pause_flip_gap_reports<12u) {
      char line[224];
      ++pause_flip_gap_reports;
      snprintf(line,sizeof(line),
        "FOCUS-PROBE: Flip-gap-ms=%lu flip=%ld foreground-now=%d "
        "latched=%ld (gap is not proof of focus loss)",
        (unsigned long)(DWORD)(now-pause_previous_flip_tick),
        (long)count,(int)(game_hwnd && GetForegroundWindow()==game_hwnd),
        (long)InterlockedCompareExchange(&pause_latched,0,0));
      log_line(line);
    }
    pause_previous_flip_tick=now;
  }
  SpiderQuickExitF12_ObserveEngineState(count,InterlockedCompareExchange(&menu_upscale_active,0,0)==1); /* passive native mode+2D renderer tracing */
  /* Game-native 0x006151F8 is GameFMV_Active in the SHA-verified
   * original executable. A post-menu Bink can play with menu_upscale=0.
   * The 3D stage must use the original menu-mode switch, NOT the optional
   * gameplay_4x3_active flag (which is false in widescreen Auto).
   * Gamepad pulse release runs before any focus-loss pause blocks Flip. */
  /* Poll SDL3 before the UI bridge to avoid one stale frame of a previous
   * controller's synthetic keys after a Start ownership switch. Actual
   * native DI handoff remains AFTER unplug guard installation below. */
  SpiderSDL3Probe_OnFlip(count,game_hwnd);
  SpiderSDL3Universal_BeforeUI(game_hwnd); /* release keys before focus pause */
  SpiderGamepadUI_OnFlip(game_hwnd,
    InterlockedCompareExchange(&menu_upscale_active,0,0)==1,
    InterlockedCompareExchange(&menu_upscale_active,0,0)!=1 &&
      InterlockedCompareExchange(&fov_game_height,0,0)>0,
    (*(const volatile DWORD *)(uintptr_t)0x006151F8u)!=0u,
    ((*(const volatile DWORD *)(uintptr_t)0x00682950u)!=0u ||
      native_pause43_visible_near_flip(count)));
  if(debug_log_enabled && count<=24) {
    char line[160]; snprintf(line,sizeof(line),"DDRAW: Surface7::Flip #%ld self=%p target=%p flags=0x%08lX",
                             (long)count,self,(void*)target,(unsigned long)flags); log_line(line);
  }
  /* The focus-loss pause blocks at this very Flip. Sample focus/cursor
   * BEFORE waiting so that the opt-in desktop cursor recovery can run
   * while Spider-Man is in the background, not only after Alt+Tab back.
   * If we waited, sample again immediately after returning to balance
   * our temporary ShowCursor(TRUE) increments on the same game thread.
   * Both samples are passive unless the explicit cursor experiment is on. */
  {
    BOOL was_background=(game_hwnd && GetForegroundWindow()!=game_hwnd);
    SpiderInputFocus_OnFlip(count);
    pause_probe_native_menu_focus(count); /* read-only native ESC Pause heartbeat */
    pause_at_flip_if_background();
    if(was_background &&
       InterlockedCompareExchange(&pause_on_focus_loss,0,0)==1)
      SpiderInputFocus_OnFlip(count);
  }
  SpiderDI8Retry_OnFlip(count);
  SpiderNativeUnplug_OnFlip(count);
  /* Keep the legacy live-adapter pre-Flip release barrier before SDL3 input. */
  SpiderSDL3GameplayLive_PreNativeOnFlip(game_hwnd);
  /* Separate default-OFF Left Alt-only native focus-latch experiment.
   * Actual confirmed external loss/return is recorded by FOCUS-PAUSE,
   * never inferred from mere SDL3 sample-eligibility transitions. */
  SpiderAltLatch_OnFlip(game_hwnd,
    InterlockedCompareExchange(&menu_upscale_active,0,0)==1,
    InterlockedCompareExchange(&menu_upscale_active,0,0)!=1 &&
      InterlockedCompareExchange(&fov_game_height,0,0)>0,
    (*(const volatile DWORD *)(uintptr_t)0x006151F8u)!=0u,
    ((*(const volatile DWORD *)(uintptr_t)0x00682950u)!=0u ||
      native_pause43_visible_near_flip(count) ||
      InterlockedCompareExchange(&pause_latched,0,0)==1 ||
      InterlockedCompareExchange(&pause_in_progress,0,0)==1));
  /* Read-only SDL3 game-action diagnostics; never claim a native DI owner. */
  SpiderSDL3GameplayPlan_OnFlip(game_hwnd,
    InterlockedCompareExchange(&menu_upscale_active,0,0)==1,
    InterlockedCompareExchange(&menu_upscale_active,0,0)!=1 &&
      InterlockedCompareExchange(&fov_game_height,0,0)>0,
    (*(const volatile DWORD *)(uintptr_t)0x006151F8u)!=0u,
    ((*(const volatile DWORD *)(uintptr_t)0x00682950u)!=0u ||
      native_pause43_visible_near_flip(count) ||
      InterlockedCompareExchange(&pause_latched,0,0)==1 ||
      InterlockedCompareExchange(&pause_in_progress,0,0)==1));
  /* Built-in SDL3 profile -> game keyboard adapter; native DI stays isolated. */
  SpiderSDL3Universal_OnFlip(game_hwnd,
    InterlockedCompareExchange(&menu_upscale_active,0,0)==1,
    InterlockedCompareExchange(&menu_upscale_active,0,0)!=1 &&
      InterlockedCompareExchange(&fov_game_height,0,0)>0,
    (*(const volatile DWORD *)(uintptr_t)0x006151F8u)!=0u,
    ((*(const volatile DWORD *)(uintptr_t)0x00682950u)!=0u ||
      native_pause43_visible_near_flip(count) ||
      InterlockedCompareExchange(&pause_latched,0,0)==1 ||
      InterlockedCompareExchange(&pause_in_progress,0,0)==1));
  /* Observe the physical F9 press early; the game's screenshot code can
   * run between two Flip calls, so surface hooks also sample F9. */
  if(InterlockedCompareExchange(&f9_trace_enabled,0,0)==1)
    f9_readback_window();
  report_null_fclose_recovery();
  if (g_frame_limiter_ready) SpiderFrameLimiter_Wait(&g_frame_limiter);
  /* Deliberately NO 1px pixel cover-up. The yellow seam remains visible if
   * rasterization is incorrect. Keep only genuine unused 4:3 letterbox/pillarbox
   * margins black. These fills do not touch the game presentation rectangle. */
  if (InterlockedCompareExchange(&gameplay_4x3_active,0,0)==1 &&
      original_get_attached && original_blt) {
    DDSCAPS2 caps;IDirectDrawSurface7 *back=NULL;
    RECT borders[4];unsigned i,nb=0;
    memset(&caps,0,sizeof(caps));caps.dwCaps=DDSCAPS_BACKBUFFER;
    if(game_map.left>0) {RECT r={0,0,game_map.left,(LONG)game_output_h};borders[nb++]=r;}
    if(game_map.right<(int)game_output_w) {RECT r={game_map.right,0,(LONG)game_output_w,(LONG)game_output_h};borders[nb++]=r;}
    if(game_map.top>0) {RECT r={game_map.left,0,game_map.right,game_map.top};borders[nb++]=r;}
    if(game_map.bottom<(int)game_output_h) {RECT r={game_map.left,game_map.bottom,game_map.right,(LONG)game_output_h};borders[nb++]=r;}
    if(nb && SUCCEEDED(borderless_get_back(self,&caps,&back)) && back) {
      DDBLTFX fill;memset(&fill,0,sizeof(fill));fill.dwSize=sizeof(fill);fill.dwFillColor=0;
      for(i=0;i<nb;i++) {
        HRESULT r=original_blt(back,&borders[i],NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fill);
        if(FAILED(r) && InterlockedIncrement(&gameplay_4x3_bar_errors)<=4)
          log_line("DISPLAY-ASPECT-43: WARNING black margin fill failed");
      }
      IDirectDrawSurface7_Release(back);
    }
  }
  int menu_active=InterlockedCompareExchange(&menu_upscale_active,0,0)==1;
  LONG menu_draws=InterlockedExchange(&menu_version_draws,0);
  LONG movie_blits=InterlockedExchange(&menu_version_movie_blits,0);
  LONG movie_last=InterlockedCompareExchange(&menu_version_last_movie_flip,0,0);
  BOOL permitted=menu_active && menu_draws>=3 && movie_blits==0 &&
     count-movie_last>3 &&
     menu_map.right>menu_map.left && menu_map.bottom>menu_map.top;
  BOOL back_drawn=FALSE;
  if(debug_log_enabled && menu_active && count%120==0) {
    char line[180];
    snprintf(line,sizeof(line),
      "MENU-VERSION: flip=%ld draws=%ld movie-blits=%ld last-movie=%ld permitted=%d",
      (long)count,(long)menu_draws,(long)movie_blits,(long)movie_last,(int)permitted);
    log_line(line);
  }
  if(permitted && borderless_back) {
    RECT content={menu_map.left,menu_map.top,menu_map.right,menu_map.bottom};
    back_drawn=SpiderMenuVersion_DrawToSurface(borderless_back,&content);
    if(back_drawn && InterlockedCompareExchange(&menu_version_logged,1,0)==0)
      log_line("MENU-VERSION: drawn on DirectDraw backbuffer: " SPIDERMAN2K1FIX_VERSION_LABEL);
  }
  /* F9 also supports the two static title/slate screens before the main
   * menu: these draw a single 3D primitive each frame. Keep the stricter
   * "permitted" predicate above for the menu-version overlay, while PNG
   * needs only a fresh frame with some geometry and no active movie Blt.
   * Capture BEFORE presentation and backbuffer clear. */
  if(InterlockedCompareExchange(&f9_png_only_enabled,0,0)==1 &&
     InterlockedCompareExchange(&f9_menu_png_pending,0,0)==1) {
    BOOL f9_still_ready=menu_active && menu_draws>=1 && movie_blits==0 &&
      count-movie_last>3 && menu_map.right>menu_map.left &&
      menu_map.bottom>menu_map.top;
    f9_capture_completed_menu_flip(count,f9_still_ready);
  }
  if(self==(void*)borderless_primary && borderless_back && original_blt) {
      static DWORD last_toast_pub_log=0,last_toast_shown_log=0;
      RECT rc={borderless_monitor_x,borderless_monitor_y,
        borderless_monitor_x+(LONG)desktop_w,borderless_monitor_y+(LONG)desktop_h};
      IDirectDrawSurface7 *present_source=borderless_back;
      if(InterlockedCompareExchange(&f9_native_png_enabled,0,0)==1) {
        IDirectDrawSurface7 *composite=
          SpiderF9Toast_CompositeFrame(borderless_back,game_hwnd,desktop_w,desktop_h);
        if(composite)present_source=composite;
      }
      hr=original_blt(self,&rc,present_source,NULL,DDBLT_WAIT,NULL);
      if(FAILED(hr) && present_source!=borderless_back) {
        /* A lost/unsupported staging surface must not blank gameplay. */
        SpiderF9Toast_ResetPresentation();
        hr=original_blt(self,&rc,borderless_back,NULL,DDBLT_WAIT,NULL);
      }
      if(FAILED(hr)) {
        LONG failures=InterlockedIncrement(&borderless_present_failed);
        if(failures<=12) {
          char line[180];
          snprintf(line,sizeof(line),"BORDERLESS: primary Blt FAILED hr=0x%08lX (no exclusive fallback)",(unsigned long)hr);
          log_line(line);
        }
      }
      /* Report first observed Flip and first ACTUAL onscreen presentation
       * for a PNG notification. Comic stills may not Flip during 2.5s. */
      if(debug_log_enabled && InterlockedCompareExchange(&f9_png_only_enabled,0,0)==1) {
        DWORD published=0,displayed=0;
        SpiderF9Toast_GetDisplayStatus(&published,&displayed);
        if(published && published!=last_toast_pub_log) {
          char msg[160];
          snprintf(msg,sizeof(msg),
            "F9-TOAST: generation=%lu first subsequent Flip=%ld painted=%d",
            (unsigned long)published,(long)count,displayed==published);
          log_line(msg);
          last_toast_pub_log=published;
        }
        if(displayed && displayed!=last_toast_shown_log) {
          char msg[150];
          snprintf(msg,sizeof(msg),
            "F9-TOAST: generation=%lu FIRST displayed at Flip=%ld",
            (unsigned long)displayed,(long)count);
          log_line(msg);
          last_toast_shown_log=displayed;
        }
      }
      /* One DirectDraw Blt above includes the notice when active. Never
       * paint PRIMARY asynchronously after presenting the finished frame. */
      /* GDI fallback only when the video-memory surface has no DC. */
      if(SUCCEEDED(hr) && permitted && !back_drawn) {
        RECT content={menu_map.left,menu_map.top,menu_map.right,menu_map.bottom};
        if(SpiderMenuVersion_DrawToWindow(game_hwnd,&content) &&
           InterlockedCompareExchange(&menu_version_logged,1,0)==0)
          log_line("MENU-VERSION: drawn through window GDI: " SPIDERMAN2K1FIX_VERSION_LABEL);
      }
      if(debug_log_enabled && (count<=12 || count%120==0)) {
        char line[190];
        snprintf(line,sizeof(line),"BORDERLESS: present #%ld backbuffer->window PRIMARY result=0x%08lX window=%lux%lu",
          (long)count,(unsigned long)hr,(unsigned long)desktop_w,(unsigned long)desktop_h);
        log_line(line);
      }
  } else {
    log_line("BORDERLESS: ERROR no windowed PRIMARY/back pair for Flip; exclusive Flip prohibited");
    hr=DDERR_INVALIDOBJECT;
  }
  /* Fill the newly available BACK buffer with black so the subsequent menu
   * frame cannot expose uninitialized/stale pixels outside its 4:3 area.
   * The current front buffer has already been presented by Flip.
   */
  if (SUCCEEDED(hr) && InterlockedCompareExchange(&menu_upscale_active,0,0)==1 &&
      original_get_attached && original_blt) {
    DDSCAPS2 caps; IDirectDrawSurface7 *back=NULL;
    memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
    if (SUCCEEDED(borderless_get_back(self,&caps,&back)) && back) {
      DDBLTFX fill;
      HRESULT cleared;
      memset(&fill,0,sizeof(fill)); fill.dwSize=sizeof(fill); fill.dwFillColor=0;
      cleared=original_blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fill);
      if (FAILED(cleared) && InterlockedIncrement(&menu_clear_failures)<=3) {
        char line[120];
        snprintf(line,sizeof(line),"DISPLAY-TEST: black backbuffer clear failed hr=0x%08lX",
                 (unsigned long)cleared);
        log_line(line);
      }
      IDirectDrawSurface7_Release(back);
    }
  }

  if(debug_log_enabled && count%120==0) {
    float factor=0.f;
    if(SpiderFOV_GetHorPlusScalar(&factor)) {
      char msg[210];
      snprintf(msg,sizeof(msg),
        "FOV-HOR+: factor=%.6g corrected=%ld skip-hud-scope=%ld skip-z-unknown=%ld skip-z-disabled=%ld skip-shape=%ld [TEST ONLY]",
        (double)factor,(long)InterlockedExchange(&fov_horplus_calls,0),
        (long)InterlockedExchange(&fov_horplus_skips_hud,0),
        (long)InterlockedExchange(&fov_horplus_skips_unknown,0),
        (long)InterlockedExchange(&fov_horplus_skips_depth,0),
        (long)InterlockedExchange(&fov_horplus_skips_shape,0));
      log_line(msg);
    }
  }
  if(debug_log_enabled && count<=24) {
    char line[100]; snprintf(line,sizeof(line),"DDRAW: Surface7::Flip result=0x%08lX",(unsigned long)hr); log_line(line);
  }
  return hr;
}
static void instrument_surface(IDirectDrawSurface7 *surface) {
  void ***object=(void ***)surface;
  void **vtable;
  if(!object) return;
  vtable=*object;
  if(!vtable) return;
  if(InterlockedCompareExchange(&surface_vtable_ready,1,0)==0) {
    memcpy(patched_surface_vtable,vtable,sizeof(patched_surface_vtable));
    original_blt=(SurfaceBltFn)vtable[5];
    original_flip=(SurfaceFlipFn)vtable[11];
    original_get_attached=(SurfaceGetAttachedFn)vtable[12];
    patched_surface_vtable[5]=(void *)hooked_blt;
    patched_surface_vtable[11]=(void *)hooked_flip;
    patched_surface_vtable[12]=(void *)hooked_get_attached;
    if(InterlockedCompareExchange(&f9_trace_enabled,0,0)==1 ||
       InterlockedCompareExchange(&f9_backbuffer_fix_enabled,0,0)==1) {
      original_get_dc=(SurfaceGetDCFn)vtable[17];
      original_surface_get_desc=(SurfaceGetDescFn)vtable[22];
      original_surface_lock=(SurfaceLockFn)vtable[25];
      original_release_dc=(SurfaceReleaseDCFn)vtable[26];
      original_surface_unlock=(SurfaceUnlockFn)vtable[32];
      patched_surface_vtable[17]=(void *)hooked_f9_surface_get_dc;
      patched_surface_vtable[22]=(void *)hooked_f9_surface_get_desc;
      patched_surface_vtable[25]=(void *)hooked_f9_surface_lock;
      patched_surface_vtable[26]=(void *)hooked_f9_surface_release_dc;
      patched_surface_vtable[32]=(void *)hooked_f9_surface_unlock;
      log_line("F9-TRACE: read-only DirectDraw7 GetDC/GetSurfaceDesc/Lock/ReleaseDC/Unlock diagnostics installed");
    }
    log_line("DDRAW: Surface7 vtable captured; Blt/Flip/GetAttachedSurface hooks ready");
    InterlockedExchange(&surface_vtable_ready,2);
  }
  if(surface_vtable_ready==2 && original_blt && original_flip &&
     original_get_attached &&
     vtable[5]==(void*)original_blt && vtable[11]==(void*)original_flip &&
     vtable[12]==(void*)original_get_attached)
  {
    *object=patched_surface_vtable;
    if (InterlockedIncrement(&flip_hooked_surfaces)==1)
      log_line("FPS-HOOK: IDirectDrawSurface7 Flip vtable replacement ACTIVE on at least one surface");
  }
  else if (surface_vtable_ready==2 &&
           vtable!=patched_surface_vtable &&
           InterlockedIncrement(&flip_hook_mismatches)==1)
    log_line("FPS-HOOK: WARNING: Surface7 vtable mismatch; some surfaces are NOT instrumented");
}
static HRESULT WINAPI hooked_surface_create(void *self, DDSURFACEDESC2 *desc,
                                            IDirectDrawSurface7 **out, IUnknown *outer) {
  HRESULT hr; LONG count=InterlockedIncrement(&surface_create_count);
  if(debug_log_enabled && count<=48) log_surface_desc(desc);
  /* Exclusive primary with backbuffer count 1 is unavailable with
   * DDSCL_NORMAL. Replace it with a *real* windowed primary + an offscreen
   * video-memory render target, exposed as a virtual BACKBUFFER. */
  if(desc && (desc->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE) &&
     (desc->ddsCaps.dwCaps&DDSCAPS_FLIP)) {
    DDSURFACEDESC2 pd,bd;IDirectDrawSurface7 *primary=NULL,*back=NULL;
    IDirectDrawClipper *clipper=NULL;HRESULT primary_hr,back_hr,clip_hr;
    char line[280];
    if(out) *out=NULL;
    if(!out || desktop_w<640 || desktop_h<480) return DDERR_INVALIDPARAMS;
    /* The game requests another flipping primary during MENU -> GAMEPLAY.
     * Native DirectDraw rejects a second primary (0x88760234).
     * Keep the original windowed primary/backbuffer and return a new COM
     * reference instead. Validate dimensions so incompatible surfaces are
     * not silently recycled across display changes.
     */
    if(borderless_primary && borderless_back) {
      DDSURFACEDESC2 existing;
      HRESULT check;
      memset(&existing,0,sizeof(existing));
      existing.dwSize=sizeof(existing);
      check=IDirectDrawSurface7_GetSurfaceDesc(borderless_back,&existing);
      if(FAILED(check) || existing.dwWidth!=desktop_w ||
         existing.dwHeight!=desktop_h) {
        snprintf(line,sizeof(line),
          "BORDERLESS: ERROR existing backbuffer incompatible hr=0x%08lX old=%lux%lu requested=%lux%lu",
          (unsigned long)check,(unsigned long)existing.dwWidth,
          (unsigned long)existing.dwHeight,
          (unsigned long)desktop_w,(unsigned long)desktop_h);
        log_line(line);
        return FAILED(check)?check:DDERR_INVALIDPARAMS;
      }
      IDirectDrawSurface7_AddRef(borderless_primary); /* game-owned ref */
      *out=borderless_primary;
      snprintf(line,sizeof(line),
        "BORDERLESS: REUSE windowed PRIMARY=%p backbuffer=%p size=%lux%lu (menu -> gameplay)",
        (void*)borderless_primary,(void*)borderless_back,
        (unsigned long)desktop_w,(unsigned long)desktop_h);
      log_line(line);
      return DD_OK;
    }
    if(borderless_primary || borderless_back) {
      log_line("BORDERLESS: ERROR incomplete PRIMARY/BACKBUFFER ownership; refusing unsafe recreation");
      return DDERR_INVALIDOBJECT;
    }
    memset(&pd,0,sizeof(pd));pd.dwSize=sizeof(pd);
    pd.dwFlags=DDSD_CAPS;
    pd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
    primary_hr=original_surface_create(self,&pd,&primary,outer);
    if(FAILED(primary_hr) || !primary) {
      snprintf(line,sizeof(line),"BORDERLESS: ERROR windowed PRIMARY create hr=0x%08lX",(unsigned long)primary_hr);
      log_line(line);return primary_hr;
    }
    clip_hr=IDirectDraw7_CreateClipper((IDirectDraw7*)self,0,&clipper,NULL);
    if(SUCCEEDED(clip_hr) && clipper) {
      clip_hr=IDirectDrawClipper_SetHWnd(clipper,0,game_hwnd);
      if(SUCCEEDED(clip_hr)) clip_hr=IDirectDrawSurface7_SetClipper(primary,clipper);
      IDirectDrawClipper_Release(clipper);
    }
    if(FAILED(clip_hr)) {
      snprintf(line,sizeof(line),"BORDERLESS: ERROR primary clipper hr=0x%08lX",(unsigned long)clip_hr);
      log_line(line);
      IDirectDrawSurface7_Release(primary);return clip_hr;
    }
    memset(&bd,0,sizeof(bd));bd.dwSize=sizeof(bd);
    bd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
    bd.dwWidth=desktop_w;bd.dwHeight=desktop_h;
    bd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
    back_hr=original_surface_create(self,&bd,&back,NULL);
    if(FAILED(back_hr) || !back) {
      snprintf(line,sizeof(line),"BORDERLESS: ERROR offscreen D3D7 backbuffer create hr=0x%08lX",(unsigned long)back_hr);
      log_line(line);
      IDirectDrawSurface7_Release(primary);
      return back_hr;
    }
    borderless_primary=primary;borderless_back=back;
    /* CreateSurface gives the initial reference to the game. Keep one
     * separate proxy-owned reference: the game's menu cleanup may Release
     * its primary before requesting the gameplay primary. */
    IDirectDrawSurface7_AddRef(primary);
    instrument_surface(primary);
    instrument_surface(back);
    *out=primary;
    snprintf(line,sizeof(line),
      "BORDERLESS: primary=%p backbuffer=%p size=%lux%lu clipper=OK; no DDSCAPS_FLIP",
      (void*)primary,(void*)back,(unsigned long)desktop_w,(unsigned long)desktop_h);
    log_line(line);
    return DD_OK;
  }
  /* Only the observed D3D depth buffer follows the physical backbuffer
   * dimensions in menu-upscale mode. Texture surfaces stay unchanged.
   */
  if (desc && (InterlockedCompareExchange(&menu_upscale_active,0,0)==1 ||
               InterlockedCompareExchange(&gameplay_4x3_active,0,0)==1) &&
      (desc->dwFlags&(DDSD_WIDTH|DDSD_HEIGHT))==(DDSD_WIDTH|DDSD_HEIGHT) &&
      (desc->ddsCaps.dwCaps&DDSCAPS_ZBUFFER) &&
      ((InterlockedCompareExchange(&menu_upscale_active,0,0)==1 &&
        desc->dwWidth==640 && desc->dwHeight==480) ||
       (InterlockedCompareExchange(&gameplay_4x3_active,0,0)==1 &&
        desc->dwWidth==game_logical_w && desc->dwHeight==game_logical_h))) {
    DDSURFACEDESC2 native=*desc;
    native.dwWidth=InterlockedCompareExchange(&menu_upscale_active,0,0)==1 ? desktop_w : game_output_w;
    native.dwHeight=InterlockedCompareExchange(&menu_upscale_active,0,0)==1 ? desktop_h : game_output_h;
    hr=original_surface_create(self,&native,out,outer);
    {
      char line[176];
      snprintf(line,sizeof(line),"DISPLAY-TEST: depth surface %lux%lu -> %lux%lu hr=0x%08lX",
               (unsigned long)desc->dwWidth,(unsigned long)desc->dwHeight,
               (unsigned long)native.dwWidth,(unsigned long)native.dwHeight,(unsigned long)hr);
      log_line(line);
    }
  } else hr=original_surface_create(self,desc,out,outer);
  if(count<=48) {
    char line[160]; snprintf(line,sizeof(line),"DDRAW: CreateSurface #%ld result=0x%08lX surface=%p",
                             (long)count,(unsigned long)hr,(void*)(out?*out:NULL)); log_line(line);
  }
  if(SUCCEEDED(hr) && out && *out) instrument_surface(*out);
  return hr;
}
static HRESULT WINAPI hooked_coop(void *self, HWND hwnd, DWORD flags) {
  char line[192];
  HRESULT hr;
  snprintf(line,sizeof(line),"BORDERLESS: intercept SetCooperativeLevel hwnd=%p requested=0x%08lX -> DDSCL_NORMAL (no exclusive)",
           (void*)hwnd,(unsigned long)flags);
  log_line(line);
  game_hwnd=hwnd;
  SpiderQuickExitF12_AttachWindow(hwnd);
  /* Only the idle-scene notification fallback uses a separate no-focus
   * translucent HWND, and only if the game stops issuing Flip. */
  SpiderF9Toast_SetGameWindow(hwnd,log_line);
  probe_desktop_mode(hwnd);
  log_monitor(hwnd);
  hr=original_coop(self,hwnd,DDSCL_NORMAL);
  if(SUCCEEDED(hr)) {
    borderless_window_apply();
    SpiderInputFocus_AttachWindow(hwnd);
  }
  else {
    snprintf(line,sizeof(line),"BORDERLESS: ERROR DDSCL_NORMAL rejected hr=0x%08lX",(unsigned long)hr);
    log_line(line);
  }
  return hr;
}
static HRESULT WINAPI hooked_mode(void *self,DWORD w,DWORD h,DWORD bpp,DWORD hz,DWORD flags) {
  char line[240]; HRESULT hr;
  (void)self;
  snprintf(line,sizeof(line),"DDRAW: SetDisplayMode REQUEST %lux%lu %lu bpp refresh=%lu flags=0x%08lX",
           (unsigned long)w,(unsigned long)h,(unsigned long)bpp,(unsigned long)hz,(unsigned long)flags);
  log_line(line);
  /* A 640x480/16bpp request is the game's LEGACY MENU mode, never a
   * request to change the physical monitor into that mode. In particular,
   * DO NOT retry it after any upscale failure: an unsupported physical
   * mode can leave the user's monitor black while intro audio continues.
   * The game still owns logical 640x480 menu coordinates and its original
   * 16bpp source/texture surfaces; the desktop output is 32bpp.
   */
  if(w==640 && h==480 && bpp==16) {
    if(InterlockedCompareExchange(&menu_upscale_enabled,0,0)!=1 ||
       desktop_w<800 || desktop_h<600 ||
       !menu_map.output_w || !menu_map.output_h) {
      log_line("DISPLAY-MENU-SAFE: BLOCKED physical 640x480x16; native desktop mapping unavailable (no unsafe fallback)");
      return DDERR_UNSUPPORTED;
    }
    snprintf(line,sizeof(line),
      "DISPLAY-MENU-SAFE: INTERCEPT 640x480x16 => physical %lux%lux32 (logical menu remains 640x480)",
      (unsigned long)desktop_w,(unsigned long)desktop_h);
    log_line(line);
    /* Do not forward original bpp=16: mode 1920x1080x16 is also often
     * unsupported by Wine/display drivers even when 1920x1080x32 works. */
    hr=DD_OK; /* borderless window: keep monitor mode unchanged */
    snprintf(line,sizeof(line),
      "BORDERLESS: MENU mode acknowledged %lux%lux32 hr=0x%08lX (no physical SetDisplayMode call)",
      (unsigned long)desktop_w,(unsigned long)desktop_h,(unsigned long)hr);
    log_line(line);
    InterlockedIncrement(&borderless_legacy_mode_calls);
    borderless_window_apply();
    InterlockedExchange(&menu_upscale_active,1);
    InterlockedExchange(&gameplay_4x3_active,0);
    InterlockedExchange(&fov_game_height,0);
    SpiderFOV_SetStage(0,(unsigned)desktop_w,(unsigned)desktop_h);
    snprintf(line,sizeof(line),
      "DISPLAY-TEST: MENU logical=640x480 physical=%lux%lux32 content=(%d,%d)-(%d,%d) [ACTIVE]",
      (unsigned long)desktop_w,(unsigned long)desktop_h,
      menu_map.left,menu_map.top,menu_map.right,menu_map.bottom);
    log_line(line);
    return hr;
  }
  /* Match only the expected internal game mode. The physical output is
   * the selected display mode, NOT the internal 4:3 logical dimensions.
   * Never forward the unsupported logical mode after a failed native call. */
  if(InterlockedCompareExchange(&gameplay_4x3_enabled,0,0)==1 &&
     w==game_logical_w && h==game_logical_h && bpp==32) {
    snprintf(line,sizeof(line),
      "DISPLAY-ASPECT-43: GAMEPLAY logical SetDisplayMode %lux%lux32 -> native output %lux%lux32",
      (unsigned long)w,(unsigned long)h,(unsigned long)game_output_w,(unsigned long)game_output_h);
    log_line(line);
    hr=DD_OK; /* borderless window: logical mode request acknowledged */
    InterlockedExchange(&gameplay_4x3_active,1);
  } else {
    InterlockedExchange(&gameplay_4x3_active,0);
    hr=DD_OK; /* borderless window: never change monitor resolution */
  }
  if (SUCCEEDED(hr)) {
    InterlockedIncrement(&borderless_legacy_mode_calls);
    borderless_window_apply();
    InterlockedExchange(&fov_game_height,(LONG)h);
    SpiderFOV_SetStage(1,(unsigned)w,(unsigned)h);
    if(InterlockedCompareExchange(&gameplay_4x3_active,0,0)==1) {
      snprintf(line,sizeof(line),
       "DISPLAY-ASPECT-43: ACTIVE logical=%lux%lu physical=%lux%lu rect=(%d,%d)-(%d,%d) [ORIGINAL 4:3 FOV]",
       (unsigned long)w,(unsigned long)h,
       (unsigned long)game_output_w,(unsigned long)game_output_h,
       game_map.left,game_map.top,game_map.right,game_map.bottom);
      log_line(line);
    }
    if (InterlockedExchange(&menu_upscale_active,0)==1) {
        f9_movie_drop_source(); /* No retained intro decoder surface in 3D. */
        log_line("BORDERLESS: MENU exited; windowed gameplay presentation active");
    }
  }
  return hr;
}
static HRESULT WINAPI hooked_create(GUID *guid,void **out,REFIID iid,IUnknown *outer) {
  /* The display device is created before SetCooperativeLevel. Determine the
   * actual game-window monitor NOW, before the virtual Spidey.cfg is read
   * into the original game. This prevents primary-vs-secondary monitor
   * resolution disagreement on Windows 11 or Wine. */
  if(!desktop_w || !desktop_h) {
    HWND early=spider_find_game_window_before_ddraw();
    if(early) {
      log_line("DISPLAY-NATIVE: pre-bootstrap game HWND identified; capturing its monitor");
      probe_desktop_mode(early);
    } else {
      log_line("DISPLAY-NATIVE: WARNING game HWND unavailable before virtual config bootstrap; fallback to current primary monitor until SetCooperativeLevel");
    }
  }
  /* SDL3 semantic profiles are the only patch-provided controller
   * mappings. No Sony/Xbox physical button presets are installed in
   * the original game's virtual config. Keep the native game defaults. */
  SpiderCfg_BootstrapGameRead();
  SpiderCfg_InstallModernKeyboardDefaults();
  log_line("SDL3-PROFILE: pre-SDL3 fixed Sony/Xbox mapping bootstrap REMOVED; native joystick mapping unchanged");
  HRESULT hr=original_create(guid,out,iid,outer);
  if(SUCCEEDED(hr) && out && *out && iid && IsEqualGUID(iid,&IID_IDirectDraw7)) {
    void ***object=(void ***)*out;
    void **vtable=*object;
    if(InterlockedCompareExchange(&ddraw_vtable_ready,1,0)==0) {
      memcpy(patched_ddraw_vtable,vtable,sizeof(patched_ddraw_vtable));
      original_dd_query=(DDQueryFn)vtable[0];
      original_surface_create=(SurfaceCreateFn)vtable[6];
      original_coop=(CoopFn)vtable[20];
      original_mode=(ModeFn)vtable[21];
      patched_ddraw_vtable[0]=(void *)hooked_dd_query;
      patched_ddraw_vtable[6]=(void *)hooked_surface_create;
      patched_ddraw_vtable[20]=(void *)hooked_coop;
      patched_ddraw_vtable[21]=(void *)hooked_mode;
      log_line("BORDERLESS: IDirectDraw7 hooks ready; DDSCL_NORMAL + windowed PRIMARY/BACKBUFFER virtualization");
      InterlockedExchange(&ddraw_vtable_ready,2);
    }
    if(ddraw_vtable_ready==2 && original_coop && original_mode && original_surface_create &&
       vtable[0]==(void*)original_dd_query && vtable[6]==(void*)original_surface_create && vtable[20]==(void*)original_coop &&
       vtable[21]==(void*)original_mode)
      *object=patched_ddraw_vtable;
  } else {
    log_line("DDRAW: DirectDrawCreateEx returned (non-DDraw7 interface or error)");
  }
  return hr;
}
static void install_ddraw_hook(void) {
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  void **slot;
  HMODULE ddraw;
  FARPROC target;
  DWORD old,ignored;
  if(!base) return;
  dos=(IMAGE_DOS_HEADER*)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000) return;
  nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     nt->OptionalHeader.ImageBase!=0x400000 || (uintptr_t)base!=0x400000 ||
     nt->OptionalHeader.SizeOfImage<=0x13b014) return;
  /* The imported IAT RVA was checked against the user's exact executable. */
  slot=(void**)(base+0x13b010);
  ddraw=GetModuleHandleA("ddraw.dll");
  if(!ddraw) { log_line("DDRAW: ddraw.dll not loaded yet; hook not installed"); return; }
  target=GetProcAddress(ddraw,"DirectDrawCreateEx");
  if(!target || *slot!=(void*)target) {
    log_line("DDRAW: IAT target mismatch; hook not installed"); return;
  }
  original_create=(CreateExFn)*slot;
  if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&old)) {
    log_line("DDRAW: IAT VirtualProtect failed"); return;
  }
  *slot=(void*)hooked_create;
  VirtualProtect(slot,sizeof(*slot),old,&ignored);
  log_line("DDRAW: DirectDrawCreateEx IAT hook installed");
}
/* Called only after supported_exe() has verified the exact disk SHA-256. */
static void install_player_hud(BOOL exe_verified) {
  /* Built in for widescreen gameplay. No HUD switch; original 4:3 keeps
   * the original game geometry, and all native signatures still guard the
   * patch before any write to the supported executable's memory. */
  if(lstrcmpiA(display_aspect_setting,"4:3")==0) {
    log_line("HUD: original 4:3 game geometry; widescreen correction not needed");
    return;
  }
  if(!SpiderHudWidescreen_Install(exe_verified,TRUE)) {
    log_line("HUD: ERROR: native 16-site install failed, HUD correction NOT enabled");
    SpiderFOV_DisableUnsafeHorPlus();
    return;
  }
  InterlockedExchange(&fov_horplus_scopes_ready,1);
  log_line("HUD: permanent widescreen correction enabled (native 16-site player HUD installed)");
  log_line("HUD-ADAPTIVE: strict geometry correction enabled for compass, instructions and boss UI");
}

/* The proxy's early IAT gate can reach the game main thread before our
 * background worker. InitOnce makes initialization exactly once and makes
 * the first DirectDrawCreateEx wait until all native and virtual-config
 * hooks are installed. Never wait under DllMain/loader lock. */
static INIT_ONCE core_start_once=INIT_ONCE_STATIC_INIT;
static volatile LONG core_start_success=0;

/* A one-shot watchdog diagnoses the crucial "60 FPS despite 30 limit"
 * case even if Flip is never intercepted. Runs in the existing worker thread
 * after hook setup, outside the loader lock. Does not change game timing.
 */
static DWORD initialize_body(LPVOID unused) {
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  char line[160]; (void)unused;
  debug_log_load_setting();
  log_line("START: " SPIDERMAN2K1FIX_VERSION_LABEL);
  if (!supported_exe()) {
    log_line("ERROR: unsupported EXE SHA-256; do not use this proxy with this executable");
    return 0;
  }
  log_line("OK: supported SpideyPC.exe SHA-256 verified");
  if(focus_timeline_enabled)
    log_line("FOCUS-TIMELINE: ENABLED read-only Blt/Flip samples + local wall-clock timestamps; requires separate FOCUS_PASSIVE=1 for WndProc events");
  install_null_fclose_guard(); /* Default ON only for SHA-verified EXE and exact native opcode signature. */
  /* Generate complete defaults once, before any GetPrivateProfile... reads. */
  SpiderCfg_EnsureDefaultIni(log_line);
  /* Repair invalid/unknown INI entries before reading any game settings. */
  SpiderCfg_ValidateCurrentIni(log_line);
  /* The supported executable is already SHA-verified and the logger is
   * enabled only with [Debug] Log=1. Read-only inventory, never DllMain. */
  if(debug_log_enabled)SpiderSystemDiagnostics_Log(log_line);
  /* Passive profile setup only: independent DI8 discovery begins later,
   * on the game's real window and Flip thread, never under loader lock.
   * All original gameplay controls are preserved. */
  {
    char input_ini[MAX_PATH];
    if(spider_path_ini_a(input_ini,sizeof(input_ini))) {
      SpiderGamepadUI_Init(input_ini,log_line);
    } else {
      log_line("GAMEPAD-UI: OFF missing executable-relative INI path");
    }
  }
  SpiderQuickExitF12_Init(log_line); /* Optional [System] QuickExitF12=1 only. */
  pause_apply_windows_ghosting_option(); /* Opt-in fix experiment for spurious Windows Ghost focus. */
  /* Native DIERR_NOTACQUIRED retry remains opt-in; read the validated INI.
   * No COM-vtable interception. The actual patch is delayed to Flip #120. */
  SpiderDI8Retry_Init(log_line);
  /* Keep the SHA-verified original DI unplug crash guard, not a native
   * controller reconnection or fallback mapping backend. */
  {
    char ini[MAX_PATH];
    if(spider_path_ini_a(ini,sizeof(ini))) {
      SpiderNativeUnplug_Init(ini,log_line);
      SpiderSDL3Probe_Init(ini,log_line);
      SpiderSDL3Universal_Init(ini,log_line);
      SpiderSDL3GameplayPlan_Init(ini,log_line);
      SpiderAltLatch_Init(ini,log_line);
    } else {
      SpiderNativeUnplug_Init(NULL,log_line);
      SpiderSDL3Probe_Init(NULL,log_line);
      SpiderSDL3Universal_Init(NULL,log_line);
      SpiderSDL3GameplayPlan_Init(NULL,log_line);
      SpiderAltLatch_Init(NULL,log_line);
    }
  }
  SpiderFocusMedia_Init(log_line); /* Opt-in native Bink pause. */
  SpiderFocusDSound_Init(log_line); /* Separately opt-in game-owned secondary buffers only. */
  SpiderPauseSurface_Init(log_line); /* Pause drawn into the game backbuffer; no HWND overlays. */
  {
    char ini[MAX_PATH];
    /* Native WAV final-sector fix is built in; no INI key is needed. */
    int pkr_index=0,pkr_play_trace=0,pkr_heli_declick=0,pkr_loader_tail=1;
    if(spider_path_ini_a(ini,sizeof(ini))) {
      pkr_index=GetPrivateProfileIntA("Debug","ExperimentalPKRAudioIndex",0,ini)==1;
      pkr_play_trace=GetPrivateProfileIntA("Debug","ExperimentalPKRAudioPlayTrace",0,ini)==1;
      pkr_heli_declick=GetPrivateProfileIntA("Debug","ExperimentalAudioHeli1Declick",0,ini)==1;
      /* Keep an undocumented, manual opt-out for troubleshooting old installs. */
      pkr_loader_tail=GetPrivateProfileIntA("Debug","ExperimentalAudioLoaderTail",1,ini)==1;
    }
    SpiderPKRAudioIndex_Init(log_line,pkr_index||pkr_heli_declick||pkr_loader_tail);
    SpiderPKRAudioIndex_EnableHeli1Declick(pkr_heli_declick);
    SpiderPKRAudioPlayHook_Init(log_line,pkr_index||pkr_play_trace||pkr_heli_declick,pkr_play_trace);
    SpiderPKRAudioLoader_Init(log_line,pkr_loader_tail);
  }
  /* The first-run generator or repair may have changed [Debug] Log. */
  {
    int previous_log_enabled=debug_log_enabled;
    debug_log_load_setting();
    if (debug_log_enabled && !previous_log_enabled)
      log_line("START: SpiderMan2k1Fix debug logging enabled by generated/repaired INI");
  }
  if (base && base[0x1163E0]==0x31 && base[0x1163E1]==0xC0 && base[0x1163E2]==0xC3)
    log_line("OK: early No-CD memory patch present at RVA 0x1163E0");
  else {
    snprintf(line,sizeof(line),"ERROR: early No-CD patch not present; first bytes=%02X %02X %02X",base?base[0x1163E0]:0,base?base[0x1163E1]:0,base?base[0x1163E2]:0);
    log_line(line);
  }
  log_line("DISPLAY: 4:3 menu and intro presentation enabled");
  {
    char ini[MAX_PATH];
    int fps=30;
    if(spider_path_ini_a(ini,sizeof(ini))) {
      {
        char fps_value[48],*end=NULL;
        long number;
        GetPrivateProfileStringA("Display","FPSLimit","",fps_value,sizeof(fps_value),ini);
        if(fps_value[0]) {
          errno=0;
          number=strtol(fps_value,&end,10);
          while(end && (*end==' ' || *end=='\t'))end++;
          if(!errno && end!=fps_value && *end==0 && number>=0 && number<=240)
            fps=(int)number;
          else log_line("INI-CHECK: invalid FPSLimit, using default 30");
        }
      }
      {
        char aspect[40];
        GetPrivateProfileStringA("Display","AspectRatio","Auto",aspect,sizeof(aspect),ini);
        /* The physical display target is chosen only from the actual
         * game-window monitor by probe_desktop_mode(), not from a blind
         * EnumDisplaySettings(NULL) of the PRIMARY monitor.
         * Both modes keep that native physical output; 4:3 is logical. */
        strcpy(display_resolution_setting,"Auto");
        if(lstrcmpiA(aspect,"4:3")!=0 && lstrcmpiA(aspect,"Auto")!=0) {
          log_line("DISPLAY-ASPECT: unknown aspect policy; using Auto");
          strcpy(aspect,"Auto");
        }
        strcpy(display_aspect_setting,aspect);
        if(lstrcmpiA(aspect,"4:3")==0)
          log_line("DISPLAY-ASPECT-43: logical 4:3 resolution will be computed from the selected game monitor");
        SpiderFOV_Init("Auto",1,0,log_line);
        if(lstrcmpiA(aspect,"4:3")==0)
          log_line("DISPLAY-ASPECT-43: automatic FOV uses original 4:3 baseline 1.0");

        /* The 4:3 menu, intro movie sizes and native comic correction are
         * permanent. Unsafe AnySize and experimental HorPlus remain OFF. */
        InterlockedExchange(&menu_upscale_enabled,1);
        InterlockedExchange(&media_viewport_4x3_enabled,1);
        InterlockedExchange(&media_native_blt_4x3_enabled,1);
        InterlockedExchange(&media_native_blt_knownsizes_enabled,1);
        InterlockedExchange(&media_native_blt_anysize_enabled,0);
        InterlockedExchange(&comic_native_aspect_enabled,
           lstrcmpiA(aspect,"4:3")==0?0:1);
        log_line("BORDERLESS: native menu and opening movies preserve 4:3 (permanent)");
      }
    }
    if (fps<0 || fps>240) {
      log_line("FPS: invalid FPSLimit (accepted 0..240); defaulting to 30");
      fps=30;
    }
    log_line(menu_upscale_enabled==1 ?
      "BORDERLESS: menu virtual 640x480 => window-centered 4:3 (always ON)" :
      "BORDERLESS: ERROR required menu mapping disabled");
    log_line(media_viewport_4x3_enabled==1 ?
      "DISPLAY-MEDIA: ExperimentalMediaAspect=1 (4:3 physical D3D7 viewport; DirectDraw media observation active)" :
      "DISPLAY-MEDIA: ExperimentalMediaAspect=0 (legacy native-size D3D7 viewport)");
    log_line(media_native_blt_4x3_enabled==1 ?
      "DISPLAY-MEDIA: ExperimentalMediaBlt4x3=1 (native primary/backbuffer final-copy remap enabled)" :
      "DISPLAY-MEDIA: ExperimentalMediaBlt4x3=0 (DirectDraw copies remain unchanged)");
    log_line(media_native_blt_knownsizes_enabled==1 ?
      "DISPLAY-MEDIA: ExperimentalMediaBltKnownSizes=1 (only observed intro sources 640x480, 320x192, 320x240)" :
      "DISPLAY-MEDIA: ExperimentalMediaBltKnownSizes=0 (only original 640x480 unless AnySize opted in)");
    log_line("BORDERLESS-INTRO: 640x480 / 320x192 / 320x240 all centered 4:3 (Auto or 4:3); black margins; 5:3 trial disabled");
    log_line(media_native_blt_anysize_enabled==1 ?
      "DISPLAY-MEDIA: ExperimentalMediaBltAnySize=1 (all qualifying intro source sizes use the same centered 4:3 presentation)" :
      "DISPLAY-MEDIA: ExperimentalMediaBltAnySize=0 (previous 640x480-only Blt filter)");
    /* The final output is logged after the first monitor/DirectDraw probe. */
    log_line(comic_native_aspect_enabled==1 ?
      "COMIC-NATIVE-FIX: ExperimentalComicNativeAspect=1 (only native fullscreen comics, opt-in)" :
      "COMIC-NATIVE-FIX: ExperimentalComicNativeAspect=0 (native comic rendering unchanged)");
    g_frame_limiter_ready=SpiderFrameLimiter_Init(&g_frame_limiter,(unsigned)fps);
    if (!g_frame_limiter_ready) log_line("FPS: ERROR: high-resolution timer initialization failed; limiter disabled");
    else {
      snprintf(line,sizeof(line),"FPS: DirectDraw Flip limiter target=%d FPS (0=disabled)",fps);
      log_line(line);
    }
  }
  log_line("BORDERLESS: windowed fullscreen enabled");
  /* F9 always uses lossless PNG on the SHA-verified game.
   * No INI choice, environment toggle, or Log=1 prerequisite.
   * Preserve game-owned DIK_F9 detection, sound and 500ms debounce.
   * If the two exact original CALLs cannot be patched, leave game code
   * untouched and report the safety fallback when logging is enabled. */
  install_f9_png_only_dispatch();
  /* Optional read-only F9 diagnostics remain independent of output format.
   * Legacy screenshot-format environment variables are intentionally ignored. */
  if(debug_log_enabled) {
    char setting[8]={0};
    if(GetEnvironmentVariableA("SPIDERMAN2K1FIX_F9_TRACE",setting,sizeof(setting))==1 &&
       setting[0]=='1') {
      InterlockedExchange(&f9_trace_enabled,1);
      log_line("F9-TRACE: optional native screenshot readback observation armed");
    }
  }
  install_player_hud(TRUE);
  /* Launch-only opt-in: no INI repair or changes to the default setup.
   * Disable early native HUD X changes so D3D7 receives source vertices
   * and transforms each accepted screen-space primitive exactly once. */
  {
    char setting[4]={0};
    DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_UI_CANVAS_TEST",
                                     setting,sizeof(setting));
    if(n==1 && setting[0]=='1' &&
       SpiderHudWidescreen_Installed() &&
       lstrcmpiA(display_aspect_setting,"4:3")!=0) {
      SpiderHudWidescreen_SetUICanvasPrototype(TRUE);
      InterlockedExchange(&ui_canvas_trial_enabled,1);
      log_line("UI-CANVAS-43: EXPERIMENT ON; common centered 4:3 gameplay UI mapping, native HUD X edits skipped; HARDWARE UNVERIFIED");
    } else {
      log_line("UI-CANVAS-43: OFF; legacy verified HUD and native menus unchanged");
    }
  }
  /* Final original-native V25 trial. Earlier V6–V24 environment flags
   * are intentionally ignored. Keep explicitly opted in until native
   * Windows and ultrawide visual coverage has been verified.
   */
  {
    char flag[4]={0};
    if(InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)==1 &&
       GetEnvironmentVariableA(
          "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_BOUNDS_TEST",
          flag,sizeof(flag))==1 && flag[0]=='1'){
      InterlockedExchange(&ui_transition_bounds25_enabled,1);
      SpiderNativeSync24_Reset(&ui_transition_sync24_frame);
      SpiderNativeBounds25_Reset(&ui_transition_bounds25_frame);
      log_line("UI-TRANSITION-BOUNDS25: V25 ON - original native V19 iris, same-frame physical left/right curtains; legacy V6–V24 pipelines removed");
    }else{
      log_line("UI-TRANSITION-BOUNDS25: OFF - original native iris unchanged");
    }
  }
  /* Separate compositor opt-in: the regular UI canvas trial remains
   * testable without changing the render order. No INI schema changes. */
  {
    char opt[4]={0};
    DWORD n=GetEnvironmentVariableA("SPIDERMAN2K1FIX_UI_COMPOSITOR_TEST",
                                   opt,sizeof(opt));
    if(n==1 && opt[0]=='1' &&
       InterlockedCompareExchange(&ui_canvas_trial_enabled,0,0)==1) {
      InterlockedExchange(&ui_compositor_trial_enabled,1);
      /* Native Pause art remains exactly the original source material.
       * Only the independently validated physical outer gutters are
       * added at EndScene. No synthetic colour/tint passes. */
      log_line("UI-COMPOSITOR: TRIAL ON (native ESC deferred to EndScene; original 4:3 decor; source-backed exterior gutters)");
      /* Independent, reversible A/B opt-out for two physical outer
       * gutters. Explicit 0 replays only the original 4:3 decoration. */
      {
        char backdrop[4]={0};
        if(GetEnvironmentVariableA("SPIDERMAN2K1FIX_UI_COMPOSITOR_NATIVE_BACKDROP",
                                   backdrop,sizeof(backdrop))==1 &&
           backdrop[0]=='0')
          InterlockedExchange(&ui_compositor_native_backdrop_enabled,0);
        log_line(InterlockedCompareExchange(&ui_compositor_native_backdrop_enabled,0,0)==1 ?
          "UI-COMPOSITOR-NATIVE-BACKDROP: source-verified OUTER GUTTERS enabled (all 160 source 4:3 triangles preserved)" :
          "UI-COMPOSITOR-NATIVE-BACKDROP: OFF (original 4:3 source mesh only)");
      }
      /* Unvalidated HUD deferral remains opt-IN and is not necessary
       * for the hardware-validated native 4:3 Pause composition. */
      {
        char hud_over_veil[4]={0};
        if(GetEnvironmentVariableA("SPIDERMAN2K1FIX_UI_COMPOSITOR_HUD_OVER_VEIL",
                                   hud_over_veil,sizeof(hud_over_veil))==1 &&
           hud_over_veil[0]=='1')
          InterlockedExchange(&ui_compositor_hud_over_veil_enabled,1);
        log_line(InterlockedCompareExchange(&ui_compositor_hud_over_veil_enabled,0,0)==1 ?
          "UI-COMPOSITOR-HUD-ORDER: ON (experimental opt-in, defer recognized HUD to after exterior gutters)" :
          "UI-COMPOSITOR-HUD-ORDER: OFF (validated native HUD path, default)");
      }
    } else {
      log_line("UI-COMPOSITOR: OFF; existing UI canvas draw path unchanged");
    }
  }
  /* HUDWidescreenFix is the parent of the three native ESC children.
   * Read hidden [HUD] child opt-outs only AFTER the verified HUD
   * installation, so master=0 or failed HUD install disables all. */
  native_pause43_init();
  native_cursor43_init();
  native_motif43_init();
  /* Always-on virtual CFG: signature-gated EXE IAT hook.
   * The hook never executes for an unverified EXE. */
  {
    char ini_path[MAX_PATH];
    if(spider_path_ini_a(ini_path,sizeof(ini_path))) {
      /* Virtual Spidey.cfg is a permanent feature for the verified EXE.
       * Keep all signature checks and safe fallback inside SpiderCfg_Install. */
      SpiderCfg_Install(ini_path,log_line);
    }
  }
  /* The EXE checksum and the exact four menu instructions were checked.
   * Only the displayed values and two menu input paths are changed. */
  {
    char ini_path[MAX_PATH];
    if(spider_path_ini_a(ini_path,sizeof(ini_path))) {
      SpiderDisplayMenu_Install(display_resolution_setting,display_aspect_setting,log_line);
    }
  }
  install_ddraw_hook();
  /* Always pause on confirmed focus loss; legacy INI keys are ignored. */
  InterlockedExchange(&pause_on_focus_loss,1);
  log_line("FOCUS-PAUSE: permanent Flip-boundary pause ON; Any-key press/release resumes");
  SpiderInputFocus_Install(log_line);
  InterlockedExchange(&core_start_success,1);
  return 0;
}
static BOOL CALLBACK core_initialize_once(PINIT_ONCE once,PVOID arg,PVOID *context) {
  (void)once;(void)arg;(void)context;
  initialize_body(NULL);
  return TRUE; /* core_start_success additionally checks the EXE verification. */
}
static DWORD WINAPI initialize_worker(LPVOID unused) {
  (void)unused;
  InitOnceExecuteOnce(&core_start_once,core_initialize_once,NULL,NULL);
  return 0;
}

/* Called exclusively by the signature-guarded winmm.dll early IAT gate,
 * on the GAME's own first DirectDrawCreateEx call, outside the loader lock.
 * A failed/unsupported game cannot proceed through an unsafe physical
 * 640x480 mode: fail this DirectDraw request instead of forwarding it. */
__declspec(dllexport) HRESULT __cdecl SpiderMan2k1Fix_DirectDrawGate(
    GUID *guid,void **out,REFIID iid,IUnknown *outer,void *real_create) {
  if(!real_create || !InitOnceExecuteOnce(&core_start_once,
       core_initialize_once,NULL,NULL) ||
     InterlockedCompareExchange(&core_start_success,0,0)!=1) {
    log_line("DDRAW-STARTUP: ERROR patch initialization not ready; refusing unprotected DirectDraw startup");
    return DDERR_UNSUPPORTED;
  }
  original_create=(CreateExFn)real_create;
  log_line("DDRAW-STARTUP: initial hook and configuration ready before first DirectDrawCreateEx");
  return hooked_create(guid,out,iid,outer);
}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
  (void)reserved;
  if(reason==DLL_PROCESS_ATTACH) {
    HANDLE t; DisableThreadLibraryCalls(instance);
    t=CreateThread(NULL,0,initialize_worker,NULL,0,NULL);
    if(t) CloseHandle(t);
  }
  return TRUE;
}
