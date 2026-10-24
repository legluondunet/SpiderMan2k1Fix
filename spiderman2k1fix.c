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
#include "input_notacquired_retry.h"
#include "pause_any_key.h"
#include "pause_surface.h"
#include "focus_media_pause.h"
#include "focus_dsound_pause.h"
#include "menu_version.h"
#include "version_info.h"
#include "f9_png.h"
#include "f9_toast.h"
#include "quick_exit_f12.h"

static const BYTE expected_sha256[32] = {
  0x0a,0x11,0xa4,0x9f,0x3f,0x63,0xd4,0xbb,
  0x15,0x65,0x0f,0x32,0x2d,0x65,0x45,0x53,
  0xfc,0x47,0x23,0x4b,0x01,0x88,0x5b,0x85,
  0x2a,0x05,0x8f,0xf0,0x74,0x68,0x9d,0xea
};
static int debug_log_enabled=0;
/* Load once at startup, not on every drawing/logging call. */
static void debug_log_load_setting(void) {
  char path[MAX_PATH],*slash;
  DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
  debug_log_enabled=0;
  if (!n || n>=sizeof(path)) return;
  slash=strrchr(path,'\\');
  if (!slash || (size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1>=sizeof(path)) return;
  strcpy(slash+1,"SpiderMan2k1Fix.ini");
  debug_log_enabled=(GetPrivateProfileIntA("Debug","Log",0,path)==1);
}
static void log_line(const char *s) {
  char path[MAX_PATH], *slash; FILE *f;
  if (!debug_log_enabled) return;
  DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
  if (!n || n>=sizeof(path)) return;
  slash=strrchr(path,'\\');
  if (!slash || (size_t)(slash-path)+strlen("SpiderMan2k1Fix.log")+1>=sizeof(path)) return;
  strcpy(slash+1,"SpiderMan2k1Fix.log");
  f=fopen(path,"a"); if(f) { fprintf(f,"%s\n",s); fclose(f); }
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

/* Native gameplay ESC 4:3 panel correction (HUD-controlled).
 * Evidence: 2026-10-24 Wine traces #737 and #741, two menu states.
 * Anchor on the unusually wide *untextured* purple ribbon's verified
 * screen rectangle plus Z/RHW. Only following 2D glyphs in that SAME
 * Flip are eligible, with their own strict depth and texture gates.
 * Never touch world/HUD vertices or alter any game-owned input/state.
 */
static volatile LONG native_pause43_enabled=0;
static volatile LONG native_pause43_banner_flip=-1;
static volatile LONG native_pause43_banner_hits=0;
static volatile LONG native_pause43_border_hits=0;
static volatile LONG native_pause43_banner_top=0;
static volatile LONG native_pause43_banner_bottom=0;
static volatile LONG native_pause43_glyph_hits=0;
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
  ribbon=(left>=-30.0f && left<=20.0f &&
       right>=(float)desktop_w-25.0f &&
       right<=(float)desktop_w+35.0f &&
       top>=(float)desktop_h*0.30f &&
       top<=(float)desktop_h*0.52f &&
       bottom>=(float)desktop_h*0.55f &&
       bottom<(float)desktop_h*0.72f &&
       (bottom-top)>(float)desktop_h*0.11f &&
       fabsf(z-0.00122f)<0.00010f &&
       fabsf(rhw-11.176f)<0.25f);
  if(ribbon) {
    /* The banner has no texture. Recheck the live bound texture here
     * rather than relying solely on depth and broad geometry.
     */
    if(SUCCEEDED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex)) &&
       tex) { IDirectDrawSurface7_Release(tex);return source; }
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
    if(!((fabsf(z-0.00098f)<0.00010f && fabsf(rhw-12.686f)<0.30f) ||
         (fabsf(z-0.00122f)<0.00010f && fabsf(rhw-11.176f)<0.30f)))
      return source;
    if(FAILED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex)) ||
       !tex)return source;
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    if(SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(tex,&desc))) {
      tw=desc.dwWidth;th=desc.dwHeight;
    }
    IDirectDrawSurface7_Release(tex);
    if(tw==8 && th==8 && bb>bt+55 &&
       fabsf(z-0.00122f)<0.00010f &&
       fabsf(rhw-11.176f)<0.25f) {
      int near_left=(left>=-42.0f && left<=-12.0f &&
                     right>=7.0f && right<=38.0f);
      int near_right=(left>=(float)desktop_w-17.0f &&
                      left<=(float)desktop_w+13.0f &&
                      right>=(float)desktop_w+16.0f &&
                      right<=(float)desktop_w+70.0f);
      int at_top=fabsf(top-((float)bt-4.5f))<5.0f;
      int at_bottom=fabsf(top-((float)bb-9.0f))<5.0f;
      int horizontal=(fabsf(left)<12.0f &&
          fabsf(right-(float)desktop_w)<20.0f &&
          bottom-top>=14.0f && bottom-top<=24.0f &&
          (at_top || fabsf(top-(float)bb)<5.0f));
      int side=(near_left || near_right) &&
          fabsf((bottom-top)-((float)bb-(float)bt-9.0f))<18.0f &&
          fabsf(top-((float)bt+4.5f))<8.0f;
      int corner=(near_left || near_right) &&
          bottom-top>=18.0f && bottom-top<=30.0f &&
          (at_top || at_bottom);
      border=horizontal || side || corner;
    }
    if(!border) {
      /* Title: PAUSED (Y=171), REALLY QUIT? (Y=283).
       * Choices: main menu Y=436/508/580; confirmation Y=468/540.
       * Exclude instructions (Y=877), compass, gameplay HUD, and
       * the separately handled red spider selector. */
      if(left<=(float)desktop_w*0.25f ||
         right>=(float)desktop_w*0.75f ||
         top<(float)desktop_h*0.13f ||
         bottom>(float)desktop_h*0.63f ||
         right-left<10.0f || right-left>84.0f ||
         bottom-top<24.0f || bottom-top>83.0f)
        return source;
      glyph=(th==32 && (tw==16 || tw==32 || tw==64));
      if(!glyph)return source;
    }
  }
  center=(float)desktop_w*0.5f;
  aspect_scale=((float)desktop_h*(4.0f/3.0f))/(float)desktop_w;
  memcpy(copy,v,4*sizeof(*copy));
  for(i=0;i<4;i++) {
    float mapped=center+(v[i].x-center)*aspect_scale;
    /* Reference F9: original 4:3 outline x=241..1679 (1439px).
     * The current 1920x1080 Auto outline is x=224..1702 (1479px),
     * but the untextured fill is already much closer to 4:3. Do NOT
     * indiscriminately shrink text or the entire HUD by 2.7%.
     * Empirical calibration is intentionally exclusive to the one
     * measured resolution until another mode is validated in-game.
     */
    if(desktop_w==1920 && desktop_h==1080) {
      if(border)
        mapped=960.0f+(mapped-963.0f)*(1439.0f/1479.0f);
      else if(ribbon)
        mapped=960.0f+(mapped-958.6f)*(1439.0f/1448.5f);
    }
    copy[i].x=mapped;
  }
  if(ribbon) hit=InterlockedIncrement(&native_pause43_banner_hits);
  else if(border) hit=InterlockedIncrement(&native_pause43_border_hits);
  else hit=InterlockedIncrement(&native_pause43_glyph_hits);
  if(hit<=4 || (hit%600)==0) {
    char line[230];
    snprintf(line,sizeof(line),
      "PAUSE-NATIVE-43: %s hit=%ld flip=%ld x=(%.1f..%.1f) scale=%.5f tex=%lux%lu [NATIVE-ESC-43]",
      ribbon?"ribbon":border?"border":"glyph",(long)hit,(long)flip,
      (double)left,(double)right,(double)aspect_scale,
      (unsigned long)tw,(unsigned long)th);
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
 * HUD-controlled by default; Wine-validated at 1920x1080 ONLY.
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
     desktop_w!=1920 || desktop_h!=1080 ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0 ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4 ||
     !geom_rva(ret,&rva) || rva!=0x00102C72UL ||
     fabsf(v[0].z-0.00146f)>0.00004f ||
     fabsf(v[0].rhw-9.988f)>0.060f) return source;
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
  center=(float)desktop_w*0.5f;
  memcpy(copy,v,4*sizeof(*copy));
  for(i=0;i<4;i++)
    copy[i].x=center+(v[i].x-center)*0.75f;
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
  const float left_edge=240.0f,right_edge=1680.0f;
  float left,right;
  SpiderMenuVertex tri[3],out[5];
  unsigned i,n;
  LONG hit;
  int gutter,any=0;
  if(!original || !device)return;
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
        "NATIVE-MOTIF-43: GUTTER-REPLAY hit=%ld source-x=(%.1f..%.1f) clip=(0..240,1680..1920) [ORIGINAL COLOR/BLEND; CENTER UNTOUCHED]",
        (long)hit,(double)left,(double)right);
      log_line(line);
    }
  }
}
/* Native gameplay ESC red selector: HUD-controlled, Wine-validated 1920x1080.
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
    "NATIVE-CURSOR-43: native red selector 4:3 ON (HUD master; 1920x1080 Auto)" :
    "NATIVE-CURSOR-43: OFF (HUD master, NativeESC4x3 or NativeCursor4x3=0)");
}
static const void *maybe_correct_native_cursor43(void *device,
    D3DPRIMITIVETYPE type,DWORD fvf,const void *source,DWORD count,
    void *ret,SpiderMenuVertex *copy) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  float left,right,top,bottom,cx,z,rhw;
  DWORD rva=0,diffuse=0;
  IDirectDrawSurface7 *tex=NULL;
  DDSURFACEDESC2 desc;
  LONG flip,hits;
  unsigned i;
  HRESULT hr;
  if(InterlockedCompareExchange(&native_cursor43_enabled,0,0)!=1 ||
     !source || !copy || !device ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4 ||
     desktop_w!=1920 || desktop_h!=1080 ||
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
  /* The selector can legitimately move anywhere on the 1920x1080
   * desktop. Former position gates (left>=450, right<=1480,
   * top>=325, bottom<=800) made the 96x72 sprite revert to its
   * 16:9-stretched aspect near the top and right: observed F9
   * scrn0011=72px, scrn0012 (y<325)=96px,
   * scrn0013 (right>1480)=96px. Do NOT constrain its screen position.
   * Match its stable local 96x72 size, near-foreground Z/RHW,
   * original 32x32 texture format, white diffuse, exact native
   * draw call and active ESC ribbon in this same frame instead.
   * Native font glyphs are different sizes/Z/RHW. */
  if(fabsf((right-left)-96.0f)>0.6f ||
     fabsf((bottom-top)-72.0f)>0.6f ||
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
  memcpy(copy,v,4*sizeof(*copy));
  for(i=0;i<4;i++)
    copy[i].x=cx+0.75f*(v[i].x-cx);
  hits=InterlockedIncrement(&native_cursor43_hits);
  if(hits<=8 || hits%600==0) {
    char line[230];
    snprintf(line,sizeof(line),
      "NATIVE-CURSOR-43: QUAD-MATCH hit=%ld flip=%ld source=(%.0f..%.0f,%.0f..%.0f) output-width=72.0 z=%.5f rhw=%.3f [X-ONLY]",
      (long)hits,(long)flip,(double)left,(double)right,
      (double)top,(double)bottom,(double)z,(double)rhw);
    log_line(line);
  }
  return copy;
}
static void free_menu_vertices(SpiderMenuVertex *allocated) {
  if (allocated) HeapFree(GetProcessHeap(),0,allocated);
}

static HRESULT WINAPI hooked_draw_primitive(void *self,D3DPRIMITIVETYPE type,DWORD fvf,LPVOID verts,DWORD count,DWORD flags) {
  if(InterlockedCompareExchange(&menu_upscale_active,0,0)==1 &&
     fvf==0x144UL && verts && count>=3 && count<=128)
    InterlockedIncrement(&menu_version_draws);
  {
    SpiderMenuVertex small[64],*allocated=NULL;
    const void *v=scale_menu_vertices(fvf,verts,count,small,64,&allocated);
    if(v==verts && count==4) {
      v=maybe_correct_native_comic(self,type,fvf,verts,count,
                                   __builtin_return_address(0),small);
    }
    /* BD has precedence and a separate signature; never rescale it. */
    if(v==verts && count==4)
      v=maybe_correct_adaptive_hud(self,type,fvf,verts,count,
                                   __builtin_return_address(0),small);
    /* Correct the native translucent motif. Where these same alpha
     * primitives previously tinted the entire 16:9 frame, replay the
     * original triangles ONLY into the two outer 240px gutters first.
     * Central motif and original banner/cursor stay unchanged. */
    if(v==verts && count==4) {
      v=maybe_correct_native_motif43(self,type,fvf,verts,count,
                                    __builtin_return_address(0),small);
      if(v!=verts)
        native_motif43_restore_gutters(self,(const SpiderMenuVertex*)verts,flags);
    }
    /* Native red selector uses exact texture and quad signature.
     * The older Wine stack-owner hypothesis has been retired. */
    if(v==verts && count==4)
      v=maybe_correct_native_cursor43(self,type,fvf,verts,count,
                                      __builtin_return_address(0),small);
    /* Native ESC after known comic/HUD precedence. */
    if(v==verts && count==4)
      v=maybe_correct_native_pause43(self,type,fvf,verts,count,
                                     __builtin_return_address(0),small);
    if(v==verts) v=maybe_correct_horplus(type,fvf,verts,count,small);
    HRESULT hr=original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags);
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
     vt[26]==(void*)original_draw_indexed_primitive) {
    DWORD protection=0,unused=0;
    if(VirtualProtect(vt,sizeof(patched_device_vtable),
                      PAGE_READWRITE,&protection)) {
      vt[13]=(void*)hooked_set_viewport;
      vt[20]=(void*)hooked_set_render_state;
      vt[25]=(void*)hooked_draw_primitive;
      vt[26]=(void*)hooked_draw_indexed_primitive;
      if(!VirtualProtect(vt,sizeof(patched_device_vtable),protection,&unused))
        log_line("D3D7: WARNING native vtable protection restore failed");
      log_line("D3D7: native device vtable patched in place (default; all four hooks ON)");
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
            vt[26]==(void*)hooked_draw_indexed_primitive) {
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
static HRESULT WINAPI hooked_blt(void *self, LPRECT dest, IDirectDrawSurface7 *source,
                                  LPRECT source_rect, DWORD flags, LPDDBLTFX fx) {
  HRESULT hr; LONG count=InterlockedIncrement(&blt_count);
  RECT dest_mapped;
  LPRECT presented_dest=dest;
  BOOL f9_movie_present=FALSE;

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
/* Ignore brief foreground changes (e.g. mouse activation quirks).
 * Real Alt+Tab is recognized only if another process stays foreground. */
static BOOL pause_confirmed_focus_loss(HWND hwnd) {
  HWND fg=GetForegroundWindow();
  DWORD game_pid=0,fg_pid=0,now;
  if(fg==hwnd) { pause_background_since=0; return FALSE; }
  GetWindowThreadProcessId(hwnd,&game_pid);
  if(fg) GetWindowThreadProcessId(fg,&fg_pid);
  /* A transient NULL foreground or another window owned by the game
   * is not proof that Spider-Man was switched to the background. */
  if(!fg || !game_pid || fg_pid==game_pid) {
    pause_background_since=0;
    return FALSE;
  }
  now=GetTickCount();
  if(!pause_background_since) {
    pause_background_since=now?now:1;
    return FALSE;
  }
  return (DWORD)(now-pause_background_since)>=150;
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
static void pause_at_flip_if_background(void) {
  HWND hwnd=game_hwnd;
  MSG msg;
  RECT pause_present_rect,pause_content_rect;
  if(InterlockedCompareExchange(&pause_on_focus_loss,0,0)!=1 || !hwnd || !IsWindow(hwnd)) return;
  if(GetForegroundWindow()==hwnd) {
    pause_background_since=0;
    InterlockedExchange(&pause_ever_foreground,1);
  } else if(InterlockedCompareExchange(&pause_ever_foreground,0,0)==1 &&
            pause_confirmed_focus_loss(hwnd) &&
            InterlockedCompareExchange(&pause_latched,1,0)==0) {
    log_line("FOCUS-PAUSE: confirmed external focus loss; pause latched until manual resume");
    pause_log_foreground_owner(hwnd,GetForegroundWindow(),"external-foreground-latched");
  }
  if(InterlockedCompareExchange(&pause_latched,0,0)!=1) return;
  if(InterlockedCompareExchange(&pause_in_progress,1,0)!=0) return;
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
                             (SpiderPauseBltFn)original_blt);
  while(IsWindow(hwnd) && InterlockedCompareExchange(&pause_latched,0,0)==1) {
    while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) {
      if(msg.message==WM_QUIT) {
        PostQuitMessage((int)msg.wParam);
        SpiderPauseAny_End();
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
    if(SpiderPauseAny_Completed())
      InterlockedExchange(&pause_latched,0);
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
  SpiderPauseSurface_End();
  SpiderFocusDSound_Resume();
  SpiderFocusMedia_Resume();
  log_line("FOCUS-PAUSE: explicit resume or window destroyed");
  pause_log_foreground_owner(hwnd,GetForegroundWindow(),"resume-foreground-state");
  pause_log_resume_focus(hwnd);
  InterlockedExchange(&pause_in_progress,0);
}
static HRESULT WINAPI hooked_flip(void *self, IDirectDrawSurface7 *target, DWORD flags) {
  HRESULT hr; LONG count=InterlockedIncrement(&flip_count);
  SpiderQuickExitF12_ObserveEngineState(count,InterlockedCompareExchange(&menu_upscale_active,0,0)==1); /* passive native mode+2D renderer tracing */
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
    pause_at_flip_if_background();
    if(was_background &&
       InterlockedCompareExchange(&pause_on_focus_loss,0,0)==1)
      SpiderInputFocus_OnFlip(count);
  }
  SpiderDI8Retry_OnFlip(count);
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
  SpiderCfg_BootstrapGameRead();
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
  install_null_fclose_guard(); /* Default ON only for SHA-verified EXE and exact native opcode signature. */
  /* Generate complete defaults once, before any GetPrivateProfile... reads. */
  SpiderCfg_EnsureDefaultIni(log_line);
  /* Repair invalid/unknown INI entries before reading any game settings. */
  SpiderCfg_ValidateCurrentIni(log_line);
  SpiderQuickExitF12_Init(log_line); /* Optional [System] QuickExitF12=1 only. */
  pause_apply_windows_ghosting_option(); /* Opt-in fix experiment for spurious Windows Ghost focus. */
  /* Native DIERR_NOTACQUIRED retry remains opt-in; read the validated INI.
   * No COM-vtable interception. The actual patch is delayed to Flip #120. */
  SpiderDI8Retry_Init(log_line);
  SpiderFocusMedia_Init(log_line); /* Opt-in native Bink pause. */
  SpiderFocusDSound_Init(log_line); /* Separately opt-in game-owned secondary buffers only. */
  SpiderPauseSurface_Init(log_line); /* Pause drawn into the game backbuffer; no HWND overlays. */
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
    char path[MAX_PATH],ini[MAX_PATH],*slash;
    DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
    int fps=30;
    if (n && n<sizeof(path) && (slash=strrchr(path,'\\'))!=NULL &&
        (size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini")+1<sizeof(path)) {
      memcpy(ini,path,n+1);
      strcpy(ini+(slash-path)+1,"SpiderMan2k1Fix.ini");
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
  /* HUDWidescreenFix is the parent of the three native ESC children.
   * Read hidden [HUD] child opt-outs only AFTER the verified HUD
   * installation, so master=0 or failed HUD install disables all. */
  native_pause43_init();
  native_cursor43_init();
  native_motif43_init();
  /* Always-on virtual CFG: signature-gated EXE IAT hook.
   * The hook never executes for an unverified EXE. */
  {
    char game_path[MAX_PATH],ini_path[MAX_PATH],*slash;
    DWORD n=GetModuleFileNameA(NULL,game_path,sizeof(game_path));
    if(n>0 && n<sizeof(game_path) && (slash=strrchr(game_path,'\\'))!=NULL) {
      memcpy(ini_path,game_path,n+1);
      strcpy(ini_path+(slash-game_path)+1,"SpiderMan2k1Fix.ini");
      /* Virtual Spidey.cfg is a permanent feature for the verified EXE.
       * Keep all signature checks and safe fallback inside SpiderCfg_Install. */
      SpiderCfg_Install(ini_path,log_line);
    }
  }
  /* The EXE checksum and the exact four menu instructions were checked.
   * Only the displayed values and two menu input paths are changed. */
  {
    char game_path[MAX_PATH],ini_path[MAX_PATH],*slash;
    DWORD n=GetModuleFileNameA(NULL,game_path,sizeof(game_path));
    if(n>0 && n<sizeof(game_path) && (slash=strrchr(game_path,'\\'))!=NULL) {
      memcpy(ini_path,game_path,n+1);
      strcpy(ini_path+(slash-game_path)+1,"SpiderMan2k1Fix.ini");
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
