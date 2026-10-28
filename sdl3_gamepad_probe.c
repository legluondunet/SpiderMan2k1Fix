/* SDL3 experimental read-only reconnaissance for Spider-Man (PC 2001).
 * This does NOT replace the validated DirectInput backend. We use the DLL
 * ABI dynamically: SDL3.dll is optional and never copied from the system PATH.
 * Opt in via [Input] ExperimentalSDL3Probe=1 and [Debug] Log=1.
 *
 * No SDL window, audio, renderer, game mapping writes or synthetic keys.
 * SDL3's instance ID is the only controller identity used here. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_gamepad_probe.h"
#include "sdl3_gamepad_selection.h"
#include "sdl3_gamepad_slots.h"

/* SDL3 3.2+ ABI subset; keep signatures aligned with SDL's public headers. */
#define SPIDER_SDL_INIT_GAMEPAD 0x00002000u
#define SPIDER_SDL_BUTTON_START 6
#define SPIDER_SDL_AXIS_LEFT_TRIGGER 4
#define SPIDER_SDL_AXIS_RIGHT_TRIGGER 5
#define SPIDER_SDL_SCAN_MS 250u

typedef bool (__cdecl *SDL_InitSubSystemFn)(uint32_t);
typedef void (__cdecl *SDL_SetMainReadyFn)(void);
typedef void (__cdecl *SDL_PumpEventsFn)(void);
typedef void (__cdecl *SDL_UpdateGamepadsFn)(void);
typedef uint32_t *(__cdecl *SDL_GetGamepadsFn)(int *);
typedef uint32_t *(__cdecl *SDL_GetJoysticksFn)(int *);
typedef const char *(__cdecl *SDL_GetGamepadNameForIDFn)(uint32_t);
typedef uint16_t (__cdecl *SDL_GetGamepadVendorForIDFn)(uint32_t);
typedef uint16_t (__cdecl *SDL_GetGamepadProductForIDFn)(uint32_t);
typedef void *(__cdecl *SDL_OpenGamepadFn)(uint32_t);
typedef void (__cdecl *SDL_CloseGamepadFn)(void *);
typedef bool (__cdecl *SDL_GetGamepadButtonFn)(void *,int);
typedef short (__cdecl *SDL_GetGamepadAxisFn)(void *,int);
typedef const char *(__cdecl *SDL_GetErrorFn)(void);
typedef void (__cdecl *SDL_freeFn)(void *);
typedef void (__cdecl *SDL_QuitSubSystemFn)(uint32_t);

static SpiderSDL3Opened pads[SPIDER_SDL3_MAX_GAMEPADS];
static SpiderSDL3Selection selection={0};
static unsigned num_pads=0;
static int requested=0,attempted=0,ready=0,scanned=0;
static int native_owner_requested=0;
static DWORD last_scan=0,last_axis_log=0;
static int last_joystick_count=-1,last_mapped_count=-1;
static short old_lt=-1,old_rt=-1;
static HMODULE module=NULL;
static void (*report)(const char *)=NULL;
static SDL_InitSubSystemFn sdl_init=NULL;
static SDL_SetMainReadyFn sdl_main_ready=NULL;
static SDL_PumpEventsFn sdl_pump=NULL;
static SDL_UpdateGamepadsFn sdl_update=NULL;
static SDL_GetGamepadsFn sdl_gamepads=NULL;
static SDL_GetJoysticksFn sdl_joysticks=NULL;
static SDL_GetGamepadNameForIDFn sdl_name=NULL;
static SDL_GetGamepadVendorForIDFn sdl_vendor=NULL;
static SDL_GetGamepadProductForIDFn sdl_product=NULL;
static SDL_OpenGamepadFn sdl_open=NULL;
static SDL_CloseGamepadFn sdl_close=NULL;
static SDL_GetGamepadButtonFn sdl_button=NULL;
static SDL_GetGamepadAxisFn sdl_axis=NULL;
static SDL_GetErrorFn sdl_error=NULL;
static SDL_freeFn sdl_free=NULL;
static SDL_QuitSubSystemFn sdl_quit_subsystem=NULL;

static void say(const char *line) {if(report)report(line);}
void SpiderSDL3Probe_Init(const char *ini,void (*logger)(const char *)) {
  report=logger;
  requested=ini && GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1;
  native_owner_requested=requested &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3NativeOwner",0,ini)==1;
  if(native_owner_requested &&
     (GetPrivateProfileIntA("Input","ExperimentalNativeJoystickUnplugGuard",0,ini)!=1 ||
      GetPrivateProfileIntA("Input","ExperimentalNativeJoystickReconnect",0,ini)!=1 ||
      GetPrivateProfileIntA("Input","ExperimentalNativeJoystickMappingSwap",0,ini)!=1)) {
    native_owner_requested=0;
    say("SDL3-OWNER: REFUSED missing native UnplugGuard+Reconnect+MappingSwap opt-ins; SDL3 stays passive");
  }
  if(requested)say("SDL3-PROBE: requested (OFF by default); discovery and selection enabled");
  if(native_owner_requested)
    say("SDL3-OWNER: EXPERIMENT requested; selected SDL3 identity may authorize opt-in DirectInput native + UI bridge (STRICT, FAIL-CLOSED)");
}

int SpiderSDL3Probe_NativeOwnerEnabled(void) {
  return native_owner_requested && ready && scanned;
}
unsigned SpiderSDL3Probe_MappedCount(void) {
  return SpiderSDL3Probe_NativeOwnerEnabled()?selection.count:0;
}
int SpiderSDL3Probe_SelectedIdentity(uint16_t *vid,uint16_t *pid,
                                     uint32_t *instance_id) {
  uint16_t v,p;
  if(vid)*vid=0;
  if(pid)*pid=0;
  if(instance_id)*instance_id=0;
  if(!SpiderSDL3Probe_NativeOwnerEnabled() || !selection.active ||
     !sdl_vendor || !sdl_product)return 0;
  v=sdl_vendor(selection.active);p=sdl_product(selection.active);
  if(!v || !p)return 0; /* A synthetic/unknown product has no safe DI match. */
  if(vid)*vid=v;
  if(pid)*pid=p;
  if(instance_id)*instance_id=selection.active;
  return 1;
}

static int sdl_load(void) {
  char path[MAX_PATH],line[310],*slash;
  DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
  attempted=1;
  if(!n || n>=sizeof(path) || !(slash=strrchr(path,'\\')) ||
     (size_t)(slash-path)+strlen("SDL3.dll")+1>=sizeof(path)) {
    say("SDL3-PROBE: disabled: cannot resolve executable-relative SDL3.dll");
    return 0;
  }
  strcpy(slash+1,"SDL3.dll");
  module=LoadLibraryA(path);
  if(!module) {
    say("SDL3-PROBE: SDL3.dll absent/unloadable beside SpideyPC.exe; legacy controller input untouched");
    return 0;
  }
#define LOAD_SDL(field,type,symbol) do { field=(type)GetProcAddress(module,symbol); if(!field)goto missing; }while(0)
  LOAD_SDL(sdl_init,SDL_InitSubSystemFn,"SDL_InitSubSystem");
  LOAD_SDL(sdl_main_ready,SDL_SetMainReadyFn,"SDL_SetMainReady");
  LOAD_SDL(sdl_pump,SDL_PumpEventsFn,"SDL_PumpEvents");
  LOAD_SDL(sdl_update,SDL_UpdateGamepadsFn,"SDL_UpdateGamepads");
  LOAD_SDL(sdl_gamepads,SDL_GetGamepadsFn,"SDL_GetGamepads");
  LOAD_SDL(sdl_joysticks,SDL_GetJoysticksFn,"SDL_GetJoysticks");
  LOAD_SDL(sdl_name,SDL_GetGamepadNameForIDFn,"SDL_GetGamepadNameForID");
  LOAD_SDL(sdl_vendor,SDL_GetGamepadVendorForIDFn,"SDL_GetGamepadVendorForID");
  LOAD_SDL(sdl_product,SDL_GetGamepadProductForIDFn,"SDL_GetGamepadProductForID");
  LOAD_SDL(sdl_open,SDL_OpenGamepadFn,"SDL_OpenGamepad");
  LOAD_SDL(sdl_close,SDL_CloseGamepadFn,"SDL_CloseGamepad");
  LOAD_SDL(sdl_button,SDL_GetGamepadButtonFn,"SDL_GetGamepadButton");
  LOAD_SDL(sdl_axis,SDL_GetGamepadAxisFn,"SDL_GetGamepadAxis");
  LOAD_SDL(sdl_error,SDL_GetErrorFn,"SDL_GetError");
  LOAD_SDL(sdl_free,SDL_freeFn,"SDL_free");
  LOAD_SDL(sdl_quit_subsystem,SDL_QuitSubSystemFn,"SDL_QuitSubSystem");
#undef LOAD_SDL
  /* Existing WinMain belongs to the original 2001 EXE, not SDL_main. */
  sdl_main_ready();
  if(!sdl_init(SPIDER_SDL_INIT_GAMEPAD)) {
    snprintf(line,sizeof(line),"SDL3-PROBE: SDL_InitSubSystem failed: %.200s",
             sdl_error()?sdl_error():"unknown");
    say(line);
    FreeLibrary(module);module=NULL;
    return 0;
  }
  ready=1;
  say("SDL3-PROBE: SDL3 gamepad subsystem ready (read-only); no DirectInput takeover");
  return 1;
missing:
  say("SDL3-PROBE: DLL missing required SDL3 3.2+ symbol; legacy input untouched");
  FreeLibrary(module);module=NULL;
  return 0;
}
static int has_id(const uint32_t *ids,unsigned count,uint32_t id) {
  unsigned i;for(i=0;i<count;i++)if(ids[i]==id)return 1;return 0;
}
/* Read-only diagnostic of the ORIGINAL game's selected joystick.
 * The SDL3 selection does not drive this game-owned DirectInput pointer.
 * The fixed global is known from the SHA-verified native reconnect module.
 * No COM invocation, pointer replacement or action table writes here. */
#define SPIDER_NATIVE_JOYSTICK_PTR 0x006B7A2Cu
static void sdl3_trace_native_owner(DWORD now) {
  static uintptr_t last_pointer=UINTPTR_MAX;
  static uint32_t last_selected=UINT32_MAX;
  static unsigned last_count=~0u;
  static DWORD last_time=0;
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t address=SPIDER_NATIVE_JOYSTICK_PTR,owner=0,base;
  char line[270];
  if(!selection.count || sizeof(void*)!=4 ||
     (uintptr_t)GetModuleHandleA(NULL)!=0x00400000u ||
     (uint32_t)(now-last_time)<200u)return;
  last_time=now;
  if(VirtualQuery((const void *)address,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT ||
     (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
     !mbi.Protect)return;
  base=(uintptr_t)mbi.BaseAddress;
  if(address<base || address-base>mbi.RegionSize ||
     mbi.RegionSize-(size_t)(address-base)<sizeof(void*))return;
  owner=(uintptr_t)InterlockedCompareExchangePointer(
     (PVOID volatile *)address,NULL,NULL);
  if(owner==last_pointer && selection.active==last_selected &&
     selection.count==last_count)return;
  snprintf(line,sizeof(line),
    "SDL3-PROBE: NATIVE-OWNER physical-opened=%u selected-SDL-id=%lu selected-VID:PID=%04X:%04X game-owned-DirectInput=%p (read-only; NOT bridged to SDL3)",
    selection.count,(unsigned long)selection.active,
    selection.active?(unsigned)sdl_vendor(selection.active):0u,
    selection.active?(unsigned)sdl_product(selection.active):0u,(void *)owner);
  say(line);
  last_pointer=owner;
  last_selected=selection.active;
  last_count=selection.count;
}
static void sdl3_detached(uint32_t id) {
  char line[125];
  snprintf(line,sizeof(line),"SDL3-PROBE: detached id=%lu",
           (unsigned long)id);
  say(line);
}
/* Only called on original game Flip thread. Bound allocations and polling. */
static void rescan(void) {
  uint32_t *ids=NULL,*joysticks=NULL,opened_ids[SPIDER_SDL3_MAX_GAMEPADS];
  unsigned opened_n=0,i,j,n;
  int mapped=0,total=0,changed=0;
  char line[300];
  ids=sdl_gamepads(&mapped);
  joysticks=sdl_joysticks(&total);
  if(mapped<0)mapped=0;
  if(total<0)total=0;
  /* Ignore a transient failed query rather than disconnecting everything. */
  if(!ids || !joysticks) {
    say("SDL3-PROBE: enumeration unavailable; existing selection unchanged");
    if(ids)sdl_free(ids);
    if(joysticks)sdl_free(joysticks);
    return;
  }
  n=(unsigned)mapped;
  if(n>SPIDER_SDL3_MAX_GAMEPADS)n=SPIDER_SDL3_MAX_GAMEPADS;
  /* Unplug can happen between two scans. Remove stale instances exactly
   * once, close only non-NULL SDL handles and compact before opening new pads.
   * A disconnected slot must never survive to the next rescan. */
  changed=(int)spider_sdl3_drop_absent(pads,&num_pads,ids,n,
                                      sdl_close,sdl3_detached);
  for(i=0;i<n;i++) {
    const char *name;
    void *handle;
    if(!ids[i] || has_id(opened_ids,opened_n,ids[i]))continue;
    for(j=0;j<num_pads;j++)if(pads[j].id==ids[i] && pads[j].handle)break;
    if(j<num_pads) {
      opened_ids[opened_n++]=ids[i];
      continue;
    }
    handle=sdl_open(ids[i]);
    if(!handle)continue;
    name=sdl_name(ids[i]);
    snprintf(line,sizeof(line),"SDL3-PROBE: attached id=%lu VID:PID=%04X:%04X name=%.130s",
             (unsigned long)ids[i],(unsigned)sdl_vendor(ids[i]),
             (unsigned)sdl_product(ids[i]),name?name:"unknown");
    say(line);
    /* Reuse a slot vacated by a removed controller. */
    for(j=0;j<SPIDER_SDL3_MAX_GAMEPADS;j++)
      if(j>=num_pads || !pads[j].handle)break;
    if(j==SPIDER_SDL3_MAX_GAMEPADS) {sdl_close(handle);continue;}
    pads[j].id=ids[i];pads[j].handle=handle;
    if(j>=num_pads)num_pads=j+1;
    opened_ids[opened_n++]=ids[i];
    changed=1;
  }
  /* Shrink trailing closed slots and refresh policy with valid open IDs. */
  while(num_pads && !pads[num_pads-1].handle)num_pads--;
  for(i=0;i<opened_n;i++)if(!opened_ids[i])opened_ids[i]=0;
  {
    uint32_t was=selection.active;
    spider_sdl3_selection_sync(&selection,opened_ids,opened_n);
    if(was!=selection.active) {
      snprintf(line,sizeof(line),"SDL3-PROBE: selected logical gamepad id=%lu VID:PID=%04X:%04X (previous=%lu), READ-ONLY",
               (unsigned long)selection.active,
               selection.active?(unsigned)sdl_vendor(selection.active):0u,
               selection.active?(unsigned)sdl_product(selection.active):0u,
               (unsigned long)was);
      say(line);
    }
  }
  if(!scanned || changed || mapped!=last_mapped_count || total!=last_joystick_count) {
    snprintf(line,sizeof(line),
      "SDL3-PROBE: discovery physical-joysticks=%d mapped-gamepads=%d opened=%u active=%lu (extra devices ignored; no game input injection)",
      total,mapped,opened_n,(unsigned long)selection.active);
    say(line);
    if(mapped>(int)SPIDER_SDL3_MAX_GAMEPADS)
      say("SDL3-PROBE: more than 16 mapped pads, experiment tracks first 16 only");
  }
  scanned=1;
  last_mapped_count=mapped;last_joystick_count=total;
  sdl_free(ids);sdl_free(joysticks);
}
void SpiderSDL3Probe_OnFlip(long flip,HWND game_hwnd) {
  DWORD now;
  unsigned i;
  char line[230];
  if(!requested || flip<120)return;
  if(!game_hwnd || !IsWindowVisible(game_hwnd) ||
     GetForegroundWindow()!=game_hwnd) {
    /* Never inherit a Start press acquired during the custom focus pause.
     * Selection identity stays stable, but takeover needs neutral->press. */
    if(ready)spider_sdl3_selection_focus_lost(&selection);
    return;
  }
  if(!attempted && !sdl_load())return;
  if(!ready)return;
  /* SDL3's event pump and initialization stay on the game/Flip thread. */
  sdl_pump();
  sdl_update();
  now=GetTickCount();
  if(!scanned || (uint32_t)(now-last_scan)>=SPIDER_SDL_SCAN_MS) {
    last_scan=now;
    rescan();
  }
  sdl3_trace_native_owner(now);
  for(i=0;i<num_pads;i++)if(pads[i].handle) {
    int pressed=sdl_button(pads[i].handle,SPIDER_SDL_BUTTON_START)?1:0;
    if(spider_sdl3_selection_start(&selection,pads[i].id,pressed)) {
      snprintf(line,sizeof(line),"SDL3-PROBE: START selected logical id=%lu VID:PID=%04X:%04X (other gamepads ignored; READ-ONLY)",
               (unsigned long)selection.active,
               (unsigned)sdl_vendor(selection.active),
               (unsigned)sdl_product(selection.active));
      say(line);
    }
    if(selection.active==pads[i].id &&
       (!last_axis_log || (uint32_t)(now-last_axis_log)>=1500u)) {
      short lt=sdl_axis(pads[i].handle,SPIDER_SDL_AXIS_LEFT_TRIGGER);
      short rt=sdl_axis(pads[i].handle,SPIDER_SDL_AXIS_RIGHT_TRIGGER);
      if(lt!=old_lt || rt!=old_rt) {
        snprintf(line,sizeof(line),"SDL3-PROBE: selected id=%lu normalized LT=%d RT=%d (passive)",
                 (unsigned long)pads[i].id,(int)lt,(int)rt);
        say(line);old_lt=lt;old_rt=rt;
      }
      last_axis_log=now;
    }
  }
  /* A Start claim may have changed logical ownership after the scan. */
  sdl3_trace_native_owner(now);
}
