/* Spider-Man (PC 2001): opt-in recovery of the ORIGINAL game-owned DirectInput8
 * joystick after a physical Sony DualShock4 unplug/replug.
 *
 * This is stage 2. Stage 1 detects DIERR_UNPLUGGED at the original Acquire
 * call site and safely returns from the native joystick reader. Stage 2
 * reacts to that counter at the already-running game's Flip/render boundary.
 *
 * The original binary has an IDirectInput8A factory at 0x006B7A30 and a
 * joystick IDirectInputDevice8A at 0x006B7A2C. We DO NOT hook the factory or
 * any COM vtable, and DO NOT re-run the original startup enumerator, whose
 * error paths would invoke the game's fatal CRT exit.
 *
 * The replacement uses the GAME'S factory, original static data format
 * at 0x0053D0F4, original cooperative HWND at 0x006B7A60, buffer size
 * 16 events, absolute axis mode, original +/-1000 ranges at offsets 0
 * and 4, and original capability-derived button count/polled flag.
 * A fresh, acquired, neutral DS4 is published to the original game global
 * ONLY after every initialization step succeeds.
 *
 * Defaults OFF. Requires ExperimentalNativeJoystickUnplugGuard=1 too.
 * No background polling; no separate DirectInput8 instance; no synthetic
 * keyboard input; no disk EXE edits; only x86 SHA/signature-checked game.
 */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define COBJMACROS
#include <windows.h>
#include <dinput.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "native_joystick_unplug.h"
#include "native_joystick_reconnect.h"
#include "native_joystick_reconnect_policy.h"
#include "native_joystick_swap_probe.h"
#include "sdl3_gamepad_probe.h"

#define SPIDER_NATIVE_BASE 0x00400000u
#define SPIDER_NATIVE_FACTORY 0x006B7A30u
#define SPIDER_NATIVE_DEVICE 0x006B7A2Cu
#define SPIDER_NATIVE_HWND 0x006B7A60u
#define SPIDER_NATIVE_FORMAT 0x0053D0F4u
#define SPIDER_NATIVE_BUTTON_BYTES 0x006B7A34u
#define SPIDER_NATIVE_BUTTON_COUNT 0x006B7A58u
#define SPIDER_NATIVE_FORCE_FEEDBACK 0x006B7A68u
#define SPIDER_NATIVE_POLLED_FLAG 0x006B7928u
/* SHA-supported, read-only native action/button table (14 x 8 bytes). */
#define SPIDER_NATIVE_ACTION_TABLE 0x00568408u

static int requested=0,initialized=0,scan_started=0,device_missing=0;
static DWORD last_scan=0, native_thread=0;
static LONG last_events=0;
static unsigned scans=0,rebound=0,failed=0;
static int swap_probe_enabled=0,swap_initial_seen=0,swap_mapping_enabled=0;
static unsigned swap_last_kind=0,swap_probe_reports=0;
static unsigned sdl3_owner_attempts=0;
static DWORD sdl3_owner_last_attempt=0;
static DWORD sdl3_owner_last_verify=0;
static uint32_t sdl3_owner_last_verified_id=0;
static unsigned sdl3_owner_last_verified_count=0;
static PVOID sdl3_owner_last_verified_ptr=NULL;
static uint32_t sdl3_last_claimed_id=0;
static DWORD sdl3_owner_log_tick=0;
static void (*write_log)(const char *)=NULL;

typedef struct SpiderPadList {
  unsigned count;
  GUID instance;
  DWORD product;
  unsigned kind;
} SpiderPadList;

/* Constant signatures from the maintainer's SHA-verified retail EXE.
 * Includes native factory EnumDevices, CreateDevice, settings, data
 * format and the null-safe original joystick reader. No installed
 * memory code changes are performed by this module. */
static const BYTE original_enum[5]={0xA1,0x30,0x7A,0x6B,0x00};
static const BYTE original_create[3]={0xFF,0x51,0x0C};
static const BYTE original_joystick_null[7]={
  0xA1,0x2C,0x7A,0x6B,0x00,0x85,0xC0
};
static const BYTE original_format[24]={
  0x18,0x00,0x00,0x00, 0x10,0x00,0x00,0x00,
  0x01,0x00,0x00,0x00, 0x50,0x00,0x00,0x00,
  0x2C,0x00,0x00,0x00, 0xB0,0x77,0x51,0x00
};

static void report(const char *s) {if(write_log)write_log(s);}
static volatile PVOID *game_device_slot(void) {
  return (volatile PVOID *)(uintptr_t)SPIDER_NATIVE_DEVICE;
}
static IDirectInput8A *game_factory(void) {
  return (IDirectInput8A *)(uintptr_t)
    InterlockedCompareExchangePointer((PVOID volatile *)(uintptr_t)SPIDER_NATIVE_FACTORY,NULL,NULL);
}
static int valid_binary_layout(void) {
  if(sizeof(void*)!=4 || (uintptr_t)GetModuleHandleA(NULL)!=SPIDER_NATIVE_BASE ||
     sizeof(DIJOYSTATE)!=0x50u) return 0;
  return memcmp((const void *)(uintptr_t)0x00501890u,original_enum,sizeof(original_enum))==0 &&
         memcmp((const void *)(uintptr_t)0x00501AFBu,original_create,sizeof(original_create))==0 &&
         memcmp((const void *)(uintptr_t)0x00501E6Du,original_joystick_null,sizeof(original_joystick_null))==0 &&
         memcmp((const void *)(uintptr_t)SPIDER_NATIVE_FORMAT,original_format,sizeof(original_format))==0;
}

/* An SDL3 instance ID and a DirectInput instance GUID are different
 * namespaces. Never confuse them; only bridge a UNIQUE, supported
 * vendor/product identity. Two identical VID/PIDs must fail closed. */
typedef struct {
  unsigned short vid,pid;
  unsigned matches;
  GUID instance;
  unsigned kind;
} SpiderSDL3Target;
static BOOL CALLBACK enumerate_sdl3_target(const DIDEVICEINSTANCEA *inst,
                                           VOID *ctx) {
  SpiderSDL3Target *t=(SpiderSDL3Target *)ctx;
  if(!inst || !t || inst->dwSize<sizeof(DIDEVICEINSTANCEA))
    return DIENUM_CONTINUE;
  if(LOWORD(inst->guidProduct.Data1)==t->vid &&
     HIWORD(inst->guidProduct.Data1)==t->pid) {
    t->matches++;
    if(t->matches==1u)t->instance=inst->guidInstance;
  }
  return DIENUM_CONTINUE;
}
static BOOL CALLBACK enumerate_supported_pad(const DIDEVICEINSTANCEA *inst,VOID *ctx) {
  SpiderPadList *list=(SpiderPadList *)ctx;
  unsigned vid,pid,kind;
  if(!inst || !list || inst->dwSize<sizeof(DIDEVICEINSTANCEA))return DIENUM_CONTINUE;
  vid=LOWORD(inst->guidProduct.Data1);
  pid=HIWORD(inst->guidProduct.Data1);
  kind=(unsigned)spider_reconnect_kind(vid,pid);
  if(!kind)return DIENUM_CONTINUE;
  list->count++;
  if(list->count==1u) {
    list->instance=inst->guidInstance;
    list->product=inst->guidProduct.Data1;
    list->kind=kind;
  }
  return DIENUM_CONTINUE;
}
/* Observe the exact original 14-slot game table; never write to it.
 * A fingerprint alone does not prove that hot-editing this structure
 * updates the game's effective action mapping. Keep the actual swap
 * mutation disabled until observed against real physical pad changes. */
static const char *swap_kind_name(unsigned kind) {
  return kind==SPIDER_RECONNECT_DS4?"DS4":
         kind==SPIDER_RECONNECT_XBOX360?"Xbox360":"Unknown";
}
static void observe_native_swap_table(const char *phase,unsigned from_kind,
                                      unsigned to_kind) {
  MEMORY_BASIC_INFORMATION mbi;
  const uintptr_t ptr=(uintptr_t)SPIDER_NATIVE_ACTION_TABLE;
  const uint8_t *table=(const uint8_t *)ptr;
  uint32_t buttons[SPIDER_PS1_BUTTON_COUNT]={0};
  int valid=0;
  unsigned preset=0;
  char msg[335];
  if(!swap_probe_enabled || swap_probe_reports>=24u)return;
  swap_probe_reports++;
  if(sizeof(void*)==4 && (uintptr_t)GetModuleHandleA(NULL)==SPIDER_NATIVE_BASE &&
     VirtualQuery(table,&mbi,sizeof(mbi))==sizeof(mbi) &&
     mbi.State==MEM_COMMIT &&
     !(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
     ((mbi.Protect&0xffu)==PAGE_READONLY ||
      (mbi.Protect&0xffu)==PAGE_READWRITE ||
      (mbi.Protect&0xffu)==PAGE_WRITECOPY ||
      (mbi.Protect&0xffu)==PAGE_EXECUTE_READ ||
      (mbi.Protect&0xffu)==PAGE_EXECUTE_READWRITE ||
      (mbi.Protect&0xffu)==PAGE_EXECUTE_WRITECOPY) &&
     ptr>=(uintptr_t)mbi.BaseAddress &&
     (uintptr_t)(ptr-(uintptr_t)mbi.BaseAddress)<mbi.RegionSize &&
     mbi.RegionSize-(size_t)(ptr-(uintptr_t)mbi.BaseAddress)>=
       SPIDER_NATIVE_SWAP_SLOTS*8u) {
    valid=spider_native_swap_read(table,SPIDER_NATIVE_SWAP_SLOTS*8u,buttons);
  }
  if(valid)preset=spider_native_swap_preset(buttons);
  snprintf(msg,sizeof(msg),
    "NATIVE-SWAP-PROBE: %s hardware=%s->%s native-table=%s preset=%s buttons[7]=%lu,%lu,%lu,%lu,%lu,%lu,%lu [READ-ONLY; runtime action mapping NOT changed]",
    phase?phase:"unknown",swap_kind_name(from_kind),
    swap_kind_name(to_kind),valid?"VALID":"UNVERIFIED",
    valid?(preset?swap_kind_name(preset):"CUSTOM"):"UNKNOWN",
    (unsigned long)buttons[0],(unsigned long)buttons[1],
    (unsigned long)buttons[2],(unsigned long)buttons[3],
    (unsigned long)buttons[4],(unsigned long)buttons[5],
    (unsigned long)buttons[6]);
  report(msg);
  if(valid && to_kind!=0u && preset!=0u && preset!=to_kind)
    report("NATIVE-SWAP-PROBE: PROFILE MISMATCH: device has changed but game still uses prior native action mapping; no unsafe runtime patch attempted");
}
/* Stage 4, explicitly opted in: switch ONLY the native 7 action/button
 * DWORDs in gControllerMappings (0x568408), not the game default table,
 * current keyboard actions, user INI or game executable on disk.
 * The original PCInput.cpp gControllerMappings[0x70] is 14 (mask, button)
 * pairs; original GetControllerMappingForAction reads THIS current table.
 * Only a fully verified known Sony/Xbox canonical preset can be replaced.
 * Customized bindings are never overwritten, and no key is injected.
 * This runs on the native game thread at the foreground Flip boundary,
 * AFTER successful neutral game-owned DirectInput device publication. */
/* Read-only safety gate for RT Swing after a DS4 -> Xbox hot swap.
 * A secondary pad's name alone is NOT proof that gameplay now consumes
 * the Xbox mapping. Require the explicitly opted runtime-swap feature,
 * original native game-owned device pointer, and the exact canonical
 * 14-slot action table already validated by the swap probe. No writes. */
static int spider_native_reconnect_canonical_mapping(unsigned kind) {
  const uintptr_t table=SPIDER_NATIVE_ACTION_TABLE;
  MEMORY_BASIC_INFORMATION mbi;
  uint32_t buttons[SPIDER_PS1_BUTTON_COUNT];
  uintptr_t base;
  if(!requested || !initialized || !swap_mapping_enabled ||
     !SpiderNativeUnplug_Installed() || !native_thread ||
     native_thread!=GetCurrentThreadId() ||
     !InterlockedCompareExchangePointer(
          (PVOID volatile *)game_device_slot(),NULL,NULL) ||
     (uintptr_t)GetModuleHandleA(NULL)!=SPIDER_NATIVE_BASE)return 0;
  if(VirtualQuery((const void *)table,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))
    return 0;
  base=(uintptr_t)mbi.BaseAddress;
  if(table<base || table-base>=mbi.RegionSize ||
     mbi.RegionSize-(size_t)(table-base)<SPIDER_NATIVE_SWAP_SLOTS*8u)
    return 0;
  return spider_native_swap_read((const uint8_t *)table,
             SPIDER_NATIVE_SWAP_SLOTS*8u,buttons) &&
         spider_native_swap_preset(buttons)==kind;
}
int SpiderNativeReconnect_CanonicalXboxMapping(void) {
  return spider_native_reconnect_canonical_mapping(SPIDER_RECONNECT_XBOX360);
}
int SpiderNativeReconnect_CanonicalDS4Mapping(void) {
  return spider_native_reconnect_canonical_mapping(SPIDER_RECONNECT_DS4);
}
static void swap_native_gameplay_mapping(unsigned to_kind) {
  const uintptr_t address=(uintptr_t)SPIDER_NATIVE_ACTION_TABLE;
  const uint32_t *target;
  uint32_t original[SPIDER_PS1_BUTTON_COUNT]={0};
  uint32_t current[SPIDER_PS1_BUTTON_COUNT]={0};
  MEMORY_BASIC_INFORMATION mbi;
  DWORD prot,old,unused;
  unsigned preset,i;
  char line[215];
  if(!swap_mapping_enabled || (to_kind!=SPIDER_RECONNECT_DS4 &&
     to_kind!=SPIDER_RECONNECT_XBOX360))return;
  if(sizeof(void*)!=4 || (uintptr_t)GetModuleHandleA(NULL)!=SPIDER_NATIVE_BASE ||
     VirtualQuery((const void *)address,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
     address<(uintptr_t)mbi.BaseAddress ||
     address-(uintptr_t)mbi.BaseAddress>=mbi.RegionSize ||
     mbi.RegionSize-(size_t)(address-(uintptr_t)mbi.BaseAddress)<0x70u) {
    report("NATIVE-SWAP: REFUSED memory layout/permissions unverified; no mapping changed");
    return;
  }
  prot=mbi.Protect&0xffu;
  if(prot!=PAGE_READWRITE && prot!=PAGE_WRITECOPY) {
    report("NATIVE-SWAP: REFUSED original native action map is not writable data");
    return;
  }
  if(!spider_native_swap_read((const uint8_t *)address,0x70u,current)) {
    report("NATIVE-SWAP: REFUSED 14-entry native action-map integrity check failed");
    return;
  }
  preset=spider_native_swap_preset(current);
  /* When two pads were present at process startup, neither early Sony
   * nor Xbox preview was permitted to rewrite the game config. For this
   * explicit SDL3 owner mode ONLY, recognize the exact original seven
   * factory button indices as a third safe canonical source. Customized
   * layouts still fail closed. */
  if(preset!=SPIDER_RECONNECT_DS4 &&
     preset!=SPIDER_RECONNECT_XBOX360) {
    static const uint32_t factory[SPIDER_PS1_BUTTON_COUNT]=
      {3u,4u,0u,1u,6u,9u,7u};
    if(SpiderSDL3Probe_NativeOwnerEnabled() &&
       memcmp(current,factory,sizeof(factory))==0)
      preset=3u;
    else {
      report("NATIVE-SWAP: REFUSED CUSTOM joystick mapping; preserve personal action bindings");
      return;
    }
  }
  if(preset==to_kind) {
    report("NATIVE-SWAP: profile already matches newly acquired pad; no RAM write");
    return;
  }
  target=to_kind==SPIDER_RECONNECT_DS4?
      spider_ps1_buttons:spider_xbox360_buttons;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)original[i]=current[i];
  if(!VirtualProtect((void *)address,0x70u,PAGE_READWRITE,&old)) {
    report("NATIVE-SWAP: REFUSED cannot protect native mapping table for write");
    return;
  }
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++) {
    uintptr_t offset=8u*(SPIDER_NATIVE_SWAP_FIRST_ACTION+i)+4u;
    memcpy((void *)(address+offset),&target[i],4u);
  }
  if(!spider_native_swap_read((const uint8_t *)address,0x70u,current) ||
     spider_native_swap_preset(current)!=to_kind) {
    for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++) {
      uintptr_t offset=8u*(SPIDER_NATIVE_SWAP_FIRST_ACTION+i)+4u;
      memcpy((void *)(address+offset),&original[i],4u);
    }
    VirtualProtect((void *)address,0x70u,old,&unused);
    report("NATIVE-SWAP: FAILED post-write validation, original 7 bindings restored");
    return;
  }
  if(!VirtualProtect((void *)address,0x70u,old,&unused)) {
    DWORD scratch;
    if(VirtualProtect((void *)address,0x70u,PAGE_READWRITE,&scratch)) {
      for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++) {
        uintptr_t offset=8u*(SPIDER_NATIVE_SWAP_FIRST_ACTION+i)+4u;
        memcpy((void *)(address+offset),&original[i],4u);
      }
      VirtualProtect((void *)address,0x70u,old,&unused);
    }
    report("NATIVE-SWAP: FAILED page-protection restoration; original bindings rollback attempted");
    return;
  }
  snprintf(line,sizeof(line),
    "NATIVE-SWAP: APPLIED game-owned seven-button 3D action profile %s->%s (0x568408 validated; RAM only, INI/keyboard preserved; menu refresh on reopen)",
    preset==3u?"Factory":swap_kind_name(preset),swap_kind_name(to_kind));
  report(line);
}
static void close_candidate(IDirectInputDevice8A *device) {
  if(!device)return;
  /* A newly created candidate may have disappeared between EnumDevices,
   * Acquire and GetDeviceState. Do NOT call Unacquire on that possibly
   * invalidated interface, as in the first disconnect crash. Its final
   * COM Release owns underlying DirectInput teardown. */
  IDirectInputDevice8_Release(device);
}
static HRESULT set_native_range(IDirectInputDevice8A *device,DWORD offset) {
  DIPROPRANGE p;
  memset(&p,0,sizeof(p));
  p.diph.dwSize=sizeof(p);
  p.diph.dwHeaderSize=sizeof(p.diph);
  p.diph.dwObj=offset;
  p.diph.dwHow=DIPH_BYOFFSET;
  p.lMin=-1000;
  p.lMax=1000;
  return IDirectInputDevice8_SetProperty(device,DIPROP_RANGE,&p.diph);
}
static HRESULT configure_candidate(IDirectInputDevice8A *device,HWND hwnd,
                                   DIDEVCAPS *caps) {
  HRESULT hr;
  DIPROPDWORD p;
  if(!device || !caps)return DIERR_INVALIDPARAM;
  memset(caps,0,sizeof(*caps));
  caps->dwSize=sizeof(*caps);
  hr=IDirectInputDevice8_GetCapabilities(device,caps);
  if(FAILED(hr))return hr;
  /* The original native format contains exactly 32 buffered button slots.
   * Refuse a surprising capability report rather than let original
   * button indexing overrun its static array. */
  if(caps->dwButtons==0 || caps->dwButtons>32u)return DIERR_INVALIDPARAM;
  hr=IDirectInputDevice8_SetCooperativeLevel(device,hwnd,
     DISCL_FOREGROUND|DISCL_NONEXCLUSIVE);
  if(FAILED(hr))return hr;
  hr=IDirectInputDevice8_SetDataFormat(device,
    (const DIDATAFORMAT *)(uintptr_t)SPIDER_NATIVE_FORMAT);
  if(FAILED(hr))return hr;
  memset(&p,0,sizeof(p));
  p.diph.dwSize=sizeof(p);
  p.diph.dwHeaderSize=sizeof(p.diph);
  p.diph.dwObj=0;
  p.diph.dwHow=DIPH_DEVICE;
  p.dwData=16;
  hr=IDirectInputDevice8_SetProperty(device,DIPROP_BUFFERSIZE,&p.diph);
  if(FAILED(hr))return hr;
  p.dwData=DIPROPAXISMODE_ABS;
  hr=IDirectInputDevice8_SetProperty(device,DIPROP_AXISMODE,&p.diph);
  if(FAILED(hr))return hr;
  /* Original retail EXE: x86 at 0x005019E0 loads dwObj=0 for lX.
   * At 0x00501A42 dwObj=4 for lY. Offset 16 is lRy and breaks
   * the native left-stick X calibration after hot-rebind. */
  hr=set_native_range(device,0u);
  if(FAILED(hr))return hr;
  hr=set_native_range(device,4u);
  if(FAILED(hr))return hr;
  return IDirectInputDevice8_Acquire(device);
}
static int neutral_and_flush(IDirectInputDevice8A *device) {
  DIJOYSTATE state;
  DIDEVICEOBJECTDATA events[16];
  HRESULT hr;
  unsigned i,pass;
  DWORD count;
  memset(&state,0,sizeof(state));
  hr=IDirectInputDevice8_GetDeviceState(device,sizeof(state),&state);
  if(FAILED(hr))return 0;
  for(i=0;i<32u;i++)if(state.rgbButtons[i]&0x80u)return 0;
  /* The native game uses buffered 20-byte events. Drain all queued
   * press/release edges before publishing, to avoid replaying commands
   * held while the device was being detected. Never flush indefinitely. */
  for(pass=0;pass<8u;pass++) {
    count=16;
    hr=IDirectInputDevice8_GetDeviceData(device,sizeof(DIDEVICEOBJECTDATA),
                                          events,&count,0);
    if(FAILED(hr))return 0;
    if(count<16u)return 1;
  }
  return 0;
}
static void zero_native_buttons(void) {
  memset((void *)(uintptr_t)SPIDER_NATIVE_BUTTON_BYTES,0,32);
}
static void detach_stale_native(void) {
  IDirectInputDevice8A *old;
  PVOID swapped;
  old=(IDirectInputDevice8A *)(uintptr_t)
    InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),NULL,NULL);
  if(!old) {device_missing=1;return;}
  report("NATIVE-RECONNECT: game-owned stale joystick detach BEGIN");
  swapped=InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),NULL,old);
  if(swapped!=old) {
    report("NATIVE-RECONNECT: native pointer changed concurrently; no COM Release");
    return;
  }
  /* The original native reader checks for NULL before Poll. The next
   * native frame therefore keeps keyboard input without futile Acquire
   * loops. The same game render thread performs detach and reassignment.
   * After DIERR_UNPLUGGED, do not Unacquire() an already-lost object. */
  zero_native_buttons();
  IDirectInputDevice8_Release(old);
  device_missing=1;
  scan_started=0;
  report("NATIVE-RECONNECT: game-owned stale joystick released; native pointer NULL; keyboard preserved");
}
static void try_rebind(HWND hwnd) {
  IDirectInput8A *di=game_factory();
  IDirectInputDevice8A *candidate=NULL;
  SpiderPadList list;
  DIDEVCAPS caps;
  HRESULT hr;
  char line[210];
  PVOID previous;
  memset(&list,0,sizeof(list));
  scans++;
  hr=IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL,
             enumerate_supported_pad,&list,DIEDFL_ATTACHEDONLY);
  if(FAILED(hr) || list.count!=1u) {
    if(scans==1 || scans%10u==0) {
      snprintf(line,sizeof(line),
        "NATIVE-RECONNECT: scan #%u no unique supported pad (DS4/Xbox360, count=%u hr=0x%08lX); keyboard active",
        scans,list.count,(unsigned long)hr);
      report(line);
    }
    return;
  }
  hr=IDirectInput8_CreateDevice(di,&list.instance,&candidate,NULL);
  if(FAILED(hr) || !candidate)goto fail;
  hr=configure_candidate(candidate,hwnd,&caps);
  if(FAILED(hr))goto fail;
  if(!neutral_and_flush(candidate)) {
    if(scans==1 || scans%10u==0)
      report("NATIVE-RECONNECT: controller candidate not neutral or buffered reads not ready; retry later");
    close_candidate(candidate);
    return;
  }
  /* The game-owned pointer must still be NULL; never replace another
   * game's newly created device. Do not touch its factory lifetime. */
  previous=InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),
                                              candidate,NULL);
  if(previous) {
    close_candidate(candidate);
    report("NATIVE-RECONNECT: ABORT, native pointer was restored by the game");
    return;
  }
  zero_native_buttons();
  *(volatile DWORD *)(uintptr_t)SPIDER_NATIVE_BUTTON_COUNT=caps.dwButtons;
  *(volatile BYTE *)(uintptr_t)SPIDER_NATIVE_POLLED_FLAG=
    (BYTE)((caps.dwFlags>>8)&1u);
  device_missing=0;
  rebound++;
  observe_native_swap_table("rebound",swap_last_kind,list.kind);
  swap_native_gameplay_mapping(list.kind);
  swap_last_kind=list.kind;
  snprintf(line,sizeof(line),
    "NATIVE-RECONNECT: REBOUND original game-owned DirectInput joystick %s VID=%04X PID=%04X; rebound=%u scan=%u buttons=%lu polled=%u; native controls restored (hardware validation pending)",
    list.kind==SPIDER_RECONNECT_DS4?"Sony DS4":"Xbox360-compatible",
    LOWORD(list.product),HIWORD(list.product),rebound,scans,(unsigned long)caps.dwButtons,
    (unsigned)((caps.dwFlags>>8)&1u));
  report(line);
  return;
fail:
  failed++;
  if(failed==1u || failed%10u==0) {
    snprintf(line,sizeof(line),
      "NATIVE-RECONNECT: candidate Configure/Create failed hr=0x%08lX failures=%u; original game untouched",
      (unsigned long)hr,failed);
    report(line);
  }
  close_candidate(candidate);
}

/* Called on the original game Flip thread only, after SDL event pumping.
 * The original native DirectInput reader sees only the published device.
 * Reuse original factory/configuration and the validated null-pointer path.
 * Never install a COM vtable hook or redirect the keyboard. */
static void sdl3_owner_reconcile(HWND hwnd) {
  IDirectInput8A *di=game_factory();
  IDirectInputDevice8A *old,*candidate=NULL;
  SpiderSDL3Target target;
  DIDEVICEINSTANCEA info;
  DIDEVCAPS caps;
  HRESULT hr;
  uint16_t vid=0,pid=0;
  uint32_t id=0;
  DWORD now=GetTickCount();
  PVOID replaced;
  char line[245];
  if(!di || !swap_mapping_enabled)return;
  /* The game's native FFB effect may still reference the first controller.
   * Refuse ALL detach/swap operations until it is absent; old DI remains
   * untouched instead of releasing a possibly referenced COM device. */
  if(InterlockedCompareExchangePointer(
       (PVOID volatile *)(uintptr_t)SPIDER_NATIVE_FORCE_FEEDBACK,
       NULL,NULL)) {
    if((uint32_t)(now-sdl3_owner_log_tick)>=2000u) {
      report("SDL3-OWNER: REFUSED native force-feedback effect active; original owner retained");
      sdl3_owner_log_tick=now;
    }
    return;
  }
  if(!SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id) ||
     spider_reconnect_kind(vid,pid)==SPIDER_RECONNECT_NONE) {
    /* No valid SDL-selected native owner: keyboard stays usable, but the
     * original game must not keep an arbitrary controller (including
     * an unsupported or unclaimed SDL-mapped device). */
    if(SpiderSDL3Probe_MappedCount()>0u &&
       InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),
                                         NULL,NULL))
      detach_stale_native();
    sdl3_last_claimed_id=0;
    return;
  }
  /* Expensive DirectInput enumeration is bounded even at 30 FPS:
   * recheck every 250ms when the same SDL selection, mapped count and
   * native COM pointer persist. React immediately to Start handoff,
   * hotplug counts, game-native pointer mutation or a missing device. */
  {
    PVOID live=InterlockedCompareExchangePointer(
      (PVOID volatile *)game_device_slot(),NULL,NULL);
    unsigned count=SpiderSDL3Probe_MappedCount();
    if(id==sdl3_owner_last_verified_id &&
       count==sdl3_owner_last_verified_count &&
       live==sdl3_owner_last_verified_ptr &&
       (uint32_t)(now-sdl3_owner_last_verify)<250u)return;
    sdl3_owner_last_verify=now;
    sdl3_owner_last_verified_id=id;
    sdl3_owner_last_verified_count=count;
    sdl3_owner_last_verified_ptr=live;
  }
  memset(&target,0,sizeof(target));
  target.vid=vid;target.pid=pid;
  target.kind=(unsigned)spider_reconnect_kind(vid,pid);
  hr=IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL,
         enumerate_sdl3_target,&target,DIEDFL_ATTACHEDONLY);
  if(FAILED(hr) || target.matches!=1u) {
    if((uint32_t)(now-sdl3_owner_log_tick)>=2000u) {
      snprintf(line,sizeof(line),
        "SDL3-OWNER: REFUSED selected-id=%lu VID:PID=%04X:%04X DirectInput-matches=%u hr=0x%08lX (must be exactly one)",
        (unsigned long)id,vid,pid,target.matches,(unsigned long)hr);
      report(line);sdl3_owner_log_tick=now;
    }
    /* Multiple matching DI instances are not distinguishable by SDL VID/PID. */
    if(InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),
                                         NULL,NULL))
      detach_stale_native();
    return;
  }
  old=(IDirectInputDevice8A *)(uintptr_t)
    InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),
                                      NULL,NULL);
  if(old) {
    memset(&info,0,sizeof(info));info.dwSize=sizeof(info);
    hr=IDirectInputDevice8_GetDeviceInfo(old,&info);
    if(SUCCEEDED(hr) && IsEqualGUID(&info.guidInstance,&target.instance)) {
      /* The original game may have selected the right device at startup
       * before SDL3 existed. Still reconcile its exact factory 7-action
       * table once: a two-pad startup skipped both early preview profiles. */
      if(sdl3_last_claimed_id!=id) {
        observe_native_swap_table("sdl3-initial",swap_last_kind,target.kind);
        swap_native_gameplay_mapping(target.kind);
        swap_last_kind=target.kind;
        sdl3_last_claimed_id=id;
        snprintf(line,sizeof(line),
          "SDL3-OWNER: ALREADY-NATIVE selected-id=%lu VID:PID=%04X:%04X native=%p; checked 3D action mapping",
          (unsigned long)id,vid,pid,(void *)old);
        report(line);
      }
      device_missing=0;
      return; /* Already correct: no extra Acquire/COM teardown. */
    }
    /* Exact native game thread; preserve original NULL-safe reader
     * and release/clear buffered state BEFORE publishing a new owner. */
    detach_stale_native();
  }
  /* Limit creation/Acquire churn if Start is still held or the driver
   * has not settled after hotplug. A native game frame must not open
   * and close a new DirectInput COM device 30 times per second. */
  if(sdl3_owner_attempts && (uint32_t)(now-sdl3_owner_last_attempt)<150u)
    return;
  sdl3_owner_last_attempt=now;
  ++sdl3_owner_attempts;
  hr=IDirectInput8_CreateDevice(di,&target.instance,&candidate,NULL);
  if(FAILED(hr) || !candidate)goto fail;
  hr=configure_candidate(candidate,hwnd,&caps);
  if(FAILED(hr))goto fail;
  if(!neutral_and_flush(candidate)) {
    if(sdl3_owner_attempts==1u || sdl3_owner_attempts%90u==0)
      report("SDL3-OWNER: waiting for selected pad buttons released and buffered events drained");
    close_candidate(candidate);
    return;
  }
  /* A live game-owned force-feedback effect can reference the old device.
   * Never replace its controller while such a native effect exists. */
  if(InterlockedCompareExchangePointer(
       (PVOID volatile *)(uintptr_t)SPIDER_NATIVE_FORCE_FEEDBACK,
       NULL,NULL)) {
    report("SDL3-OWNER: REFUSED native force-feedback effect active; no publish");
    close_candidate(candidate);
    return;
  }
  replaced=InterlockedCompareExchangePointer(
             (PVOID volatile *)game_device_slot(),candidate,NULL);
  if(replaced) {
    report("SDL3-OWNER: ABORT game-owned pointer no longer NULL");
    close_candidate(candidate);
    return;
  }
  zero_native_buttons();
  *(volatile DWORD *)(uintptr_t)SPIDER_NATIVE_BUTTON_COUNT=caps.dwButtons;
  *(volatile BYTE *)(uintptr_t)SPIDER_NATIVE_POLLED_FLAG=
    (BYTE)((caps.dwFlags>>8)&1u);
  device_missing=0;
  rebound++;
  observe_native_swap_table("sdl3-claim",swap_last_kind,target.kind);
  swap_native_gameplay_mapping(target.kind);
  swap_last_kind=target.kind;
  sdl3_last_claimed_id=id;
  snprintf(line,sizeof(line),
    "SDL3-OWNER: REBOUND selected-id=%lu VID:PID=%04X:%04X native=%p buttons=%lu (one game-owned DirectInput device; selection bridged)",
    (unsigned long)id,vid,pid,(void *)candidate,(unsigned long)caps.dwButtons);
  report(line);
  return;
fail:
  if(sdl3_owner_attempts==1u || sdl3_owner_attempts%90u==0) {
    snprintf(line,sizeof(line),
      "SDL3-OWNER: candidate Create/configure failed hr=0x%08lX (retry; native NULL; keyboard preserved)",
      (unsigned long)hr);
    report(line);
  }
  close_candidate(candidate);
}
int SpiderNativeReconnect_SDL3SelectionReady(uint16_t vid,uint16_t pid) {
  IDirectInputDevice8A *current;
  DIDEVICEINSTANCEA info;
  if(!requested || !initialized || !SpiderNativeUnplug_Installed() ||
     !native_thread || native_thread!=GetCurrentThreadId() ||
     !SpiderSDL3Probe_NativeOwnerEnabled())return 0;
  current=(IDirectInputDevice8A *)(uintptr_t)
    InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),
                                      NULL,NULL);
  if(!current)return 0;
  memset(&info,0,sizeof(info));info.dwSize=sizeof(info);
  if(FAILED(IDirectInputDevice8_GetDeviceInfo(current,&info)))return 0;
  return LOWORD(info.guidProduct.Data1)==vid &&
         HIWORD(info.guidProduct.Data1)==pid;
}

/* Observe menu synthetic key transitions after a successful native
 * rebind. No COM method calls and no changes to the original game input.
 * Logging before SendInput is particularly useful if the subsequent
 * native menu handler fails before the next Flip and its regular log. */
void SpiderNativeReconnect_LogSyntheticMenuKey(unsigned scan,const char *phase) {
  /* Reset the bounded forensic window at EVERY native rebind so that
   * rare failures after a second or third physical reconnection are
   * not invisible simply because the first rebind used the quota. */
  static unsigned reported=0,reported_generation=0;
  char line[240];
  PVOID joystick,ffb;
  if(!requested || !initialized || rebound==0)return;
  if(native_thread!=GetCurrentThreadId())return;
  if(reported_generation!=rebound) {
    reported_generation=rebound;
    reported=0;
  }
  if(reported>=40u)return;
  joystick=InterlockedCompareExchangePointer(
    (PVOID volatile *)game_device_slot(),NULL,NULL);
  ffb=InterlockedCompareExchangePointer(
    (PVOID volatile *)(uintptr_t)SPIDER_NATIVE_FORCE_FEEDBACK,NULL,NULL);
  reported++;
  snprintf(line,sizeof(line),
    "NATIVE-RECONNECT: POST-REBIND INPUT #%u scan=0x%02X phase=%s joystick=%p native-ffb=%p rebound=%u unplug-events=%ld [PASSIVE]",
    reported,scan,phase?phase:"unknown",joystick,ffb,rebound,
    (long)SpiderNativeUnplug_Events());
  report(line);
}
/* Passive, bounded mode transitions/heartbeat after a successful
 * rebind. Samples POINTER VALUES ONLY (never dereferences or invokes
 * any COM method) to help correlate future mid-3D input faults with
 * the native device generation, on the original Flip thread. */
void SpiderNativeReconnect_ObserveMode(long flip_index,int menu_2d,
                                       int gameplay_3d,int bink_active) {
  static int last_mode=-1;
  static long last_flip=0;
  static unsigned reports=0,observed_rebound=0;
  char line[240];
  int mode;
  PVOID joystick,ffb;
  if(!requested || !initialized || !SpiderNativeUnplug_Installed() ||
     rebound==0 || native_thread!=GetCurrentThreadId() || reports>=80u)
    return;
  mode=gameplay_3d?2:(bink_active?3:(menu_2d?1:0));
  if(mode==last_mode && observed_rebound==rebound &&
     (flip_index-last_flip)<300)return;
  last_mode=mode;
  observed_rebound=rebound;
  last_flip=flip_index;
  reports++;
  joystick=InterlockedCompareExchangePointer(
    (PVOID volatile *)game_device_slot(),NULL,NULL);
  ffb=InterlockedCompareExchangePointer(
    (PVOID volatile *)(uintptr_t)SPIDER_NATIVE_FORCE_FEEDBACK,NULL,NULL);
  snprintf(line,sizeof(line),
    "NATIVE-RECONNECT: HEALTH #%u flip=%ld mode=%s joystick=%p native-ffb=%p rebound=%u unplug-events=%ld [PASSIVE]",
    reports,flip_index,mode==2?"3D":(mode==3?"BINK":(mode==1?"MENU":"TRANSITION")),
    joystick,ffb,rebound,(long)SpiderNativeUnplug_Events());
  report(line);
}

void SpiderNativeReconnect_Init(const char *ini,void (*logger)(const char *)) {
  write_log=logger;
  requested=(ini && GetPrivateProfileIntA("Input",
      "ExperimentalNativeJoystickReconnect",0,ini)==1);
  initialized=0;
  swap_probe_enabled=0;
  swap_mapping_enabled=0;
  swap_initial_seen=0;
  swap_last_kind=0;
  swap_probe_reports=0;
  if(!requested) {report("NATIVE-RECONNECT: OFF by default");return;}
  if(!ini || GetPrivateProfileIntA("Input",
      "ExperimentalNativeJoystickUnplugGuard",0,ini)!=1) {
    requested=0;
    report("NATIVE-RECONNECT: DISABLED, stage-1 native unplug guard must be enabled");
    return;
  }
  swap_probe_enabled=GetPrivateProfileIntA("Debug",
      "NativeJoystickSwapProbe",0,ini)==1;
  swap_mapping_enabled=GetPrivateProfileIntA("Input",
      "ExperimentalNativeJoystickMappingSwap",0,ini)==1;
  initialized=valid_binary_layout();
  if(!initialized) {
    requested=0;
    report("NATIVE-RECONNECT: DISABLED, unsupported original joystick signatures/PE layout");
    return;
  }
  report("NATIVE-RECONNECT: requested experimental game-owned Sony DS4 / Xbox 360 DirectInput rebind, only after guard is installed and foreground");
  if(swap_probe_enabled)
    report("NATIVE-SWAP-PROBE: requested; fingerprint game-owned 14-slot native mapping before/after rebind; optional explicitly gated runtime swaps");
  if(swap_mapping_enabled)
    report("NATIVE-SWAP: opt-in guarded DS4/Xbox profile switch after successful native reconnect; only canonical 7-action current RAM map, no disk/keyboard writes");
}
void SpiderNativeReconnect_OnFlip(long flip_index, HWND game_hwnd) {
  LONG events;
  DWORD now;
  HWND native_hwnd;
  IDirectInput8A *di;
  PVOID device;
  if(!requested || !initialized || flip_index<120)return;
  if(!SpiderNativeUnplug_Installed())return;
  if(!native_thread)native_thread=GetCurrentThreadId();
  if(native_thread!=GetCurrentThreadId())return;
  native_hwnd=*(HWND volatile *)(uintptr_t)SPIDER_NATIVE_HWND;
  if(!game_hwnd || native_hwnd!=game_hwnd || !IsWindowVisible(game_hwnd) ||
     GetForegroundWindow()!=game_hwnd)return;
  di=game_factory();
  if(!spider_reconnect_eligible(requested,1,1,di!=NULL))return;
  if(swap_probe_enabled && !swap_initial_seen) {
    SpiderPadList initial;
    HRESULT probe_hr;
    memset(&initial,0,sizeof(initial));
    swap_initial_seen=1;
    probe_hr=IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL,
             enumerate_supported_pad,&initial,DIEDFL_ATTACHEDONLY);
    if(SUCCEEDED(probe_hr) && initial.count==1u)
      swap_last_kind=initial.kind;
    observe_native_swap_table("startup",swap_last_kind,swap_last_kind);
  }
  if(SpiderSDL3Probe_NativeOwnerEnabled()) {
    /* Prevent the old one-pad recovery path choosing a random device
     * when SDL3 has multiple mapped controllers. */
    events=SpiderNativeUnplug_Events();
    if(events!=last_events) {
      last_events=events;
      detach_stale_native();
    }
    sdl3_owner_reconcile(native_hwnd);
    return;
  }
  events=SpiderNativeUnplug_Events();
  if(events!=last_events) {
    last_events=events;
    detach_stale_native();
  }
  device=InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),
                                            NULL,NULL);
  if(device || (!device_missing && events>0))return;
  /* A pad that is absent at first startup also gets a bounded scan,
   * but never before the game has built its native factory/HWND. */
  now=GetTickCount();
  if(!spider_reconnect_scan_due(now,last_scan,scan_started))return;
  scan_started=1;
  last_scan=now;
  try_rebind(native_hwnd);
}
