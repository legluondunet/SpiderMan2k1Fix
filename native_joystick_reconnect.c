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

#define SPIDER_NATIVE_BASE 0x00400000u
#define SPIDER_NATIVE_FACTORY 0x006B7A30u
#define SPIDER_NATIVE_DEVICE 0x006B7A2Cu
#define SPIDER_NATIVE_HWND 0x006B7A60u
#define SPIDER_NATIVE_FORMAT 0x0053D0F4u
#define SPIDER_NATIVE_BUTTON_BYTES 0x006B7A34u
#define SPIDER_NATIVE_BUTTON_COUNT 0x006B7A58u
#define SPIDER_NATIVE_POLLED_FLAG 0x006B7928u

static int requested=0,initialized=0,scan_started=0,device_missing=0;
static DWORD last_scan=0, native_thread=0;
static LONG last_events=0;
static unsigned scans=0,rebound=0,failed=0;
static void (*write_log)(const char *)=NULL;

typedef struct SpiderDS4List {
  unsigned count;
  GUID instance;
  DWORD product;
} SpiderDS4List;

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

static BOOL CALLBACK enumerate_ds4(const DIDEVICEINSTANCEA *inst,VOID *ctx) {
  SpiderDS4List *list=(SpiderDS4List *)ctx;
  unsigned vid,pid;
  if(!inst || !list || inst->dwSize<sizeof(DIDEVICEINSTANCEA))return DIENUM_CONTINUE;
  vid=LOWORD(inst->guidProduct.Data1);
  pid=HIWORD(inst->guidProduct.Data1);
  if(!spider_reconnect_sony_ds4(vid,pid))return DIENUM_CONTINUE;
  list->count++;
  if(list->count==1u) {
    list->instance=inst->guidInstance;
    list->product=inst->guidProduct.Data1;
  }
  return DIENUM_CONTINUE;
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
  SpiderDS4List list;
  DIDEVCAPS caps;
  HRESULT hr;
  char line[210];
  PVOID previous;
  memset(&list,0,sizeof(list));
  scans++;
  hr=IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL,
             enumerate_ds4,&list,DIEDFL_ATTACHEDONLY);
  if(FAILED(hr) || list.count!=1u) {
    if(scans==1 || scans%10u==0) {
      snprintf(line,sizeof(line),
        "NATIVE-RECONNECT: scan #%u no unique attached Sony DS4 (count=%u hr=0x%08lX); keyboard active",
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
      report("NATIVE-RECONNECT: DS4 candidate not neutral or buffered reads not ready; retry later");
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
  snprintf(line,sizeof(line),
    "NATIVE-RECONNECT: REBOUND original game-owned DirectInput joystick Sony VID=054C PID=%04X; rebound=%u scan=%u buttons=%lu polled=%u; native controls restored (hardware validation pending)",
    HIWORD(list.product),rebound,scans,(unsigned long)caps.dwButtons,
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
void SpiderNativeReconnect_Init(const char *ini,void (*logger)(const char *)) {
  write_log=logger;
  requested=(ini && GetPrivateProfileIntA("Input",
      "ExperimentalNativeJoystickReconnect",0,ini)==1);
  initialized=0;
  if(!requested) {report("NATIVE-RECONNECT: OFF by default");return;}
  if(!ini || GetPrivateProfileIntA("Input",
      "ExperimentalNativeJoystickUnplugGuard",0,ini)!=1) {
    requested=0;
    report("NATIVE-RECONNECT: DISABLED, stage-1 native unplug guard must be enabled");
    return;
  }
  initialized=valid_binary_layout();
  if(!initialized) {
    requested=0;
    report("NATIVE-RECONNECT: DISABLED, unsupported original joystick signatures/PE layout");
    return;
  }
  report("NATIVE-RECONNECT: requested experimental game-owned DS4 rebind, only after guard is installed and foreground");
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
