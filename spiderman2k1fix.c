#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <ddraw.h>
#include <d3d.h>
#include "hud_widescreen.h"
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
#include "menu_version.h"
#include "version_info.h"

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
/* DirectDraw7 borderless-windowed adaptation. Never alter physical display modes. */
typedef HRESULT (WINAPI *CreateExFn)(GUID *, void **, REFIID, IUnknown *);
typedef HRESULT (WINAPI *CoopFn)(void *, HWND, DWORD);
typedef HRESULT (WINAPI *ModeFn)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef HRESULT (WINAPI *SurfaceCreateFn)(void *, DDSURFACEDESC2 *, IDirectDrawSurface7 **, IUnknown *);
typedef HRESULT (WINAPI *SurfaceBltFn)(void *, LPRECT, IDirectDrawSurface7 *, LPRECT, DWORD, LPDDBLTFX);
typedef HRESULT (WINAPI *SurfaceFlipFn)(void *, IDirectDrawSurface7 *, DWORD);
typedef HRESULT (WINAPI *SurfaceGetAttachedFn)(void *, LPDDSCAPS2, IDirectDrawSurface7 **);
static CreateExFn original_create;
static CoopFn original_coop;
static ModeFn original_mode;
static SurfaceCreateFn original_surface_create;
static SurfaceBltFn original_blt;
static SurfaceFlipFn original_flip;
static SurfaceGetAttachedFn original_get_attached;
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
static LONG borderless_monitor_x=0,borderless_monitor_y=0;
static volatile LONG borderless_present_failed=0;
static volatile LONG borderless_legacy_mode_calls=0;
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
  /* Cache ONCE, before the game's first exclusive mode switch. Re-querying
   * later would incorrectly cache the current 640x480 game resolution.
   */
  if (!desktop_w && !desktop_h) {
    unsigned selected_w=0,selected_h=0;
    SpiderMenuMapping mapping;
    /* MENU always uses the captured desktop mode; gameplay may use an
     * independent physical output, while its logical canvas is 4:3. */
    if(SpiderMenu_MakeMapping(640,480,(int)mode.dmPelsWidth,
                              (int)mode.dmPelsHeight,&mapping)) {
      desktop_w=mapping.output_w;
      desktop_h=mapping.output_h;
      menu_map=mapping;
    }
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
  if(!SpiderComicNative_Transform((const SpiderMenuVertex*)source,copy,
                (unsigned)count,(unsigned)desktop_w,(unsigned)desktop_h)) return source;
  hit=InterlockedIncrement(&comic_native_aspect_matches);
  if(hit<=8 || hit%120==0) {
    char msg[286];
    snprintf(msg,sizeof(msg),
      "COMIC-NATIVE-FIX: #%ld stage=NATIVE rva=0x%08lX texture=%lux%lu "
      "x-before=(%.1f,%.1f) x-after=(%.1f,%.1f) y-unchanged [EXPERIMENT]",
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
  if(device_ready==2 && vt[13]==(void*)original_set_viewport)
    *obj=patched_device_vtable;
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
  if(debug_log_enabled && count<=24) {
    char line[100]; snprintf(line,sizeof(line),"DDRAW: Surface7::Blt result=0x%08lX",(unsigned long)hr); log_line(line);
  }
  return hr;
}
/* The surface-creation hook remains responsible for installing the shared vtable. */
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
static HRESULT WINAPI hooked_flip(void *self, IDirectDrawSurface7 *target, DWORD flags) {
  HRESULT hr; LONG count=InterlockedIncrement(&flip_count);
  if(debug_log_enabled && count<=24) {
    char line[160]; snprintf(line,sizeof(line),"DDRAW: Surface7::Flip #%ld self=%p target=%p flags=0x%08lX",
                             (long)count,self,(void*)target,(unsigned long)flags); log_line(line);
  }
  SpiderInputFocus_OnFlip(count);
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
  if(self==(void*)borderless_primary && borderless_back && original_blt) {
      RECT rc={borderless_monitor_x,borderless_monitor_y,
        borderless_monitor_x+(LONG)desktop_w,borderless_monitor_y+(LONG)desktop_h};
      hr=original_blt(self,&rc,borderless_back,NULL,DDBLT_WAIT,NULL);
      if(FAILED(hr)) {
        LONG failures=InterlockedIncrement(&borderless_present_failed);
        if(failures<=12) {
          char line[180];
          snprintf(line,sizeof(line),"BORDERLESS: primary Blt FAILED hr=0x%08lX (no exclusive fallback)",(unsigned long)hr);
          log_line(line);
        }
      }
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
    if(borderless_back) {
      IDirectDrawSurface7_Release(borderless_back);
      borderless_back=NULL;
    }
    borderless_primary=NULL;
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
        log_line("BORDERLESS: MENU exited; windowed gameplay presentation active");
    }
  }
  return hr;
}
static HRESULT WINAPI hooked_create(GUID *guid,void **out,REFIID iid,IUnknown *outer) {
  /* Before first video device initialization, on the game main thread. */
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
  char exe[MAX_PATH], ini[MAX_PATH], *slash;
  DWORD n;
  int option;
  n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if (!n || n>=sizeof(exe)) {
    log_line("HUD: cannot determine INI path; correction disabled");
    return;
  }
  memcpy(ini,exe,n+1);
  slash=strrchr(ini,'\\');
  if (!slash || (size_t)(slash-ini)+strlen("SpiderMan2k1Fix.ini")+1>=sizeof(ini)) {
    log_line("HUD: invalid INI path; correction disabled");
    return;
  }
  strcpy(slash+1,"SpiderMan2k1Fix.ini");
  option=GetPrivateProfileIntA("HUD","HUDWidescreenFix",0,ini);
  if(lstrcmpiA(display_aspect_setting,"4:3")==0 && option==1) {
    log_line("HUD: disabled automatically for original 4:3 mode");
    option=0;
  }
  if (option!=1) {
    log_line("HUD: widescreen transformation OFF (HUDWidescreenFix=0)");
    if(lstrcmpiA(display_aspect_setting,"4:3")!=0 &&
       GetPrivateProfileIntA("Display","ExperimentalFOVHorPlus",0,ini)==1) {
      if(SpiderHudWidescreen_InstallScopesOnly(exe_verified)) {
        InterlockedExchange(&fov_horplus_scopes_ready,1);
        log_line("FOV-HOR+: passive HUD text/bar/rect scope tracking INSTALLED (no HUD transforms)");
      } else {
        log_line("FOV-HOR+: ERROR passive HUD scope signatures/install failed");
        SpiderFOV_DisableUnsafeHorPlus();
      }
    }
    return;
  }
  if (!SpiderHudWidescreen_Install(exe_verified,TRUE)) {
    log_line("HUD: ERROR: native 16-site install failed, HUD correction NOT enabled");
    SpiderFOV_DisableUnsafeHorPlus();
    return;
  }
  InterlockedExchange(&fov_horplus_scopes_ready,1);
  log_line("HUD: EXPERIMENTAL native 16-site widescreen correction installed (unverified in-game)");
}
/* A one-shot watchdog diagnoses the crucial "60 FPS despite 30 limit"
 * case even if Flip is never intercepted. Runs in the existing worker thread
 * after hook setup, outside the loader lock. Does not change game timing.
 */
static DWORD WINAPI initialize(LPVOID unused) {
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  char line[160]; (void)unused;
  debug_log_load_setting();
  log_line("START: SpiderMan2k1Fix 1.0.0");
  if (!supported_exe()) {
    log_line("ERROR: unsupported EXE SHA-256; do not use this proxy with this executable");
    return 0;
  }
  log_line("OK: supported SpideyPC.exe SHA-256 verified");
  /* Generate complete defaults once, before any GetPrivateProfile... reads. */
  SpiderCfg_EnsureDefaultIni(log_line);
  /* Repair invalid/unknown INI entries before reading any game settings. */
  SpiderCfg_ValidateCurrentIni(log_line);
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
        char resolution[40],aspect[40],fov[40];
        int fov_exp=GetPrivateProfileIntA("Display","ExperimentalFOVAspect",0,ini);
        GetPrivateProfileStringA("Display","FOV","Auto",fov,sizeof(fov),ini);
        /* Legacy FOV knobs are kept until the later cleanup. Forced 4:3
         * restores the original game camera without Y compensation. */
        GetPrivateProfileStringA("Display","AspectRatio","Auto",aspect,sizeof(aspect),ini);
        if(lstrcmpiA(aspect,"4:3")==0) {
          DEVMODEA current;
          unsigned chosen_w=0,chosen_h=0;
          GetPrivateProfileStringA("Display","Resolution","Auto",resolution,sizeof(resolution),ini);
          if(lstrcmpiA(resolution,"Auto")!=0) {
            log_line("BORDERLESS: Resolution override ignored: full-window output always uses the current monitor resolution");
            strcpy(resolution,"Auto");
          }
          memset(&current,0,sizeof(current));current.dmSize=sizeof(current);
          if(!EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&current) ||
             !SpiderDisplayPolicy_ResolveRuntime(resolution,aspect,
                  current.dmPelsWidth,current.dmPelsHeight,&chosen_w,&chosen_h) ||
             (unsigned long long)chosen_w*3ull!=(unsigned long long)chosen_h*4ull) {
            log_line("DISPLAY-ASPECT-43: WARNING invalid logical canvas; falling back to validated Auto widescreen");
            strcpy(aspect,"Auto");
          } else {
            snprintf(line,sizeof(line),"DISPLAY-ASPECT-43: logical 4:3 canvas=%ux%u (physical native output remains independent)",chosen_w,chosen_h);
            log_line(line);
          }
        }
        if(lstrcmpiA(aspect,"4:3")==0) {
          /* The automatic aspect FOV remains active; for a logical 4:3
           * canvas its scalar is exactly 1.0, the original game baseline. */
          SpiderFOV_Init("Auto",1,0,log_line);
          log_line("DISPLAY-ASPECT-43: automatic FOV uses logical 4:3 aspect (original game baseline 1.0)");
        } else {
          if(lstrcmpiA(aspect,"Auto")!=0)
            log_line("DISPLAY-ASPECT-43: invalid AspectRatio; falling back to Auto (valid: Auto, 4:3)");
          SpiderFOV_Init(fov,fov_exp,GetPrivateProfileIntA("Display","ExperimentalFOVHorPlus",0,ini),log_line);
        }
        int menu=GetPrivateProfileIntA("Display","ExperimentalMenu4x3",0,ini);
        int media=GetPrivateProfileIntA("Display","ExperimentalMediaAspect",0,ini);
        int bltfix=GetPrivateProfileIntA("Display","ExperimentalMediaBlt4x3",0,ini);
        int knownsizes=GetPrivateProfileIntA("Display","ExperimentalMediaBltKnownSizes",0,ini);
        int video_aspect=GetPrivateProfileIntA("Display","ExperimentalIntroSourceAspect",0,ini);
        int anysize=GetPrivateProfileIntA("Display","ExperimentalMediaBltAnySize",0,ini);
        int comicnative=GetPrivateProfileIntA("Display","ExperimentalComicNativeAspect",0,ini);
        if(comicnative==1 && lstrcmpiA(aspect,"4:3")!=0)
          InterlockedExchange(&comic_native_aspect_enabled,1);
        GetPrivateProfileStringA("Display","Resolution","Auto",resolution,sizeof(resolution),ini);
        if(lstrcmpiA(resolution,"Auto")!=0) {
          log_line("BORDERLESS: Resolution override ignored: full-window output always uses the current monitor resolution");
          strcpy(resolution,"Auto");
        }
        strncpy(display_resolution_setting,resolution,sizeof(display_resolution_setting)-1);
        display_resolution_setting[sizeof(display_resolution_setting)-1]=0;
        if(lstrcmpiA(aspect,"4:3")!=0) strcpy(aspect,"Auto");
        strcpy(display_aspect_setting,aspect);
        /* Valid explicit resolutions MUST NOT disable previously validated
         * 4:3/menu/video corrections; their physical output is computed by
         * probe_desktop_mode rather than gating this entire feature off. */
        if ((lstrcmpiA(aspect,"Auto")==0 ||
             lstrcmpiA(aspect,"4:3")==0)) {
          if(menu!=1) log_line("BORDERLESS: menu 4:3 forced ON; ExperimentalMenu4x3 ignored");
          InterlockedExchange(&menu_upscale_enabled,1);
          /* Opening movies are always 4:3 on the monitor-size window.
           * Old experimental switches remain readable for compatibility,
           * but cannot disable or select 5:3 for these four intro movies. */
          if(media!=1 || bltfix!=1 || knownsizes!=1 || video_aspect!=0)
            log_line("BORDERLESS-INTRO: legacy media options overridden; all four opening movies forced to 4:3");
          InterlockedExchange(&media_viewport_4x3_enabled,1);
          InterlockedExchange(&media_native_blt_4x3_enabled,1);
          InterlockedExchange(&media_native_blt_knownsizes_enabled,1);
          /* AnySize is still opt-in and is never necessary for the intros. */
          if (anysize==1) InterlockedExchange(&media_native_blt_anysize_enabled,1);
        }
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
  install_player_hud(TRUE);
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
      if(GetPrivateProfileIntA("Display","ExperimentalDisplayMenuLock",1,ini_path)==1)
        SpiderDisplayMenu_Install(display_resolution_setting,display_aspect_setting,log_line);
      else log_line("DISPLAY-MENU-LOCK: explicitly disabled by INI");
    }
  }
  install_ddraw_hook();
  SpiderInputFocus_Install(log_line);
  return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
  (void)reserved;
  if(reason==DLL_PROCESS_ATTACH) {
    HANDLE t; DisableThreadLibraryCalls(instance);
    t=CreateThread(NULL,0,initialize,NULL,0,NULL);
    if(t) CloseHandle(t);
  }
  return TRUE;
}
