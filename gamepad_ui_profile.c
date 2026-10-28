/*
 * SpiderMan2k1Fix / experimental DirectInput UI profile:
 * - Auto-detect ORIGINAL Sony DualShock 4 VID=054C product IDs 05C4,09CC,
 *   and the 0BA0 original adapter, strictly via DirectInput8 product GUID.
 * - Never modify the game's original DirectInput COM vtables or the native
 *   controller-action tables. All seven original gameplay mappings survive.
 * - In the original 2D UI or Bink sequence, turn a debounced DualShock
 *   CROSS/OPTIONS edge into Enter, and CIRCLE (or optional SHARE) into
 *   Escape, using the same paired scan-code injection.
 * - TRIANGLE already triggers native menu confirmation on the user's game:
 *   leave it alone; sending Escape as well would create conflicting actions.
 *
 * SendInput integration with the original game's mixed native keyboard
 * paths is NOT yet verified under Wine or native Windows. This is a
 * diagnostic candidate. A failed independent Acquire/GetDeviceState
 * disables synthesis safely; never bypasses an original movie time gate.
 */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define COBJMACROS
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "gamepad_ui_profile.h"

typedef HRESULT (WINAPI *SpiderDICreateFn)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
enum {SPIDER_PROFILE_OFF=0,SPIDER_PROFILE_AUTO=1,SPIDER_PROFILE_DUALSHOCK4=2};
enum {SPIDER_CONFIRM_CROSS=1,SPIDER_CONFIRM_OPTIONS=2,SPIDER_CONFIRM_BOTH=3};
enum {SPIDER_BACK_OFF=0,SPIDER_BACK_CIRCLE=1,SPIDER_BACK_SHARE=2};

typedef struct {
  unsigned found,ds4_count;
  GUID ds4_instance;
  DWORD product_id;
  char name[MAX_PATH];
  unsigned short last_vendor,last_product;
  char last_name[MAX_PATH];
} SpiderDIDiscovery;

static int configured=0,profile=SPIDER_PROFILE_AUTO,confirm=SPIDER_CONFIRM_BOTH;
static int back=SPIDER_BACK_CIRCLE;
static int probing=0,detected=0,previous_buttons=0,previous_back=0;
static unsigned active_scan=0;
static LONG sent_enter=0,sent_escape=0;
static unsigned bad_reads=0;
static IDirectInput8A *di=NULL;
static IDirectInputDevice8A *device=NULL;
static HMODULE di_module=NULL;
static void (*log_message)(const char *)=NULL;

static void report(const char *msg) {if(log_message)log_message(msg);}
static int ds4_pid(unsigned vid,unsigned pid) {
  return vid==0x054cu && (pid==0x05c4u || pid==0x09ccu || pid==0x0ba0u);
}
static BOOL CALLBACK enumerate_gamepad(const DIDEVICEINSTANCEA *id,VOID *ctx) {
  SpiderDIDiscovery *d=(SpiderDIDiscovery*)ctx;
  unsigned vid=(unsigned)(id->guidProduct.Data1&0xffffu);
  unsigned pid=(unsigned)((id->guidProduct.Data1>>16)&0xffffu);
  if(d->found<32u)d->found++;
  d->last_vendor=(unsigned short)vid;
  d->last_product=(unsigned short)pid;
  strncpy(d->last_name,id->tszProductName,sizeof(d->last_name)-1);
  d->last_name[sizeof(d->last_name)-1]=0;
  if(ds4_pid(vid,pid)) {
    d->ds4_count++;
    if(d->ds4_count==1u) {
      d->ds4_instance=id->guidInstance;
      d->product_id=(DWORD)pid;
      strncpy(d->name,id->tszProductName,sizeof(d->name)-1);
      d->name[sizeof(d->name)-1]=0;
    }
  }
  return DIENUM_CONTINUE;
}
/* Balance precisely the scan code injected at the previous Flip,
 * including across any focus-loss pause. Never leave Escape held. */
static void release_active_key(void) {
  INPUT in;
  char line[160];
  if(!active_scan)return;
  memset(&in,0,sizeof(in));
  in.type=INPUT_KEYBOARD;
  in.ki.wScan=(WORD)active_scan;
  in.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;
  if(SendInput(1,&in,sizeof(in))!=1) {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: WARNING synthetic KEYUP failed scan=0x%02X Win32=%lu",
      active_scan,(unsigned long)GetLastError());
    report(line);
  }
  active_scan=0;
}
static void begin_scancode(unsigned scan,const char *action) {
  INPUT in;
  char line[185];
  LONG number;
  memset(&in,0,sizeof(in));
  in.type=INPUT_KEYBOARD;
  in.ki.wScan=(WORD)scan;
  in.ki.dwFlags=KEYEVENTF_SCANCODE;
  if(SendInput(1,&in,sizeof(in))!=1) {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: WARNING %s KEYDOWN SendInput failed Win32=%lu",
      action,(unsigned long)GetLastError());
    report(line);
    return;
  }
  active_scan=scan;
  number=scan==0x01u?++sent_escape:++sent_enter;
  if(number<=20) {
    snprintf(line,sizeof(line),
      "GAMEPAD-UI: SYNTHETIC %s (DIK 0x%02X) down #%ld; release next Flip; 2D scope only",
      action,scan,(long)number);
    report(line);
  }
}
static void disable_profile(const char *reason) {
  release_active_key();
  if(device) {IDirectInputDevice8_Unacquire(device);
              IDirectInputDevice8_Release(device);device=NULL;}
  if(di) {IDirectInput8_Release(di);di=NULL;}
  if(di_module) {FreeLibrary(di_module);di_module=NULL;}
  detected=0;
  report(reason);
}
void SpiderGamepadUI_Init(const char *ini,void (*logger)(const char *)) {
  char choice[40],buttons[40],back_choice[40],line[230];
  log_message=logger;
  configured=1;
  if(!ini||!ini[0]) {
    profile=SPIDER_PROFILE_OFF;
    report("GAMEPAD-UI: OFF no configuration path");
    return;
  }
  GetPrivateProfileStringA("Input","ControllerProfile","Auto",choice,sizeof(choice),ini);
  if(!_stricmp(choice,"Off"))profile=SPIDER_PROFILE_OFF;
  else if(!_stricmp(choice,"Auto"))profile=SPIDER_PROFILE_AUTO;
  else if(!_stricmp(choice,"DualShock4"))profile=SPIDER_PROFILE_DUALSHOCK4;
  else {profile=SPIDER_PROFILE_OFF;report("GAMEPAD-UI: OFF invalid ControllerProfile");}
  GetPrivateProfileStringA("Input","ControllerEnterButtons","Both",buttons,sizeof(buttons),ini);
  if(!_stricmp(buttons,"Cross"))confirm=SPIDER_CONFIRM_CROSS;
  else if(!_stricmp(buttons,"Options"))confirm=SPIDER_CONFIRM_OPTIONS;
  else if(!_stricmp(buttons,"Both"))confirm=SPIDER_CONFIRM_BOTH;
  else {confirm=SPIDER_CONFIRM_BOTH;report("GAMEPAD-UI: invalid ControllerEnterButtons; using Both");}
  GetPrivateProfileStringA("Input","ControllerBackButton","Circle",
                           back_choice,sizeof(back_choice),ini);
  if(!_stricmp(back_choice,"Off"))back=SPIDER_BACK_OFF;
  else if(!_stricmp(back_choice,"Circle"))back=SPIDER_BACK_CIRCLE;
  else if(!_stricmp(back_choice,"Share"))back=SPIDER_BACK_SHARE;
  else {back=SPIDER_BACK_CIRCLE;report("GAMEPAD-UI: invalid ControllerBackButton; using Circle");}
  if(GetPrivateProfileIntA("Input","ExperimentalGamepadStart",-1,ini)!=-1 ||
     GetPrivateProfileStringA("Input","ExperimentalGamepadStart","",choice,sizeof(choice),ini)!=0)
    report("GAMEPAD-UI: old ExperimentalGamepadStart is RETIRED/ignored; remove BUTTON8 setting");
  snprintf(line,sizeof(line),
    "GAMEPAD-UI: config profile=%s confirm=%s back=%s Return=DIK_0x1C Escape=DIK_0x01 (only 2D)",
    profile==SPIDER_PROFILE_OFF?"Off":profile==SPIDER_PROFILE_AUTO?"Auto":"DualShock4",
    confirm==SPIDER_CONFIRM_CROSS?"Cross":confirm==SPIDER_CONFIRM_OPTIONS?"Options":"Both",
    back==SPIDER_BACK_OFF?"Off":back==SPIDER_BACK_CIRCLE?"Circle":"Share");
  report(line);
}
static void detect_device(HWND hwnd) {
  SpiderDIDiscovery d;
  SpiderDICreateFn create;
  HRESULT hr;
  char line[300];
  HINSTANCE game=GetModuleHandleA(NULL);
  probing=1;
  if(profile==SPIDER_PROFILE_OFF) {
    report("GAMEPAD-UI: OFF; original gamepad controls untouched");
    return;
  }
  di_module=LoadLibraryA("dinput8.dll");
  create=di_module?(SpiderDICreateFn)GetProcAddress(di_module,"DirectInput8Create"):NULL;
  if(!create) {disable_profile("GAMEPAD-UI: unavailable dinput8.dll enumeration; original controls preserved");return;}
  hr=create(game,DIRECTINPUT_VERSION,&IID_IDirectInput8A,(void**)&di,NULL);
  if(FAILED(hr)||!di) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: DirectInput8Create failed hr=0x%08lX; original controls preserved",(unsigned long)hr);
    disable_profile(line);return;
  }
  memset(&d,0,sizeof(d));
  hr=IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL,enumerate_gamepad,
                               &d,DIEDFL_ATTACHEDONLY);
  if(FAILED(hr)) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: EnumDevices failed hr=0x%08lX; original controls preserved",(unsigned long)hr);
    disable_profile(line);return;
  }
  snprintf(line,sizeof(line),
    "GAMEPAD-UI: DISCOVERY gamepads=%u SonyDualShock4=%u last-product=%04X:%04X last-name=%.110s",
    d.found,d.ds4_count,(unsigned)d.last_vendor,(unsigned)d.last_product,d.last_name);
  report(line);
  /* Never apply a Sony profile to an unidentified or ambiguously selected
   * controller: the original game picks its controller independently. */
  if(d.found!=1u || d.ds4_count!=1u) {
    disable_profile("GAMEPAD-UI: no unique verified DualShock4; auto profile inactive; game mappings unchanged");
    return;
  }
  hr=IDirectInput8_CreateDevice(di,&d.ds4_instance,&device,NULL);
  if(FAILED(hr)||!device) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: CreateDevice failed hr=0x%08lX",(unsigned long)hr);
    disable_profile(line);return;
  }
  hr=IDirectInputDevice8_SetDataFormat(device,&c_dfDIJoystick2);
  if(SUCCEEDED(hr))
    hr=IDirectInputDevice8_SetCooperativeLevel(device,hwnd,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE);
  if(FAILED(hr)) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: device format/cooperative level failed hr=0x%08lX",(unsigned long)hr);
    disable_profile(line);return;
  }
  hr=IDirectInputDevice8_Acquire(device);
  if(FAILED(hr)) {
    snprintf(line,sizeof(line),"GAMEPAD-UI: secondary nonexclusive Acquire failed hr=0x%08lX; original controller untouched",(unsigned long)hr);
    disable_profile(line);return;
  }
  detected=1;
  snprintf(line,sizeof(line),
    "GAMEPAD-UI: PROFILE DualShock4 ACTIVE Sony VID=054C PID=%04lX name=%.110s; Cross=BUTTON1 Options=BUTTON9 -> Return; Circle=BUTTON2 Share=BUTTON8 -> optional Escape (2D only)",
    (unsigned long)d.product_id,d.name);
  report(line);
}
void SpiderGamepadUI_OnFlip(HWND hwnd,int menu_2d,int gameplay_3d) {
  DIJOYSTATE2 state;
  HRESULT hr;
  unsigned pressed,back_pressed;
  int permitted;
  if(!configured || !hwnd || !IsWindow(hwnd))return;
  /* Critical: release injected key before focus pause can block this thread. */
  release_active_key();
  if(GetForegroundWindow()!=hwnd || !IsWindowVisible(hwnd)) {
    previous_buttons=0;
    previous_back=0;
    return;
  }
  if(!probing)detect_device(hwnd);
  if(!detected||!device)return;
  hr=IDirectInputDevice8_Poll(device);
  if(FAILED(hr) && hr!=DI_NOEFFECT) {
    /* Some DirectInput joysticks use implicit polling; read even when the
     * optional Poll returns a harmless non-success status. */
  }
  memset(&state,0,sizeof(state));
  hr=IDirectInputDevice8_GetDeviceState(device,sizeof(state),&state);
  if(FAILED(hr)) {
    if(bad_reads++<4u) {
      char line[155];
      snprintf(line,sizeof(line),
        "GAMEPAD-UI: independent DirectInput read hr=0x%08lX; no Return emitted",
        (unsigned long)hr);
      report(line);
    }
    IDirectInputDevice8_Acquire(device);
    previous_buttons=0;
    previous_back=0;
    return;
  }
  bad_reads=0;
  pressed=(state.rgbButtons[1]&0x80u?SPIDER_CONFIRM_CROSS:0) |
          (state.rgbButtons[9]&0x80u?SPIDER_CONFIRM_OPTIONS:0);
  back_pressed=(state.rgbButtons[2]&0x80u?SPIDER_BACK_CIRCLE:0) |
               (state.rgbButtons[8]&0x80u?SPIDER_BACK_SHARE:0);
  permitted=menu_2d && !gameplay_3d;
  if(permitted) {
    /* Escape wins when two UI buttons are pressed on the same Flip: never
     * inject both contradictory menu actions in one frame. */
    if((back_pressed & (unsigned)back) &&
       !(previous_back & (unsigned)back))
      begin_scancode(0x01u,"Escape");
    else if((pressed & (unsigned)confirm) &&
            !(previous_buttons & (unsigned)confirm))
      begin_scancode(0x1Cu,"Return");
  }
  previous_buttons=(int)pressed;
  previous_back=(int)back_pressed;
}
