/* SDL3 controller discovery and explicit Start selection for Spider-Man 2001.
 * Load the bundled SDL3.dll at the first safe foreground game Flip, never
 * from system PATH, and pump only on that original game thread. SDL3 is the
 * sole gamepad mapping backend; DirectInput remains only for original-game
 * internals and safety. No SDL video/audio subsystem or extra threads.
 * [Debug] Log=1 enables diagnostics; SDL3 instance IDs identify pads. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "spider_fix_paths.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_gamepad_probe.h"
#include "sdl3_gamepad_selection.h"
#include "sdl3_gamepad_slots.h"
#include "sdl3_gamepad_identity.h"
#include "sdl3_controller_profile.h"
#include "sdl3_universal_actions.h"
#include "sdl3_controller_profile_store.h"
#include "native_joystick_reconnect.h"
#include "native_joystick_reconnect_policy.h"
#include "sdl3_raw_mapping.h"
#include "ps1_focus_pause_wake.h"

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
typedef int (__cdecl *SDL_AddGamepadMappingsFromFileFn)(const char *);
typedef bool (__cdecl *SDL_IsGamepadFn)(uint32_t);
typedef const char *(__cdecl *SDL_GetJoystickNameForIDFn)(uint32_t);
typedef const char *(__cdecl *SDL_GetGamepadNameForIDFn)(uint32_t);
typedef uint16_t (__cdecl *SDL_GetGamepadVendorForIDFn)(uint32_t);
typedef uint16_t (__cdecl *SDL_GetGamepadProductForIDFn)(uint32_t);
typedef void *(__cdecl *SDL_OpenGamepadFn)(uint32_t);
typedef void (__cdecl *SDL_CloseGamepadFn)(void *);
typedef bool (__cdecl *SDL_GetGamepadButtonFn)(void *,int);
typedef bool (__cdecl *SDL_GamepadConnectedFn)(void *);
typedef short (__cdecl *SDL_GetGamepadAxisFn)(void *,int);
typedef const char *(__cdecl *SDL_GetErrorFn)(void);
typedef void (__cdecl *SDL_freeFn)(void *);
typedef void (__cdecl *SDL_QuitSubSystemFn)(uint32_t);
typedef struct { uint8_t data[16]; } SpiderSDL3GUID;
typedef SpiderSDL3GUID (__cdecl *SDL_GetJoystickGUIDForIDFn)(uint32_t);
typedef void (__cdecl *SDL_GUIDToStringFn)(SpiderSDL3GUID,char *,int);
typedef void *(__cdecl *SDL_OpenJoystickFn)(uint32_t);
typedef void (__cdecl *SDL_CloseJoystickFn)(void *);
typedef int (__cdecl *SDL_GetNumJoystickButtonsFn)(void *);
typedef int (__cdecl *SDL_GetNumJoystickAxesFn)(void *);
typedef int (__cdecl *SDL_GetNumJoystickHatsFn)(void *);
typedef bool (__cdecl *SDL_GetJoystickButtonFn)(void *,int);
typedef int16_t (__cdecl *SDL_GetJoystickAxisFn)(void *,int);
typedef uint8_t (__cdecl *SDL_GetJoystickHatFn)(void *,int);
typedef int (__cdecl *SDL_AddGamepadMappingFn)(const char *);

static SpiderSDL3Opened pads[SPIDER_SDL3_MAX_GAMEPADS];
static SpiderSDL3Selection selection={0};
static unsigned num_pads=0;
static int requested=0,attempted=0,ready=0,scanned=0;
static int native_owner_requested=0;
static int universal_requested=0;
static int bindings_trace_requested=0;
/* One SDL3 semantic profile per VID:PID; persisted for opted-in
 * controller setup only. No game-owned DirectInput action writes. */
static SpiderSDL3ProfileBank bindings_bank;
static uint8_t bindings_loaded[SPIDER_SDL3_PROFILE_BANK_CAPACITY];
static uint32_t bindings_trace_id=0,bindings_previous=0;
static int bindings_trace_armed=0;
static unsigned bindings_trace_lines=0;
static DWORD last_scan=0,last_axis_log=0;
static int last_joystick_count=-1,last_mapped_count=-1;
static short old_lt=-1,old_rt=-1;
/* Read-only DualShock 3 054C:0268 hardware probe. Its SDL3 gamepad is
 * visible but the original game's DirectInput action indices have not
 * been verified. Keep the trace finite and NEVER grant native ownership
 * from mere SDL3 detection. */
static uint32_t ps3_trace_id=0,ps3_trace_buttons=0;
static unsigned ps3_trace_lines=0;
static HMODULE module=NULL;
static DWORD sdl3_game_thread=0;
/* Pending selection from an explicitly pressed Start while the setup modal
 * owns the game thread. Commit only at the next foreground Flip, after
 * verifying the same opened unique SDL3 device. Never rebind from modal. */
static uint32_t setup_owner_pending=0;
static uint16_t setup_owner_vendor=0,setup_owner_product=0;
/* Separate GENERIC SDL3 controller selection: never pretend that a pad
 * with unknown DirectInput layout owns the original 2001 input backend.
 * Explicit Start in the SDL3 setup modal is required. A successful
 * selection grants read-only action sampling, not native gameplay. */
static uint32_t generic_selected_id=0;
static uint16_t generic_selected_vendor=0,generic_selected_product=0;
static SpiderSDL3UniversalActions generic_actions={0};
static SpiderSDL3ControllerProfile generic_profile_snapshot={0};
static int generic_profile_snapshot_valid=0;
static unsigned generic_trace_lines=0;
static uint32_t generic_last_buttons=0;
static int generic_last_axes[4]={0};
static void (*report)(const char *)=NULL;
static SDL_InitSubSystemFn sdl_init=NULL;
static SDL_SetMainReadyFn sdl_main_ready=NULL;
static SDL_PumpEventsFn sdl_pump=NULL;
static SDL_UpdateGamepadsFn sdl_update=NULL;
static SDL_GetGamepadsFn sdl_gamepads=NULL;
static SDL_GetJoysticksFn sdl_joysticks=NULL;
static SDL_AddGamepadMappingsFromFileFn sdl_add_mappings_file=NULL;
static SDL_IsGamepadFn sdl_is_gamepad=NULL;
static SDL_GetJoystickNameForIDFn sdl_joystick_name=NULL;
static uint32_t raw_last_ids[SPIDER_SDL3_MAX_GAMEPADS]={0};
static unsigned raw_last_n=0;
static SDL_GetGamepadNameForIDFn sdl_name=NULL;
static SDL_GetGamepadVendorForIDFn sdl_vendor=NULL;
static SDL_GetGamepadProductForIDFn sdl_product=NULL;
static SDL_OpenGamepadFn sdl_open=NULL;
static SDL_CloseGamepadFn sdl_close=NULL;
static SDL_GetGamepadButtonFn sdl_button=NULL;
static SDL_GamepadConnectedFn sdl_connected=NULL;
static SpiderPausePadWake sdl_pause_wake={0};
static int sdl_pause_universal=0;
static uint32_t sdl_pause_id=0;
static uint16_t sdl_pause_vid=0,sdl_pause_pid=0;
static int sdl_pause_active=0,sdl_pause_lost_reported=0;
static SDL_GetGamepadAxisFn sdl_axis=NULL;
static SDL_GetErrorFn sdl_error=NULL;
static SDL_freeFn sdl_free=NULL;
static SDL_QuitSubSystemFn sdl_quit_subsystem=NULL;
static SDL_GetJoystickGUIDForIDFn sdl_joystick_guid=NULL;
static SDL_GUIDToStringFn sdl_guid_string=NULL;
static SDL_OpenJoystickFn sdl_open_joystick=NULL;
static SDL_CloseJoystickFn sdl_close_joystick=NULL;
static SDL_GetNumJoystickButtonsFn sdl_raw_buttons_count=NULL;
static SDL_GetNumJoystickAxesFn sdl_raw_axes_count=NULL;
static SDL_GetNumJoystickHatsFn sdl_raw_hats_count=NULL;
static SDL_GetJoystickButtonFn sdl_raw_button=NULL;
static SDL_GetJoystickAxisFn sdl_raw_axis=NULL;
static SDL_GetJoystickHatFn sdl_raw_hat=NULL;
static SDL_AddGamepadMappingFn sdl_add_mapping=NULL;
static void *raw_handle=NULL;
static uint32_t raw_session_id=0;

static void say(const char *line) {if(report)report(line);}
/* Load once per model, including a profile created earlier by the passive
 * binding trace. A corrupted file is ignored without modifying the disk. */
static SpiderSDL3ControllerProfile *spider_sdl3_get_profile(
    uint16_t vid,uint16_t pid) {
  SpiderSDL3ControllerProfile trial,*profile;
  unsigned slot;
  int load_status;
  profile=spider_sdl3_bank_get(&bindings_bank,vid,pid);
  if(!profile)return NULL;
  slot=(unsigned)(profile-bindings_bank.slots);
  if(slot>=SPIDER_SDL3_PROFILE_BANK_CAPACITY)return NULL;
  if(!bindings_loaded[slot]) {
    bindings_loaded[slot]=1;
    load_status=SpiderSDL3ProfileStore_Load(vid,pid,&trial);
    if(load_status==1 && spider_sdl3_profile_valid(&trial))
      *profile=trial;
  }
  return profile;
}
void SpiderSDL3Probe_Init(const char *ini,void (*logger)(const char *)) {
  report=logger;
  spider_sdl3_bank_reset(&bindings_bank);
  memset(bindings_loaded,0,sizeof(bindings_loaded));
  bindings_trace_id=0;bindings_previous=0;
  bindings_trace_armed=0;bindings_trace_lines=0;
  generic_selected_id=0;
  generic_selected_vendor=0;generic_selected_product=0;
  generic_profile_snapshot_valid=0;
  generic_trace_lines=0;generic_last_buttons=0;
  memset(generic_last_axes,0,sizeof(generic_last_axes));
  spider_sdl3_universal_reset(&generic_actions);
  bindings_trace_requested=ini &&
    GetPrivateProfileIntA("Debug","SDL3BindingTrace",0,ini)==1;
  /* SDL3 is the only supported mapping backend. Old INI toggles no longer
   * change whether discovery, profiles, or universal gameplay run. */
  requested=1;
  universal_requested=1;
  SpiderSDL3ProfileStore_Init(1,logger);
  if(bindings_trace_requested)
    say("SDL3-BINDINGS: optional semantic action probe ENABLED (read-only; no gameplay remap, no UI changes; max 48 events)");
  /* Retire the legacy SDL3-identity -> native DirectInput rebind: mapped
   * SDL3 input goes through the universal adapter exclusively. */
  native_owner_requested=0;
  if(requested)say("SDL3-PROBE: built-in discovery and explicit Start selection; ready on first safe foreground Flip");
  if(universal_requested)
    say("SDL3-UNIVERSAL: default-on logical Start selection for mapped SDL3 pads; no unverified native DI rebind");
}

int SpiderSDL3Probe_NativeOwnerEnabled(void) {
  return native_owner_requested && ready && scanned;
}
unsigned SpiderSDL3Probe_MappedCount(void) {
  return SpiderSDL3Probe_NativeOwnerEnabled()?selection.count:0;
}
int SpiderSDL3Probe_SelectedIdentity(uint16_t *vid,uint16_t *pid,
                                     uint32_t *instance_id) {
  SpiderSDL3Identity open[SPIDER_SDL3_MAX_GAMEPADS]={{0}};
  unsigned i;
  uint16_t v,p;
  if(vid)*vid=0;
  if(pid)*pid=0;
  if(instance_id)*instance_id=0;
  if(!SpiderSDL3Probe_NativeOwnerEnabled() || !selection.active ||
     !sdl_vendor || !sdl_product)return 0;
  v=sdl_vendor(selection.active);p=sdl_product(selection.active);
  if(!v || !p)return 0; /* A synthetic/unknown product has no safe DI match. */
  /* A single-player session can contain several DISTINCT controllers.
   * It is safe to bind their selected native DI device by exact VID:PID.
   * Two open pads with the same model are ambiguous under the original
   * game's DirectInput8 product-only identity: fail closed for both. */
  for(i=0;i<num_pads && i<SPIDER_SDL3_MAX_GAMEPADS;i++) {
    if(pads[i].id && pads[i].handle &&
       (!sdl_connected || sdl_connected(pads[i].handle))) {
      open[i].instance_id=pads[i].id;
      open[i].vendor=sdl_vendor(pads[i].id);
      open[i].product=sdl_product(pads[i].id);
    }
  }
  if(!spider_sdl3_selected_model_unique(
       open,num_pads,selection.active,v,p))return 0;
  if(vid)*vid=v;
  if(pid)*pid=p;
  if(instance_id)*instance_id=selection.active;
  return 1;
}

/* Per-user fallback when game files are installed under Program Files.
 * Separate from community gamecontrollerdb and per-action profile storage. */
static int spider_raw_local_path(const char *file,char *out,
                                 size_t cap,int create) {
  char root[MAX_PATH],folder[MAX_PATH];
  DWORD n,attrs;
  int wrote;
  if(!file || !out || !cap)return 0;
  n=GetEnvironmentVariableA("LOCALAPPDATA",root,sizeof(root));
  if(!n || n>=sizeof(root))return 0;
  wrote=snprintf(folder,sizeof(folder),"%s\\SpiderMan2k1Fix",root);
  if(wrote<=0 || (size_t)wrote>=sizeof(folder))return 0;
  attrs=GetFileAttributesA(folder);
  if(attrs==INVALID_FILE_ATTRIBUTES && create) {
    if(!CreateDirectoryA(folder,NULL) &&
       GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    attrs=GetFileAttributesA(folder);
  }
  if(attrs==INVALID_FILE_ATTRIBUTES ||
     !(attrs&FILE_ATTRIBUTE_DIRECTORY) ||
     (attrs&FILE_ATTRIBUTE_REPARSE_POINT))return 0;
  wrote=snprintf(out,cap,"%s\\%s",folder,file);
  return wrote>0 && (size_t)wrote<cap;
}
static int sdl_load(void) {
  char path[MAX_PATH],line[310];
  attempted=1;
  if(!spider_path_code_asset_a("SDL3.dll",path,sizeof(path))) {
    say("SDL3-PROBE: disabled: cannot resolve game-relative SDL3.dll");
    return 0;
  }
  module=LoadLibraryA(path);
  if(!module) {
    say("SDL3-PROBE: SDL3.dll absent/unloadable from fix folder or legacy root; native controller input untouched");
    return 0;
  }
#define LOAD_SDL(field,type,symbol) do { field=(type)GetProcAddress(module,symbol); if(!field)goto missing; }while(0)
  LOAD_SDL(sdl_init,SDL_InitSubSystemFn,"SDL_InitSubSystem");
  LOAD_SDL(sdl_main_ready,SDL_SetMainReadyFn,"SDL_SetMainReady");
  LOAD_SDL(sdl_pump,SDL_PumpEventsFn,"SDL_PumpEvents");
  LOAD_SDL(sdl_update,SDL_UpdateGamepadsFn,"SDL_UpdateGamepads");
  LOAD_SDL(sdl_gamepads,SDL_GetGamepadsFn,"SDL_GetGamepads");
  LOAD_SDL(sdl_joysticks,SDL_GetJoysticksFn,"SDL_GetJoysticks");
  /* Optional diagnostics for raw devices without SDL Gamepad mapping.
   * This does NOT grant game ownership or invent a physical mapping. */
  sdl_is_gamepad=(SDL_IsGamepadFn)GetProcAddress(module,"SDL_IsGamepad");
  sdl_add_mappings_file=(SDL_AddGamepadMappingsFromFileFn)
    GetProcAddress(module,"SDL_AddGamepadMappingsFromFile");
  sdl_joystick_name=(SDL_GetJoystickNameForIDFn)GetProcAddress(module,"SDL_GetJoystickNameForID");
#define SDL_RAW_LOAD(field,type,symbol) field=(type)GetProcAddress(module,symbol)
  SDL_RAW_LOAD(sdl_joystick_guid,SDL_GetJoystickGUIDForIDFn,"SDL_GetJoystickGUIDForID");
  SDL_RAW_LOAD(sdl_guid_string,SDL_GUIDToStringFn,"SDL_GUIDToString");
  SDL_RAW_LOAD(sdl_open_joystick,SDL_OpenJoystickFn,"SDL_OpenJoystick");
  SDL_RAW_LOAD(sdl_close_joystick,SDL_CloseJoystickFn,"SDL_CloseJoystick");
  SDL_RAW_LOAD(sdl_raw_buttons_count,SDL_GetNumJoystickButtonsFn,"SDL_GetNumJoystickButtons");
  SDL_RAW_LOAD(sdl_raw_axes_count,SDL_GetNumJoystickAxesFn,"SDL_GetNumJoystickAxes");
  SDL_RAW_LOAD(sdl_raw_hats_count,SDL_GetNumJoystickHatsFn,"SDL_GetNumJoystickHats");
  SDL_RAW_LOAD(sdl_raw_button,SDL_GetJoystickButtonFn,"SDL_GetJoystickButton");
  SDL_RAW_LOAD(sdl_raw_axis,SDL_GetJoystickAxisFn,"SDL_GetJoystickAxis");
  SDL_RAW_LOAD(sdl_raw_hat,SDL_GetJoystickHatFn,"SDL_GetJoystickHat");
  SDL_RAW_LOAD(sdl_add_mapping,SDL_AddGamepadMappingFn,"SDL_AddGamepadMapping");
#undef SDL_RAW_LOAD
  LOAD_SDL(sdl_name,SDL_GetGamepadNameForIDFn,"SDL_GetGamepadNameForID");
  LOAD_SDL(sdl_vendor,SDL_GetGamepadVendorForIDFn,"SDL_GetGamepadVendorForID");
  LOAD_SDL(sdl_product,SDL_GetGamepadProductForIDFn,"SDL_GetGamepadProductForID");
  LOAD_SDL(sdl_open,SDL_OpenGamepadFn,"SDL_OpenGamepad");
  LOAD_SDL(sdl_close,SDL_CloseGamepadFn,"SDL_CloseGamepad");
  LOAD_SDL(sdl_button,SDL_GetGamepadButtonFn,"SDL_GetGamepadButton");
  /* Optional SDL symbol: absence disables only the experimental wake. */
  sdl_connected=(SDL_GamepadConnectedFn)GetProcAddress(module,"SDL_GamepadConnected");
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
  /* Optional upstream SDL gamecontrollerdb-style mappings are loaded from
   * the fix installation only, never a working-directory relative path.
   * SDL applies its own platform filter and upgrades raw joysticks to
   * mapped gamepads. An absent file leaves SDL's built-in DB unchanged. */
  if(sdl_add_mappings_file) {
    char dbpath[MAX_PATH],dbmsg[350];
    DWORD attrs;
    if(spider_path_code_asset_a("SDL3-gamecontrollerdb.txt",dbpath,
                                sizeof(dbpath))) {
      attrs=GetFileAttributesA(dbpath);
      if(attrs!=INVALID_FILE_ATTRIBUTES && !(attrs&FILE_ATTRIBUTE_DIRECTORY)) {
        int added=sdl_add_mappings_file(dbpath);
        snprintf(dbmsg,sizeof(dbmsg),
          "SDL3-MAPPINGS: external model database entries=%d status=%s (SDL platform filtering; no native owner claim)",
          added,added<0?"FAILED":"LOADED");
        say(dbmsg);
      }
    }
  }
  /* Per-device mappings saved by the keyboard wizard. Load installation
   * files first, then LOCALAPPDATA overrides (the user-writable fallback).
   * A single GUID-specific file never rewrites the community DB. */
  if(sdl_add_mappings_file) {
    char wildcard[MAX_PATH],mapfile[MAX_PATH],dbmsg[330];
    WIN32_FIND_DATAA found;
    HANDLE find;
    unsigned loaded=0,source;
    for(source=0;source<2u;source++) {
      int pathok=source==0u?
        spider_path_code_asset_a("SDL3-mapping-*.txt",wildcard,sizeof(wildcard)):
        spider_raw_local_path("SDL3-mapping-*.txt",wildcard,sizeof(wildcard),0);
      if(!pathok)continue;
      find=FindFirstFileA(wildcard,&found);
      if(find==INVALID_HANDLE_VALUE)continue;
      do {
        const char *n=found.cFileName;
        size_t len=strlen(n);
        unsigned k;
        int ok=(len==49u && memcmp(n,"SDL3-mapping-",13)==0 &&
                strcmp(n+45,".txt")==0);
        if(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        for(k=13u;ok && k<45u;k++)
          if(!isxdigit((unsigned char)n[k]))ok=0;
        if(!ok || loaded>=128u)continue;
        pathok=source==0u?
          spider_path_code_asset_a(n,mapfile,sizeof(mapfile)):
          spider_raw_local_path(n,mapfile,sizeof(mapfile),0);
        if(pathok && sdl_add_mappings_file(mapfile)>=0)loaded++;
      } while(FindNextFileA(find,&found));
      FindClose(find);
    }
    snprintf(dbmsg,sizeof(dbmsg),
      "SDL3-MAPPINGS: persisted per-GUID user mappings loaded=%u (game directory + LOCALAPPDATA fallback; SDL Gamepad API only)",
      loaded);
    say(dbmsg);
  }
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
  if((!ids && mapped!=0) || (!joysticks && total!=0)) {
    say("SDL3-PROBE: enumeration unavailable; existing selection unchanged");
    if(ids)sdl_free(ids);
    if(joysticks)sdl_free(joysticks);
    return;
  }
  /* Gamepad enumeration excludes raw joysticks without a standard SDL
   * mapping. Expose these to diagnostics instead of silently implying
   * universal support. Never open them on the game-owned DI path. */
  {
    uint32_t current_raw[SPIDER_SDL3_MAX_GAMEPADS]={0};
    unsigned raw_n=0,k;
    for(k=0;k<(unsigned)total && raw_n<SPIDER_SDL3_MAX_GAMEPADS;k++) {
      uint32_t id=joysticks[k];
      unsigned seen=0,jj;
      if(!id)continue;
      if(sdl_is_gamepad) {
        if(sdl_is_gamepad(id))continue;
      } else {
        for(jj=0;jj<(unsigned)mapped;jj++)
          if(ids[jj]==id) {seen=1;break;}
        if(seen)continue;
      }
      current_raw[raw_n++]=id;
      for(jj=0;jj<raw_last_n;jj++)
        if(raw_last_ids[jj]==id)break;
      if(jj==raw_last_n) {
        const char *name=sdl_joystick_name?sdl_joystick_name(id):NULL;
        snprintf(line,sizeof(line),
          "SDL3-RAW: unmapped joystick id=%lu name=%.130s; detected but NOT configurable/usable in 3D yet; raw mapping wizard required",
          (unsigned long)id,name?name:"unknown");
        say(line);
      }
    }
    if(raw_n!=raw_last_n || memcmp(current_raw,raw_last_ids,
                                   sizeof(raw_last_ids))!=0) {
      memcpy(raw_last_ids,current_raw,sizeof(raw_last_ids));
      raw_last_n=raw_n;
    }
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
  /* Load existing model profiles when SDL3 discovers them at startup.
   * No dependency on opening the UI or enabling semantic trace. */
  if(requested && sdl_vendor && sdl_product) {
    unsigned k;
    for(k=0;k<num_pads;k++)
      if(pads[k].id && pads[k].handle)
        spider_sdl3_get_profile(sdl_vendor(pads[k].id),
                                sdl_product(pads[k].id));
  }
  last_mapped_count=mapped;last_joystick_count=total;
  if(ids)sdl_free(ids);
  if(joysticks)sdl_free(joysticks);
}
/* A setup modal pauses the game's original Flip thread on its stack.
 * Pump the already-loaded SDL3 gamepads through this explicit same-thread
 * entry point. Never claim a native owner or synthesize an input here. */
int SpiderSDL3Probe_SetupPump(void) {
  DWORD now;
  if(!requested || !ready || !scanned || !sdl3_game_thread ||
     sdl3_game_thread!=GetCurrentThreadId() ||
     !sdl_pump || !sdl_update)return 0;
  sdl_pump();sdl_update();
  now=GetTickCount();
  if((uint32_t)(now-last_scan)>=SPIDER_SDL_SCAN_MS) {
    last_scan=now;
    rescan();
  }
  return 1;
}
void SpiderSDL3Probe_SetupModalReturn(uint32_t requested_id,
                                      uint16_t vendor,uint16_t product) {
  setup_owner_pending=0;
  setup_owner_vendor=0;
  setup_owner_product=0;
  if(!ready || !scanned || !sdl3_game_thread ||
     sdl3_game_thread!=GetCurrentThreadId())return;
  /* Start sampled inside the dialog must never be replayed as a second
   * normal game selection press. Its specific owner request is verified
   * separately at the next game-owned foreground Flip. */
  spider_sdl3_selection_focus_lost(&selection);
  if(requested_id && vendor && product) {
    setup_owner_pending=requested_id;
    setup_owner_vendor=vendor;
    setup_owner_product=product;
    say("SDL3-PROBE: SETUP RETURN controller-initiated owner handoff PENDING (Start or pad Close; foreground Flip only; native DirectInput untouched)");
  } else
    say("SDL3-PROBE: SETUP RETURN Start takeover disarmed until neutral; gameplay selected-id preserved");
}
SpiderSDL3ControllerProfile *SpiderSDL3Probe_SetupProfile(uint16_t vid,uint16_t pid) {
  if(!ready || !scanned || !sdl3_game_thread ||
     sdl3_game_thread!=GetCurrentThreadId())return NULL;
  return spider_sdl3_get_profile(vid,pid);
}
int SpiderSDL3Probe_SetupCommit(const SpiderSDL3ControllerProfile *candidate) {
  SpiderSDL3ControllerProfile *profile;
  if(!candidate || !spider_sdl3_profile_valid(candidate) ||
     !ready || !scanned || !sdl3_game_thread ||
     sdl3_game_thread!=GetCurrentThreadId())return 0;
  profile=spider_sdl3_get_profile(candidate->vendor,candidate->product);
  if(!profile || !SpiderSDL3ProfileStore_Save(candidate))return 0;
  *profile=*candidate;
  bindings_trace_armed=0;bindings_previous=0;
  generic_profile_snapshot_valid=0;
  spider_sdl3_universal_reset(&generic_actions);
  return 1;
}

static int spider_raw_ui_thread(void) {
  return requested && ready && scanned && sdl3_game_thread &&
         sdl3_game_thread==GetCurrentThreadId();
}
unsigned SpiderSDL3Probe_RawListAll(uint32_t *out,
                                      unsigned *mapped_flags,
                                      unsigned capacity) {
  uint32_t *raw;
  int count=0;
  unsigned i,n=0;
  if(!spider_raw_ui_thread() || !sdl_joysticks || !sdl_is_gamepad)return 0;
  raw=sdl_joysticks(&count);
  if(!raw)return 0;
  if(count<0) {sdl_free(raw);return 0;}
  for(i=0;i<(unsigned)count;i++) {
    uint32_t id=raw[i];
    if(!id)continue;
    if(n<capacity) {
      if(out)out[n]=id;
      if(mapped_flags)mapped_flags[n]=sdl_is_gamepad(id)?1u:0u;
    }
    ++n;
  }
  sdl_free(raw);
  return n;
}
int SpiderSDL3Probe_RawName(uint32_t id,char *name,size_t capacity) {
  const char *readable;
  if(name && capacity)name[0]=0;
  if(!name || !capacity || !id || !spider_raw_ui_thread() ||
     !sdl_joystick_name)return 0;
  readable=sdl_joystick_name(id);
  if(!readable || !*readable)return 0;
  snprintf(name,capacity,"%s",readable);
  return 1;
}
unsigned SpiderSDL3Probe_RawList(uint32_t *out,unsigned capacity) {
  uint32_t *raw,*mapped;
  int count=0,mapped_count=0;
  unsigned i,j,n=0;
  if(!spider_raw_ui_thread() || !sdl_joysticks || !sdl_gamepads)return 0;
  raw=sdl_joysticks(&count);
  mapped=sdl_gamepads(&mapped_count);
  if((!raw && count!=0) || (!mapped && mapped_count!=0)) {
    if(raw)sdl_free(raw);
    if(mapped)sdl_free(mapped);
    return 0;
  }
  for(i=0;i<(unsigned)count;i++) {
    int ismapped=0;
    uint32_t id=raw[i];
    if(!id)continue;
    if(sdl_is_gamepad)ismapped=sdl_is_gamepad(id);
    else for(j=0;j<(unsigned)mapped_count;j++)
      if(mapped[j]==id) {ismapped=1;break;}
    if(ismapped)continue;
    if(out && n<capacity)out[n]=id;
    ++n;
  }
  if(raw)sdl_free(raw);
  if(mapped)sdl_free(mapped);
  return n;
}
void SpiderSDL3Probe_RawEnd(void) {
  if(raw_handle && sdl_close_joystick)
    sdl_close_joystick(raw_handle);
  raw_handle=NULL;raw_session_id=0;
}
int SpiderSDL3Probe_RawBegin(uint32_t id,char guid[33],
                            char *name,size_t capacity) {
  uint32_t ids[SPIDER_SDL3_MAX_GAMEPADS]={0};
  unsigned i,n;
  SpiderSDL3GUID rawguid;
  const char *device_name;
  if(guid)guid[0]=0;
  if(name && capacity)name[0]=0;
  if(!guid || !name || capacity<2u || !spider_raw_ui_thread() ||
     !sdl_joystick_guid || !sdl_guid_string || !sdl_open_joystick ||
     !sdl_close_joystick || !sdl_raw_buttons_count ||
     !sdl_raw_axes_count || !sdl_raw_hats_count || !sdl_raw_button ||
     !sdl_raw_axis || !sdl_raw_hat || !sdl_add_mapping)
    return 0;
  /* A device with an existing SDL mapping must still be eligible for
   * recalibration. A standard mapping does not guarantee correctness. */
  n=SpiderSDL3Probe_RawListAll(ids,NULL,SPIDER_SDL3_MAX_GAMEPADS);
  if(n>SPIDER_SDL3_MAX_GAMEPADS)n=SPIDER_SDL3_MAX_GAMEPADS;
  for(i=0;i<n;i++)if(ids[i]==id)break;
  if(i==n)return 0;
  SpiderSDL3Probe_RawEnd();
  rawguid=sdl_joystick_guid(id);
  sdl_guid_string(rawguid,guid,33);
  if(!spider_sdl3_guid_valid(guid))return 0;
  raw_handle=sdl_open_joystick(id);
  if(!raw_handle)return 0;
  raw_session_id=id;
  device_name=sdl_joystick_name?sdl_joystick_name(id):NULL;
  snprintf(name,capacity,"%s",device_name?device_name:"Unknown SDL joystick");
  /* SDL mapping names cannot contain commas or control characters. */
  for(i=0;name[i];i++) {
    if(name[i]==',' || (unsigned char)name[i]<32u)name[i]=' ';
  }
  if(!spider_sdl3_raw_name_safe(name)) {
    SpiderSDL3Probe_RawEnd();return 0;
  }
  return 1;
}
int SpiderSDL3Probe_RawRead(SpiderSDL3RawSample *out) {
  int buttons,axes,hats,i;
  unsigned count;
  if(out)memset(out,0,sizeof(*out));
  if(!out || !spider_raw_ui_thread() || !raw_handle ||
     !raw_session_id || !sdl_raw_button || !sdl_raw_axis ||
     !sdl_raw_hat)return 0;
  buttons=sdl_raw_buttons_count(raw_handle);
  axes=sdl_raw_axes_count(raw_handle);
  hats=sdl_raw_hats_count(raw_handle);
  if(buttons<0 || axes<0 || hats<0)return 0;
  count=(unsigned)buttons;
  if(count>32u)count=32u;
  out->button_count=count;
  for(i=0;i<(int)count;i++)
    if(sdl_raw_button(raw_handle,i))out->buttons|=1u<<(unsigned)i;
  count=(unsigned)axes;
  if(count>16u)count=16u;
  out->axis_count=count;
  for(i=0;i<(int)count;i++)out->axes[i]=sdl_raw_axis(raw_handle,i);
  count=(unsigned)hats;
  if(count>4u)count=4u;
  out->hat_count=count;
  for(i=0;i<(int)count;i++)out->hats[i]=sdl_raw_hat(raw_handle,i);
  return 1;
}
/* Save ONE hardware GUID per file to avoid rewriting the user's own
 * community mapping database. Replace only after a completed temp write.
 * The SDL runtime is updated first so an invalid mapping is never saved. */
int SpiderSDL3Probe_RawRegisterAndSave(const char *mapping,const char *guid) {
  char file[MAX_PATH],temp[MAX_PATH],filename[80],line[260];
  DWORD written=0,bytes;
  HANDLE handle;
  int result;
  if(!spider_raw_ui_thread() || !sdl_add_mapping || !mapping ||
     !spider_sdl3_guid_valid(guid) || strlen(mapping)>4096u ||
     strncmp(mapping,guid,32u)!=0 || mapping[32]!=',' ||
     strchr(mapping,'\n') || strchr(mapping,'\r'))return 0;
  SpiderSDL3Probe_RawEnd();
  result=sdl_add_mapping(mapping);
  if(result<0) {
    say("SDL3-RAW: SDL_AddGamepadMapping refused mapping; nothing saved");
    return 0;
  }
  snprintf(filename,sizeof(filename),"SDL3-mapping-%s.txt",guid);
  for(unsigned location=0u;location<2u;location++) {
    int available=location==0u?
      spider_path_code_asset_a(filename,file,sizeof(file)):
      spider_raw_local_path(filename,file,sizeof(file),1);
    if(!available || strlen(file)+5u>=sizeof(temp))continue;
    snprintf(temp,sizeof(temp),"%s.tmp",file);
    handle=CreateFileA(temp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,NULL);
    if(handle==INVALID_HANDLE_VALUE)continue;
    bytes=(DWORD)strlen(mapping);
    if(!WriteFile(handle,mapping,bytes,&written,NULL) || written!=bytes ||
       !WriteFile(handle,"\r\n",2u,&written,NULL) || written!=2u ||
       !FlushFileBuffers(handle)) {
      CloseHandle(handle);DeleteFileA(temp);continue;
    }
    CloseHandle(handle);
    if(!MoveFileExA(temp,file,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
      DeleteFileA(temp);continue;
    }
    snprintf(line,sizeof(line),
      "SDL3-RAW: MAPPING SAVED GUID=%s location=%s (SDL3 runtime activated; original game unchanged)",
      guid,location==0u?"GAME":"LOCALAPPDATA");
    say(line);
    return 1;
  }
  say("SDL3-RAW: mapping applied for this session, but atomic save failed in game directory and LOCALAPPDATA");
  return 0;
}

unsigned SpiderSDL3Probe_SetupCount(void) {
  return ready && scanned && sdl3_game_thread==GetCurrentThreadId()?
         num_pads:0;
}
int SpiderSDL3Probe_SetupRead(unsigned slot,uint32_t *instance_id,
    uint16_t *vid,uint16_t *pid,char *name,size_t capacity,
    SpiderSDL3ControllerSample *sample) {
  unsigned i;
  const char *text_name;
  void *handle;
  uint32_t id;
  if(instance_id)*instance_id=0;
  if(vid)*vid=0;
  if(pid)*pid=0;
  if(name && capacity)name[0]=0;
  if(sample)memset(sample,0,sizeof(*sample));
  if(!sample || !ready || !scanned ||
     sdl3_game_thread!=GetCurrentThreadId() ||
     slot>=num_pads || !sdl_button || !sdl_axis ||
     !sdl_vendor || !sdl_product || !sdl_name)return 0;
  handle=pads[slot].handle;id=pads[slot].id;
  if(!handle || !id)return 0;
  /* A disconnected handle must not be sampled after detachment. */
  if(!sdl_vendor(id) || !sdl_product(id))return 0;
  for(i=0;i<SPIDER_SDL3_BUTTON_COUNT;i++)
    if(sdl_button(handle,(int)i))sample->buttons|=1u<<i;
  /* The setup modal alone uses the left stick for safe UI navigation.
   * The native game and read-only on-Flip sample stay unchanged. */
  sample->axis[SPIDER_SDL3_AXIS_LEFTX]=sdl_axis(handle,SPIDER_SDL3_AXIS_LEFTX);
  sample->axis[SPIDER_SDL3_AXIS_LEFTY]=sdl_axis(handle,SPIDER_SDL3_AXIS_LEFTY);
  sample->axis[SPIDER_SDL3_AXIS_LT]=sdl_axis(handle,SPIDER_SDL3_AXIS_LT);
  sample->axis[SPIDER_SDL3_AXIS_RT]=sdl_axis(handle,SPIDER_SDL3_AXIS_RT);
  if(instance_id)*instance_id=id;
  if(vid)*vid=sdl_vendor(id);
  if(pid)*pid=sdl_product(id);
  text_name=sdl_name(id);
  if(name && capacity)
    snprintf(name,capacity,"%s",text_name?text_name:"Mapped SDL3 gamepad");
  return 1;
}
/* Explicit logical SDL3 selection for unknown native DirectInput layouts
 * and, in universal opt-in mode, ALL uniquely identified mapped devices.
 * This is NOT an alias of SelectedIdentity(): no native DI ownership.
 *
 * Caller must be on the game/Flip thread, with SDL3 already pumped and
 * the exact selected instance still connected and unique by VID:PID.
 * Returning a sample never produces a keyboard event or changes DI. */
int SpiderSDL3Probe_GenericSelectedSample(uint32_t *instance,
    uint16_t *vid,uint16_t *pid,SpiderSDL3ControllerSample *sample) {
  SpiderSDL3Identity opened[SPIDER_SDL3_MAX_GAMEPADS]={{0}};
  unsigned i,j;
  uint32_t id=generic_selected_id;
  if(instance)*instance=0;
  if(vid)*vid=0;
  if(pid)*pid=0;
  if(sample)memset(sample,0,sizeof(*sample));
  if(!instance || !vid || !pid || !sample ||
     !ready || !scanned || !sdl3_game_thread ||
     sdl3_game_thread!=GetCurrentThreadId() ||
     !sdl_vendor || !sdl_product || !sdl_axis || !sdl_button ||
     !sdl_connected || !id || !generic_selected_vendor ||
     !generic_selected_product)return 0;
  /* Outside universal mode, retain the legacy native Sony/Xbox owner
   * isolation. Opt-in universal mode instead routes all mapped pads
   * through ONE keyboard source, with native rebind disabled. */
  if(!universal_requested &&
     spider_reconnect_kind(generic_selected_vendor,
                           generic_selected_product)!=SPIDER_RECONNECT_NONE)
    return 0;
  for(i=0;i<num_pads && i<SPIDER_SDL3_MAX_GAMEPADS;i++) {
    if(!pads[i].handle || !pads[i].id ||
       !sdl_connected(pads[i].handle))continue;
    opened[i].instance_id=pads[i].id;
    opened[i].vendor=sdl_vendor(pads[i].id);
    opened[i].product=sdl_product(pads[i].id);
  }
  if(!spider_sdl3_selected_model_unique(opened,num_pads,id,
       generic_selected_vendor,generic_selected_product))return 0;
  for(i=0;i<num_pads;i++)
    if(pads[i].id==id && pads[i].handle &&
       sdl_connected(pads[i].handle))break;
  if(i>=num_pads)return 0;
  for(j=0;j<SPIDER_SDL3_BUTTON_COUNT;j++)
    if(sdl_button(pads[i].handle,(int)j))sample->buttons|=1u<<j;
  for(j=0;j<SPIDER_SDL3_AXIS_COUNT;j++)
    sample->axis[j]=sdl_axis(pads[i].handle,(int)j);
  if(sample->axis[SPIDER_SDL3_AXIS_LT]<0 ||
     sample->axis[SPIDER_SDL3_AXIS_RT]<0)return 0;
  *instance=id;
  *vid=generic_selected_vendor;
  *pid=generic_selected_product;
  return 1;
}
/* SDL3 exposes each trigger as a separate 0..32767 axis. The
 * legacy XUSB DirectInput driver may instead combine LT and RT on lZ,
 * which varies with a device rebind and driver. Read the ALREADY-open,
 * SELECTED SDL3 handle only, on the original game/Flip thread.
 * This never updates SDL, polls the game device or consumes events. */
int SpiderSDL3Probe_SelectedRT(int *normalized_rt) {
  unsigned i;
  uint32_t selected;
  int axis;
  if(normalized_rt)*normalized_rt=0;
  if(!normalized_rt || !SpiderSDL3Probe_NativeOwnerEnabled() ||
     !selection.active || !sdl_axis)return 0;
  selected=selection.active;
  for(i=0;i<num_pads;i++) {
    if(pads[i].id!=selected || !pads[i].handle)continue;
    axis=(int)sdl_axis(pads[i].handle,SPIDER_SDL_AXIS_RIGHT_TRIGGER);
    if(axis<0 || axis>32767)return 0;
    *normalized_rt=axis;
    return 1;
  }
  return 0;
}
int SpiderSDL3Probe_SelectedSample(uint32_t *instance,uint16_t *vid,
                                  uint16_t *pid,
                                  SpiderSDL3ControllerSample *sample) {
  unsigned i,j;
  uint32_t id=0;
  uint16_t v=0,p=0;
  if(instance)*instance=0;
  if(vid)*vid=0;
  if(pid)*pid=0;
  if(sample)memset(sample,0,sizeof(*sample));
  if(!instance || !vid || !pid || !sample ||
     !ready || !scanned || !sdl3_game_thread ||
     sdl3_game_thread!=GetCurrentThreadId() ||
     !sdl_axis || !sdl_button ||
     !SpiderSDL3Probe_SelectedIdentity(&v,&p,&id))return 0;
  for(i=0;i<num_pads;i++)
    if(pads[i].id==id && pads[i].handle)break;
  if(i==num_pads)return 0;
  for(j=0;j<SPIDER_SDL3_BUTTON_COUNT;j++)
    if(sdl_button(pads[i].handle,(int)j))sample->buttons|=1u<<j;
  /* Native owner verification remains unchanged; add normalized axes. */
  for(j=0;j<SPIDER_SDL3_AXIS_COUNT;j++)
    sample->axis[j]=sdl_axis(pads[i].handle,(int)j);
  if(sample->axis[SPIDER_SDL3_AXIS_LT]<0 ||
     sample->axis[SPIDER_SDL3_AXIS_RT]<0)return 0;
  *instance=id;*vid=v;*pid=p;
  return 1;
}
/* Universal SDL3 selection is separate from game-owned DirectInput.
 * Require an EXPLICIT Start (or validated modal handoff) for universal
 * keyboard injection, even if native selection has auto-selected the
 * only connected controller. When detached, a stale instance still
 * fails GenericSelectedSample until the next explicit Start.
 *
 * Never fall back to SelectedSample(): native auto-selection is passive
 * and must NEVER authorize keyboard events or native DI suspension. */
int SpiderSDL3Probe_SelectedMappedSample(uint32_t *instance,
    uint16_t *vid,uint16_t *pid,SpiderSDL3ControllerSample *sample) {
  if(instance)*instance=0;
  if(vid)*vid=0;
  if(pid)*pid=0;
  if(sample)memset(sample,0,sizeof(*sample));
  if(!universal_requested || !instance || !vid || !pid || !sample ||
      !sdl3_game_thread || sdl3_game_thread!=GetCurrentThreadId() ||
      !generic_selected_id)
    return 0;
  return SpiderSDL3Probe_GenericSelectedSample(instance,vid,pid,sample);
}
/* The player may hold physical Cross/A after skipping the New Game Bink.
 * This is ALSO the native 3D Jump/scene-skip button. The preceding
 * synthetic Return KEYUP cannot release a real gamepad button.
 * At the verified movie #4 -> first native 3D input owner, read ONLY the
 * already-open selected SDL3 SOUTH button (SDL_GAMEPAD_BUTTON_SOUTH=0).
 * SDL updates and the game's DirectInput read stay on the same thread;
 * native cutscene/joystick state is not edited or artificially released.
 * A 5s watchdog prevents an indefinitely blocked engine with a stuck
 * button. An actual timeout remains a reported unprotected edge case. */
int SpiderSDL3Probe_NewGameBinkWaitSouthRelease(HWND hwnd) {
  uint32_t id=0,live_id=0;
  uint16_t vid=0,pid=0,live_vid=0,live_pid=0;
  unsigned i;
  void *pad=NULL;
  DWORD started,elapsed;
  char line[240];
  if(!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) ||
     GetForegroundWindow()!=hwnd || !ready || !scanned ||
     !sdl_pump || !sdl_update || !sdl_button || !sdl_connected ||
     !sdl3_game_thread || sdl3_game_thread!=GetCurrentThreadId() ||
     !SpiderSDL3Probe_NativeOwnerEnabled() ||
     !SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id) ||
     !id || !SpiderNativeReconnect_SDL3SelectionReady(vid,pid))return 0;
  if(!((vid==0x054cu &&
        (pid==0x05c4u || pid==0x09ccu || pid==0x0ba0u)) ||
       (vid==0x045eu && pid==0x028eu)))return 0;
  for(i=0;i<num_pads;i++)
    if(pads[i].id==id && pads[i].handle &&
       sdl_connected(pads[i].handle)) {pad=pads[i].handle;break;}
  if(!pad)return 0;
  /* Unlike SelectedSample (which reads the last on-Flip state), a
   * synchronous native input handoff must refresh physical SDL state. */
  sdl_pump();sdl_update();
  if(!sdl_connected(pad))return -1;
  if(!sdl_button(pad,0))return 0;
  snprintf(line,sizeof(line),
    "SDL3-BINK-4-SOUTH: HELD at first 3D owner id=%lu VID:PID=%04X:%04X; waiting for physical Cross/A release (max 5000ms; game input untouched)",
    (unsigned long)id,(unsigned)vid,(unsigned)pid);
  say(line);
  started=GetTickCount();
  for(;;) {
    if(!IsWindow(hwnd) || !IsWindowVisible(hwnd) ||
       GetForegroundWindow()!=hwnd ||
       !SpiderSDL3Probe_SelectedIdentity(&live_vid,&live_pid,&live_id) ||
       live_id!=id || live_vid!=vid || live_pid!=pid ||
       !sdl_connected(pad)) {
      say("SDL3-BINK-4-SOUTH: ABORT owner/device/focus changed; no input mutation");
      return -1;
    }
    Sleep(10);
    sdl_pump();sdl_update();
    if(!sdl_connected(pad)) {
      say("SDL3-BINK-4-SOUTH: ABORT device detached; no input mutation");
      return -1;
    }
    elapsed=(DWORD)(GetTickCount()-started);
    if(!sdl_button(pad,0)) {
      /* Let the original DirectInput driver observe the physical
       * release before the game's very first scripted 3D input poll. */
      Sleep(40);
      snprintf(line,sizeof(line),
        "SDL3-BINK-4-SOUTH: RELEASED after %lu ms; first native 3D input may continue",
        (unsigned long)elapsed);
      say(line);
      return 1;
    }
    if(elapsed>=5000u) {
      say("SDL3-BINK-4-SOUTH: TIMEOUT 5000ms SOUTH still held; unprotected skip possible; game input resumes");
      return -2;
    }
  }
}
/* The original Flip thread is paused synchronously. Pump the selected
 * SDL3 handle on that same thread; never acquire or alter native DirectInput.
 * Foreground plus neutral->press->release are mandatory to resume. */
int SpiderSDL3Probe_PauseWake_Begin(void) {
  uint32_t id=0;
  uint16_t vid=0,pid=0;
  unsigned i;
  SpiderSDL3ControllerSample sample;
  sdl_pause_universal=0;
  sdl_pause_active=0;sdl_pause_id=0;
  sdl_pause_vid=0;sdl_pause_pid=0;sdl_pause_lost_reported=0;
  memset(&sdl_pause_wake,0,sizeof(sdl_pause_wake));
  if(!requested || !ready || !scanned || !sdl_connected ||
     !sdl_button || !sdl_pump || !sdl_update ||
     !sdl3_game_thread || sdl3_game_thread!=GetCurrentThreadId())
    return 0;
  if(universal_requested) {
    /* Opt-in adapter selected the precise SDL3-mapped controller. PS3
     * is not a verified native DI owner, but a digital SDL3 wake
     * requires neither native acquisition nor any injected key. */
    if(!SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample))
      return 0;
    sdl_pause_universal=1;
  } else if(!SpiderSDL3Probe_NativeOwnerEnabled() ||
            !SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id) ||
            !SpiderNativeReconnect_SDL3SelectionReady(vid,pid))
    return 0;
  for(i=0;i<num_pads;i++)
    if(pads[i].id==id && pads[i].handle &&
       sdl_connected(pads[i].handle))break;
  if(i==num_pads)return 0;
  sdl_pause_id=id;sdl_pause_vid=vid;sdl_pause_pid=pid;
  sdl_pause_active=1;
  say(sdl_pause_universal?
    "FOCUS-PAUSE-SDL3: selected universal mapped pad armed (foreground-only; neutral + fresh digital press/release; NO native DirectInput ownership)":
    "FOCUS-PAUSE-SDL3: selected game-owned controller armed (foreground-only; fresh digital press+release; no DirectInput Acquire)");
  return 1;
}
unsigned SpiderSDL3Probe_PauseWake_Poll(HWND hwnd) {
  unsigned i,j,buttons=0;
  uint32_t id=0;
  uint16_t vid=0,pid=0;
  SpiderSDL3ControllerSample sample;
  void *handle=NULL;
  if(!sdl_pause_active)return 0;
  if(!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) ||
     GetForegroundWindow()!=hwnd) {
    memset(&sdl_pause_wake,0,sizeof(sdl_pause_wake));
    return 0;
  }
  if(!sdl3_game_thread || sdl3_game_thread!=GetCurrentThreadId())
    goto lost;
  if(sdl_pause_universal) {
    /* Regular OnFlip is blocked inside this pause loop. After checking
     * foreground, update SDL3 on the same thread and revalidate the
     * chosen instance and VID:PID: no silent fallback to the DS4. */
    sdl_pump();sdl_update();
    if(!SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample) ||
       id!=sdl_pause_id || vid!=sdl_pause_vid || pid!=sdl_pause_pid)
      goto lost;
    for(j=0;j<12u;j++) {
      if(j==SPIDER_SDL3_BUTTON_GUIDE ||
         j==SPIDER_SDL3_BUTTON_START)continue;
      if(sample.buttons&(1u<<j))buttons|=1u<<j;
    }
  } else {
    if(!SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id) ||
       id!=sdl_pause_id || vid!=sdl_pause_vid || pid!=sdl_pause_pid)
      goto lost;
    for(i=0;i<num_pads;i++)
      if(pads[i].id==id && pads[i].handle) {handle=pads[i].handle;break;}
    if(!handle || !sdl_connected(handle))goto lost;
    sdl_pump();sdl_update();
    if(!sdl_connected(handle))goto lost;
    for(j=0;j<12u;j++) {
      if(j==SPIDER_SDL3_BUTTON_GUIDE ||
         j==SPIDER_SDL3_BUTTON_START)continue;
      if(sdl_button(handle,(int)j))buttons|=1u<<j;
    }
  }
  return spider_pause_pad_wake_step(&sdl_pause_wake,buttons);
lost:
  sdl_pause_universal=0;
  sdl_pause_active=0;
  memset(&sdl_pause_wake,0,sizeof(sdl_pause_wake));
  if(!sdl_pause_lost_reported) {
    say("FOCUS-PAUSE-SDL3: selected device detached/changed while paused; wake disabled, keyboard remains available");
    sdl_pause_lost_reported=1;
  }
  return 0;
}
void SpiderSDL3Probe_PauseWake_End(void) {
  sdl_pause_universal=0;
  sdl_pause_active=0;sdl_pause_id=0;
  sdl_pause_vid=0;sdl_pause_pid=0;sdl_pause_lost_reported=0;
  memset(&sdl_pause_wake,0,sizeof(sdl_pause_wake));
  spider_sdl3_selection_focus_lost(&selection);
  bindings_trace_armed=0;bindings_previous=0;
}
/* Optional semantic preview for a future on-screen mapping wizard.
 * Read selected SDL3 normalized state from the already-open handle
 * AFTER the once-per-Flip SDL3 event pump. Never poll/change native
 * DirectInput; never forward synthetic game actions.
 * New selection/focus-loss requires neutral inputs before logging actions. */
static void sdl3_binding_trace_on_flip(void) {
  unsigned i,j;
  uint32_t id=selection.active,held=0,fresh,released;
  uint16_t vid=0,pid=0;
  SpiderSDL3ControllerSample sample;
  SpiderSDL3ControllerProfile *profile=NULL;
  char line[250];
  if(!bindings_trace_requested || bindings_trace_lines>=48u)return;
  if(id!=bindings_trace_id) {
    bindings_trace_id=id;
    bindings_previous=0;
    bindings_trace_armed=0;
    if(!id)return;
  }
  if(!id || !sdl_button || !sdl_axis || !sdl_vendor || !sdl_product)return;
  vid=sdl_vendor(id);pid=sdl_product(id);
  if(!vid || !pid)return;
  for(i=0;i<num_pads;i++)if(pads[i].id==id && pads[i].handle)break;
  if(i>=num_pads)return;
  memset(&sample,0,sizeof(sample));
  for(j=0;j<SPIDER_SDL3_BUTTON_COUNT;j++)
    if(sdl_button(pads[i].handle,(int)j))sample.buttons|=1u<<j;
  sample.axis[SPIDER_SDL3_AXIS_LT]=
    sdl_axis(pads[i].handle,SPIDER_SDL3_AXIS_LT);
  sample.axis[SPIDER_SDL3_AXIS_RT]=
    sdl_axis(pads[i].handle,SPIDER_SDL3_AXIS_RT);
  profile=spider_sdl3_bank_get(&bindings_bank,vid,pid);
  if(!profile)return; /* Unknown or exhausted profile bank: no guessing. */
  if(!bindings_trace_armed) {
    if(!spider_sdl3_profile_neutral(&sample))return;
    bindings_trace_armed=1;
    bindings_previous=0;
    snprintf(line,sizeof(line),
      "SDL3-BINDINGS: ARMED selected-id=%lu VID:PID=%04X:%04X standard-semantic-defaults (neutral observed; READ-ONLY)",
      (unsigned long)id,(unsigned)vid,(unsigned)pid);
    say(line);bindings_trace_lines++;
    return;
  }
  if(!spider_sdl3_profile_actions(profile,&sample,
                                   bindings_previous,&held)) {
    bindings_previous=0;
    bindings_trace_armed=0;
    return; /* Bad normalized data; reset, require fresh neutral sample. */
  }
  fresh=held & ~bindings_previous;
  released=bindings_previous & ~held;
  bindings_previous=held;
  if(!fresh && !released)return;
  snprintf(line,sizeof(line),
    "SDL3-BINDINGS: selected-id=%lu action-held=0x%02lX pressed=0x%02lX released=0x%02lX buttons=0x%06lX LT=%d RT=%d [READ-ONLY; NOT GAMEPLAY]",
    (unsigned long)id,(unsigned long)held,
    (unsigned long)fresh,(unsigned long)released,
    (unsigned long)sample.buttons,
    (int)sample.axis[SPIDER_SDL3_AXIS_LT],
    (int)sample.axis[SPIDER_SDL3_AXIS_RT]);
  say(line);
  bindings_trace_lines++;
}
/* Generic 054C:0268 and other SDL-mapped controllers: first prove that
 * deliberate setup selection drives per-model semantic actions correctly,
 * with neutral-first edges and axes, WITHOUT touching original gameplay.
 * A later engine bridge must be independently reviewed and hardware-tested.
 */
static void sdl3_generic_semantic_trace_on_flip(void) {
  uint32_t id=0;
  uint16_t vid=0,pid=0;
  SpiderSDL3ControllerSample sample;
  SpiderSDL3ControllerProfile *profile;
  SpiderSDL3UniversalFrame frame;
  int was_armed,axis_changed=0;
  unsigned i;
  char line[360];
  if(!generic_selected_id || generic_trace_lines>=48u)return;
  if(!SpiderSDL3Probe_GenericSelectedSample(&id,&vid,&pid,&sample)) {
    spider_sdl3_universal_reset(&generic_actions);
    generic_profile_snapshot_valid=0;
    return;
  }
  profile=spider_sdl3_get_profile(vid,pid);
  if(!profile || !spider_sdl3_profile_valid(profile)) {
    spider_sdl3_universal_reset(&generic_actions);
    generic_profile_snapshot_valid=0;
    return;
  }
  if(!generic_profile_snapshot_valid ||
     memcmp(&generic_profile_snapshot,profile,sizeof(*profile))) {
    spider_sdl3_universal_reset(&generic_actions);
    generic_profile_snapshot=*profile;
    generic_profile_snapshot_valid=1;
    generic_last_buttons=0;
    memset(generic_last_axes,0,sizeof(generic_last_axes));
  }
  was_armed=generic_actions.armed!=0;
  frame=spider_sdl3_universal_step(&generic_actions,profile,&sample,
                                 id,1,1);
  if(!frame.valid)return;
  if(!was_armed && generic_actions.armed) {
    snprintf(line,sizeof(line),
      "SDL3-GENERIC: SEMANTIC ARMED id=%lu VID:PID=%04X:%04X (neutral confirmed, READ-ONLY; no DirectInput game ownership)",
      (unsigned long)id,(unsigned)vid,(unsigned)pid);
    say(line);generic_trace_lines++;
    return;
  }
  if(!generic_actions.armed)return;
  for(i=0;i<4u;i++) {
    int delta=(int)sample.axis[i]-generic_last_axes[i];
    if(delta>15000 || delta< -15000)axis_changed=1;
  }
  if(!frame.pressed && !frame.released && !axis_changed)return;
  generic_last_buttons=sample.buttons;
  for(i=0;i<4u;i++)generic_last_axes[i]=sample.axis[i];
  snprintf(line,sizeof(line),
    "SDL3-GENERIC: SEMANTIC id=%lu held=0x%02lX pressed=0x%02lX released=0x%02lX SDL-buttons=0x%06lX LX=%d LY=%d RX=%d RY=%d LT=%d RT=%d [READ-ONLY; original game input unchanged]",
    (unsigned long)id,(unsigned long)frame.held,
    (unsigned long)frame.pressed,(unsigned long)frame.released,
    (unsigned long)generic_last_buttons,
    (int)sample.axis[SPIDER_SDL3_AXIS_LEFTX],
    (int)sample.axis[SPIDER_SDL3_AXIS_LEFTY],
    (int)sample.axis[SPIDER_SDL3_AXIS_RIGHTX],
    (int)sample.axis[SPIDER_SDL3_AXIS_RIGHTY],
    (int)sample.axis[SPIDER_SDL3_AXIS_LT],
    (int)sample.axis[SPIDER_SDL3_AXIS_RT]);
  say(line);generic_trace_lines++;
}
void SpiderSDL3Probe_OnFlip(long flip,HWND game_hwnd) {
  DWORD now;
  unsigned i;
  char line[230];
  /* Discovery starts on the FIRST safe foreground original-game Flip.
   * Delaying until Flip #120 made Start unavailable through most of the
   * opening movie, because Bink can produce fewer Flips than gameplay.
   * Keep same-thread SDL init, foreground checks, neutral-to-Start gate
   * and no asynchronous SDL pumping. The separate SHA-gated native
   * DIERR_UNPLUGGED guard retains its independent Flip #120 safety gate. */
  if(!requested || flip<1)return;
  /* A nonforeground first frame must not pin a transient presentation
   * thread forever. Once SDL3 is initialized, only its owning thread
   * may ever pump, select, or clear a controller's Start-arming state. */
  if(sdl3_game_thread && sdl3_game_thread!=GetCurrentThreadId())return;
  if(!game_hwnd || !IsWindowVisible(game_hwnd) ||
     GetForegroundWindow()!=game_hwnd) {
    /* Never inherit a Start press acquired during the custom focus pause.
     * Selection identity stays stable, but takeover needs neutral->press. */
    if(ready)spider_sdl3_selection_focus_lost(&selection);
    bindings_trace_armed=0;bindings_previous=0;
    generic_profile_snapshot_valid=0;
    spider_sdl3_universal_reset(&generic_actions);
    return;
  }
  if(!sdl3_game_thread)sdl3_game_thread=GetCurrentThreadId();
  if(!attempted) {
    char notice[180];
    snprintf(notice,sizeof(notice),
      "SDL3-PROBE: first safe foreground game Flip #%ld; initializing bundled SDL3 on game thread=%lu",
      flip,(unsigned long)GetCurrentThreadId());
    say(notice);
    if(!sdl_load())return;
  }
  if(!ready)return;
  /* SDL3's event pump and initialization stay on the game/Flip thread. */
  sdl_pump();
  sdl_update();
  now=GetTickCount();
  if(!scanned || (uint32_t)(now-last_scan)>=SPIDER_SDL_SCAN_MS) {
    last_scan=now;
    rescan();
  }
  /* The setup modal never rebinds the game-owned DirectInput device.
   * On the NEXT safe foreground game Flip, validate the requested modal
   * Start's instance and its unique VID:PID before changing SDL identity.
   * Native reconnect subsequently verifies/rebinds the original device
   * using its existing strict single-owner policy (no new COM hooks). */
  if(setup_owner_pending) {
    SpiderSDL3Identity opened[SPIDER_SDL3_MAX_GAMEPADS]={{0}};
    uint32_t requested_id=setup_owner_pending;
    uint16_t vendor=setup_owner_vendor,product=setup_owner_product;
    unsigned j;
    int accepted=0;
    setup_owner_pending=0;
    setup_owner_vendor=0;
    setup_owner_product=0;
    for(j=0;j<num_pads && j<SPIDER_SDL3_MAX_GAMEPADS;j++) {
      if(pads[j].id && pads[j].handle &&
         (!sdl_connected || sdl_connected(pads[j].handle))) {
        opened[j].instance_id=pads[j].id;
        opened[j].vendor=sdl_vendor(pads[j].id);
        opened[j].product=sdl_product(pads[j].id);
      }
    }
    accepted=spider_sdl3_selected_model_unique(opened,num_pads,
                                                requested_id,vendor,product);
    if(accepted &&
       (universal_requested ||
        spider_reconnect_kind(vendor,product)==SPIDER_RECONNECT_NONE)) {
      /* In opt-in universal mode, even a supported native Sony/Xbox
       * model uses the same SDL3 keyboard bridge. Original game-owned
       * DirectInput selection.active and native mappings survive. */
      generic_selected_id=requested_id;
      generic_selected_vendor=vendor;
      generic_selected_product=product;
      generic_profile_snapshot_valid=0;
      spider_sdl3_universal_reset(&generic_actions);
      snprintf(line,sizeof(line),
        "SDL3-GENERIC: SELECTED explicit modal Start id=%lu VID:PID=%04X:%04X; read-only semantic frames, native DirectInput owner unchanged",
        (unsigned long)requested_id,(unsigned)vendor,(unsigned)product);
      say(line);
    } else if(accepted && SpiderSDL3Probe_NativeOwnerEnabled()) {
      /* Selecting a supported native device revokes any previous
       * generic-only selection so no independent source can survive. */
      generic_selected_id=0;generic_selected_vendor=0;
      generic_selected_product=0;generic_profile_snapshot_valid=0;
      spider_sdl3_universal_reset(&generic_actions);
      selection.active=requested_id;
      /* Any stale gameplay Start edges are still disarmed by modal exit.
       * The real native owner will be rebound later in this Flip. */
      snprintf(line,sizeof(line),
        "SDL3-PROBE: SETUP RETURN COMMITTED verified modal owner selected-id=%lu VID:PID=%04X:%04X; native ownership to reconcile",
        (unsigned long)requested_id,(unsigned)vendor,(unsigned)product);
    } else {
      snprintf(line,sizeof(line),
        "SDL3-PROBE: SETUP RETURN REFUSED stale/ambiguous modal owner id=%lu VID:PID=%04X:%04X; previous game and generic owners preserved",
        (unsigned long)requested_id,(unsigned)vendor,(unsigned)product);
      say(line);
    }
  }
  sdl3_trace_native_owner(now);
  for(i=0;i<num_pads;i++)if(pads[i].handle) {
    unsigned vid=(unsigned)sdl_vendor(pads[i].id);
    unsigned pid=(unsigned)sdl_product(pads[i].id);
    int pressed=sdl_button(pads[i].handle,SPIDER_SDL_BUTTON_START)?1:0;
    /* This user-confirmed PS3 appears in BOTH SDL3 and DirectInput.
     * Its per-button DirectInput assignment remains unverified, so log
     * SDL3 logical buttons only, including Start bit 6. This is passive
     * and never polls/acquires the original native device. */
    if(vid==0x054cu && pid==0x0268u && ps3_trace_lines<48u) {
      uint32_t buttons=0;
      unsigned j;
      for(j=0;j<SPIDER_SDL3_BUTTON_COUNT;j++)
        if(sdl_button(pads[i].handle,(int)j))buttons|=1u<<j;
      if(ps3_trace_id!=pads[i].id || buttons!=ps3_trace_buttons) {
        snprintf(line,sizeof(line),
          "SDL3-PS3-PROBE: id=%lu VID:PID=054C:0268 buttons=0x%06lX changed=0x%06lX START=%u [SDL3 ONLY; DirectInput indexes unknown, never forwarded]",
          (unsigned long)pads[i].id,(unsigned long)buttons,
          (unsigned long)(buttons^(ps3_trace_id==pads[i].id?
                                        ps3_trace_buttons:0u)),
          (unsigned)((buttons>>SPIDER_SDL_BUTTON_START)&1u));
        say(line);ps3_trace_lines++;
        ps3_trace_id=pads[i].id;
        ps3_trace_buttons=buttons;
      }
    }
    {
      uint32_t previous=selection.active;
      if(universal_requested?
          spider_sdl3_selection_start_edge(&selection,pads[i].id,pressed):
          spider_sdl3_selection_start(&selection,pads[i].id,pressed)) {
        /* After unplug, selection_sync may auto-select the only DS4.
         * A FRESH Start on that same DS4 must still claim opt-in
         * universal ownership, not be discarded as 'unchanged'. */
        if(universal_requested ||
           spider_reconnect_kind(vid,pid)==SPIDER_RECONNECT_NONE) {
          /* One strict SDL3 normalized keyboard source across all mapped
           * models; never claim native game DI. Reject duplicates. */
          unsigned matches=0,k;
          selection.active=previous;
          for(k=0;k<num_pads;k++)
            if(pads[k].id && pads[k].handle &&
               (!sdl_connected || sdl_connected(pads[k].handle)) &&
               sdl_vendor(pads[k].id)==vid &&
               sdl_product(pads[k].id)==pid)matches++;
          if(universal_requested && vid && pid && matches==1u) {
            generic_selected_id=pads[i].id;
            generic_selected_vendor=(uint16_t)vid;
            generic_selected_product=(uint16_t)pid;
            generic_profile_snapshot_valid=0;
            spider_sdl3_universal_reset(&generic_actions);
            snprintf(line,sizeof(line),
              "SDL3-UNIVERSAL: START selected generic id=%lu VID:PID=%04X:%04X (keyboard bridge only; native DI unchanged)",
              (unsigned long)pads[i].id,vid,pid);
          } else snprintf(line,sizeof(line),
              "SDL3-OWNER: START REFUSED unsupported native controller id=%lu VID:PID=%04X:%04X; previous selected-id=%lu preserved (SDL3 probe only)",
              (unsigned long)pads[i].id,vid,pid,
              (unsigned long)previous);
          say(line);
        } else {
          generic_selected_id=0;generic_selected_vendor=0;
          generic_selected_product=0;generic_profile_snapshot_valid=0;
          spider_sdl3_universal_reset(&generic_actions);
          snprintf(line,sizeof(line),
            "SDL3-PROBE: START selected logical id=%lu VID:PID=%04X:%04X (other gamepads ignored; %s)",
            (unsigned long)selection.active,vid,pid,
            universal_requested?"universal keyboard bridge":"READ-ONLY");
          say(line);
        }
      }
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
  sdl3_binding_trace_on_flip();
  sdl3_generic_semantic_trace_on_flip();
}
