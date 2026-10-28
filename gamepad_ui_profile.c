/*
 * SpiderMan2k1Fix / experimental DirectInput UI profile:
 * - Auto-detect ORIGINAL Sony DualShock 4 VID=054C product IDs 05C4,09CC,
 *   and the 0BA0 original adapter, strictly via DirectInput8 product GUID.
 * - Never modify the game's original DirectInput COM vtables. All seven
 *   original gameplay mappings survive. An OFF-by-default experiment may
 *   unbind only the original Start DWORD after exact signature checks.
 * - In the original 2D UI or Bink sequence, turn a debounced DualShock
 *   CROSS/OPTIONS edge into Enter, and CIRCLE (or optional SHARE) into
 *   Escape, using the same paired scan-code injection.
 * - TRIANGLE triggers native menu confirmation and 3D cinematic skip on
 *   the user's game: do not override it or add a contradictory Escape.
 * - Bink movies can run after leaving the renderer's 2D menu mode.
 * - In 3D, OPTIONS requests Escape/Pause ONLY when the saved seven
 *   native gameplay bindings no longer assign BUTTON9 to an action.
 *   Factory Zipline=BUTTON9 therefore blocks Pause, not gameplay.
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
#include <stdlib.h>
#include "gamepad_ui_profile.h"
#include "ps1_native_start_mapping.h"
#include "ps1_joystick_readonly_menu.h"

typedef HRESULT (WINAPI *SpiderDICreateFn)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
enum {SPIDER_PROFILE_OFF=0,SPIDER_PROFILE_AUTO=1,SPIDER_PROFILE_DUALSHOCK4=2};
enum {SPIDER_CONFIRM_CROSS=1,SPIDER_CONFIRM_OPTIONS=2,SPIDER_CONFIRM_BOTH=3};
enum {SPIDER_BACK_OFF=0,SPIDER_BACK_CIRCLE=1,SPIDER_BACK_SHARE=2};
enum {SPIDER_PAUSE_OFF=0,SPIDER_PAUSE_OPTIONS=1};
static const char *const native_actions[7]={
  "Jump","Kick","Punch","Web","Aim","Zipline","Swing"};
static const unsigned native_factory_buttons[7]={3u,4u,0u,1u,6u,9u,7u};

typedef struct {
  unsigned found,ds4_count;
  GUID ds4_instance;
  DWORD product_id;
  char name[MAX_PATH];
  unsigned short last_vendor,last_product;
  char last_name[MAX_PATH];
} SpiderDIDiscovery;

static int configured=0,profile=SPIDER_PROFILE_AUTO,confirm=SPIDER_CONFIRM_BOTH;
static int back=SPIDER_BACK_CIRCLE,pause_button=SPIDER_PAUSE_OPTIONS;
static int ps1_preview_requested=0,ps1_preview_active=0,previous_share=0;
static int ps1_options_skip_requested=0;
static int ps1_readonly_menu_requested=0,ps1_readonly_menu_active=0;
static int ps1_native_start_unbound=0,previous_native_start=0;
static unsigned ps1_native_start_reports=0;
static unsigned ps1_cutscene_reports=0;
static int probing=0,detected=0,previous_buttons=0,previous_back=0,previous_pause=0;
static unsigned active_scan=0;
static LONG sent_enter=0,sent_escape=0;
static char profile_ini[MAX_PATH]={0};
static unsigned pause_conflict_reports=0,outside_mode_reports=0;
static unsigned postmenu_bink_reports=0;
static unsigned bad_reads=0;
static IDirectInput8A *di=NULL;
static IDirectInputDevice8A *device=NULL;
static HMODULE di_module=NULL;
static void (*log_message)(const char *)=NULL;

static void report(const char *msg) {if(log_message)log_message(msg);}
static int ds4_pid(unsigned vid,unsigned pid) {
  return vid==0x054cu && (pid==0x05c4u || pid==0x09ccu || pid==0x0ba0u);
}
static int parse_native_button(const char *value,unsigned *button) {
  const char *p=value;
  char *end=NULL;
  unsigned long n;
  if(!p || !button)return 0;
  if(_strnicmp(p,"BUTTON",6)==0)p+=6;
  if(!*p || *p=='-' || *p=='+')return 0;
  n=strtoul(p,&end,0);
  if(!end || end==p || *end || n>127ul)return 0;
  *button=(unsigned)n;
  return 1;
}
/* Before mapping OPTIONS to Escape, verify that the original game does
 * not assign BUTTON9 to ANY native gameplay action. A factory install
 * maps Zipline there, so this guard refuses Pause until user rebinds
 * Zipline in the game's Joystick settings (e.g. BUTTON5/R1). */
static int options_free_of_native_actions(void) {
  unsigned i;
  char value[50],fallback[24],line[240];
  if(!profile_ini[0])return 0;
  for(i=0;i<7u;i++) {
    unsigned button;
    snprintf(fallback,sizeof(fallback),"BUTTON%u",native_factory_buttons[i]);
    GetPrivateProfileStringA("Joystick",native_actions[i],fallback,
                             value,sizeof(value),profile_ini);
    if(!parse_native_button(value,&button) || button==9u) {
      if(pause_conflict_reports++<6u) {
        snprintf(line,sizeof(line),
          "GAMEPAD-UI: PAUSE BLOCKED: native [Joystick] %s=%s conflicts with OPTIONS/BUTTON9 or is unknown; rebind in game's Joystick menu, e.g. Zipline to R1/BUTTON5",
          native_actions[i],value);
        report(line);
      }
      return 0;
    }
  }
  return 1;
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
      "GAMEPAD-UI: SYNTHETIC %s (DIK 0x%02X) down #%ld; release next Flip; native menu/Bink/guarded 3D scope",
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
  char choice[40],buttons[40],back_choice[40],pause_choice[40],line[260];
  log_message=logger;
  configured=1;
  if(!ini||!ini[0]) {
    profile=SPIDER_PROFILE_OFF;
    report("GAMEPAD-UI: OFF no configuration path");
    return;
  }
  if(strlen(ini)>=sizeof(profile_ini)) {
    profile=SPIDER_PROFILE_OFF;
    report("GAMEPAD-UI: OFF INI path too long");
    return;
  }
  strcpy(profile_ini,ini);
  /* Experimental only: no automatic fixed-profile activation until native
   * menu lock and Start handling are validated in the real game. */
  ps1_preview_requested=GetPrivateProfileIntA("Input","ControllerPS1Preview",0,ini)==1;
  ps1_options_skip_requested=GetPrivateProfileIntA("Input","ControllerPS1OptionsSkip",0,ini)==1;
  ps1_readonly_menu_requested=GetPrivateProfileIntA("Input","ControllerPS1ReadOnlyJoystick",0,ini)==1;
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
  GetPrivateProfileStringA("Input","ControllerPauseButton","Options",
                           pause_choice,sizeof(pause_choice),ini);
  if(!_stricmp(pause_choice,"Off"))pause_button=SPIDER_PAUSE_OFF;
  else if(!_stricmp(pause_choice,"Options"))pause_button=SPIDER_PAUSE_OPTIONS;
  else {pause_button=SPIDER_PAUSE_OFF;report("GAMEPAD-UI: invalid ControllerPauseButton; disabling pause bridge");}
  if(GetPrivateProfileIntA("Input","ExperimentalGamepadStart",-1,ini)!=-1 ||
     GetPrivateProfileStringA("Input","ExperimentalGamepadStart","",choice,sizeof(choice),ini)!=0)
    report("GAMEPAD-UI: old ExperimentalGamepadStart is RETIRED/ignored; remove BUTTON8 setting");
  snprintf(line,sizeof(line),
    "GAMEPAD-UI: config profile=%s confirm=%s back=%s pause=%s ps1preview=%d options3dskip=%d Return=DIK_0x1C Escape=DIK_0x01",
    profile==SPIDER_PROFILE_OFF?"Off":profile==SPIDER_PROFILE_AUTO?"Auto":"DualShock4",
    confirm==SPIDER_CONFIRM_CROSS?"Cross":confirm==SPIDER_CONFIRM_OPTIONS?"Options":"Both",
    back==SPIDER_BACK_OFF?"Off":back==SPIDER_BACK_CIRCLE?"Circle":"Share",
    pause_button==SPIDER_PAUSE_OFF?"Off":"Options (conflict guarded)",
    ps1_preview_requested,ps1_options_skip_requested);
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
    "GAMEPAD-UI: PROFILE DualShock4 ACTIVE Sony VID=054C PID=%04lX name=%.110s; Cross=BUTTON1 Options=BUTTON9 -> Return in 2D/Bink; Circle=BUTTON2 -> Back in 2D; OPTIONS pause guarded in 3D",
    (unsigned long)d.product_id,d.name);
  report(line);
}
/* This is the ORIGINAL, game-owned CutSceneSkipAllow state machine.
 * The same addresses and per-scene conditions were mapped in the existing
 * F10 diagnostics (quick_exit_f12.c). Never guess 3D from rendering.
 * Memory reads require a readable COMMITTED owner span, and the write
 * uses ONLY the original game skip-request byte after its own permission.
 * Manually opted in (ControllerPS1OptionsSkip=1), not the Auto default.
 * The user's Cross is already mapped to native Jump, which the game
 * uses for 3D cinematic skip; it does NOT need a synthetic key here. */
static int ps1_request_native_options_cutscene_skip(void) {
  const DWORD owner=*(const volatile DWORD *)(uintptr_t)0x006A9038u;
  const volatile BYTE *obj;
  volatile BYTE *native_request=(volatile BYTE *)(uintptr_t)0x0060CFC4u;
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t base,where;
  BYTE scene_context,skip_allowed,script_active;
  DWORD script_ip;
  if(owner<0x00010000u ||
     VirtualQuery((LPCVOID)(uintptr_t)owner,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT ||
     (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
     !((mbi.Protect&0xffu)==PAGE_READONLY ||
       (mbi.Protect&0xffu)==PAGE_READWRITE ||
       (mbi.Protect&0xffu)==PAGE_WRITECOPY ||
       (mbi.Protect&0xffu)==PAGE_EXECUTE_READ ||
       (mbi.Protect&0xffu)==PAGE_EXECUTE_READWRITE ||
       (mbi.Protect&0xffu)==PAGE_EXECUTE_WRITECOPY))return 0;
  base=(uintptr_t)mbi.BaseAddress;
  where=(uintptr_t)owner;
  if(where<base || where-base>mbi.RegionSize ||
     mbi.RegionSize-(where-base)<0x1bcu)return 0;
  obj=(const volatile BYTE *)where;
  scene_context=obj[0x1acu];
  skip_allowed=obj[0x1a4u];
  script_active=obj[0x1b4u];
  script_ip=*(const volatile DWORD *)(where+0x1b8u);
  if(scene_context!=1u || skip_allowed!=1u || script_active!=1u ||
     script_ip==0u || *native_request!=0u)return 0;
  /* The exact game-native 0x4C66A6 writer uses this one-byte request;
   * original 0x4BC3E0 consumes it and cleans up scene resources. */
  *native_request=1u;
  report("GAMEPAD-UI: PS1-3D-OPTIONS native CutSceneSkipAllow=1 request=1 (original 0x4BC3E0 cleanup; NOT forced)");
  return 1;
}
/* Early game HWND acquisition precedes the verified native CRT reload.
 * On failure retry the old unchanged UI discovery on the first Flip. */
int SpiderGamepadUI_PreparePS1(HWND hwnd) {
  if(!configured || !ps1_preview_requested || profile==SPIDER_PROFILE_OFF ||
     !hwnd || !IsWindow(hwnd) || GetForegroundWindow()!=hwnd)return 0;
  if(!probing)detect_device(hwnd);
  if(!detected){
    probing=0;
    report("GAMEPAD-UI: PS1-PREVIEW deferred: early DS4 device acquisition failed; original game mappings untouched");
    return 0;
  }
  report("GAMEPAD-UI: PS1-PREVIEW Sony DS4 acquired before native config bootstrap");
  return 1;
}
/* The retired native BUTTON11 Start remap never reached its table scan:
 * its arbitrary 32 MiB PE SizeOfImage cap rejected the user's retail EXE.
 * Read-only, one-shot replacement: examine verified writable .data pages
 * and record exact 14-entry candidate counts. No mapping writes, no hooks.
 * A positive count is still NOT proof that modifying this mapping is safe. */
/* The normal probe reads at most 1 MiB; the read-only extended
 * scan is manually opted in because this 2001 retail PE maps a 42 MiB
 * writable data section. Query each committed region independently; never
 * cross a region boundary or touch game-owned input mapping bytes.
 *
 * A complete scan could still miss maps stored in another section or
 * synthesized at runtime. Do NOT use the result as permission to patch.
 */
static void ps1_probe_native_start_tables(void) {
  static const DWORD actions[14]={
    0x1u,0x2u,0x4u,0x8u,0x10u,0x20u,0x40u,0x80u,
    0x100u,0x200u,0x400u,0x1000u,0x2000u,0x8000u};
  BYTE *base=(BYTE *)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *pe;
  IMAGE_SECTION_HEADER *sec;
  unsigned j,sections=0,matches=0,truncated=0,regions=0,strict_matches=0;
  BYTE *strict_candidate=NULL;
  int deep=profile_ini[0] &&
    GetPrivateProfileIntA("Input","ControllerPS1DeepStartProbe",0,profile_ini)==1;
  int disable_native_start=profile_ini[0] &&
    GetPrivateProfileIntA("Input","ControllerPS1DisableNativeStart",0,profile_ini)==1;
  const size_t max_bytes=deep?(size_t)(64u*1024u*1024u):(size_t)(1024u*1024u);
  char line[270];
  if(!base || (uintptr_t)base!=0x00400000u)return;
  dos=(IMAGE_DOS_HEADER *)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE ||
     dos->e_lfanew<=0 || dos->e_lfanew>0x1000) {
    report("GAMEPAD-PS1: START-TABLE read-only probe refused invalid DOS header");
    return;
  }
  pe=(IMAGE_NT_HEADERS32 *)(base+dos->e_lfanew);
  if(pe->Signature!=IMAGE_NT_SIGNATURE ||
     pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     pe->OptionalHeader.ImageBase!=0x00400000u ||
     pe->OptionalHeader.SizeOfImage<0x00140000u ||
     pe->OptionalHeader.SizeOfImage>0x10000000u ||
     pe->FileHeader.NumberOfSections<1u ||
     pe->FileHeader.NumberOfSections>32u) {
    snprintf(line,sizeof(line),
      "GAMEPAD-PS1: START-TABLE read-only probe unsupported PE image=0x%08lX sections=%u base=0x%08lX (NO writes)",
      (unsigned long)pe->OptionalHeader.SizeOfImage,
      (unsigned)pe->FileHeader.NumberOfSections,
      (unsigned long)pe->OptionalHeader.ImageBase);
    report(line);
    return;
  }
  sec=IMAGE_FIRST_SECTION(pe);
  for(j=0;j<(unsigned)pe->FileHeader.NumberOfSections;j++) {
    DWORD span=sec[j].Misc.VirtualSize;
    size_t cursor=0,budget;
    BYTE *data;
    if(memcmp(sec[j].Name,".data",5)!=0 ||
       !(sec[j].Characteristics&IMAGE_SCN_MEM_WRITE) ||
       span<14u*8u ||
       sec[j].VirtualAddress>=pe->OptionalHeader.SizeOfImage ||
       span>pe->OptionalHeader.SizeOfImage-sec[j].VirtualAddress)
      continue;
    sections++;
    data=base+sec[j].VirtualAddress;
    budget=span;
    if(budget>max_bytes){budget=max_bytes;truncated++;}
    while(cursor+14u*8u<=budget) {
      MEMORY_BASIC_INFORMATION mbi;
      uintptr_t here=(uintptr_t)(data+cursor),region_start,region_end;
      size_t safe,offset;
      DWORD protect;
      unsigned k;
      if(VirtualQuery((LPCVOID)here,&mbi,sizeof(mbi))!=sizeof(mbi) ||
         !mbi.RegionSize) {
        truncated++;
        break;
      }
      region_start=(uintptr_t)mbi.BaseAddress;
      if(here<region_start || mbi.RegionSize>(size_t)(~(uintptr_t)0)-region_start) {
        truncated++;
        break;
      }
      region_end=region_start+mbi.RegionSize;
      if(region_end<=here){truncated++;break;}
      safe=region_end-here;
      if(safe>budget-cursor)safe=budget-cursor;
      protect=mbi.Protect&0xffu;
      regions++;
      if(mbi.State==MEM_COMMIT &&
         !(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
         (protect==PAGE_READWRITE || protect==PAGE_EXECUTE_READWRITE ||
          protect==PAGE_WRITECOPY || protect==PAGE_EXECUTE_WRITECOPY)) {
        /* All sampled DWORDs must fit within this single readable
         * allocation region. Native tables spanning a page split are
         * intentionally omitted, never accessed speculatively. */
        for(offset=0;offset+14u*8u<=safe;offset+=4u) {
          const DWORD *v=(const DWORD *)(const void *)(data+cursor+offset);
          if(v[0]!=actions[0])continue;
          for(k=1u;k<14u;k++)if(v[2u*k]!=actions[k])break;
          if(k==14u && v[2u*11u+1u]==11u) {
            matches++;
            if(spider_ps1_start_strict_factory((const uint8_t *)(const void *)v,
                                               safe-offset)) {
              strict_matches++;
              if(strict_matches==1u)strict_candidate=(BYTE *)(void *)v;
            }
            if(matches<=10u) {
              snprintf(line,sizeof(line),
                "GAMEPAD-PS1: START-TABLE candidate=%u RVA=0x%08lX Start=BUTTON11 [READ-ONLY; NO table edits]",
                matches,(unsigned long)(sec[j].VirtualAddress+cursor+offset));
              report(line);
            }
          }
        }
      } else {
        truncated++;
      }
      cursor+=safe;
    }
    if(budget>cursor && cursor+14u*8u>budget) {
      /* This last sub-112-byte tail is intentionally skipped. */
    }
  }
  snprintf(line,sizeof(line),
    "GAMEPAD-PS1: START-TABLE probe data-sections=%u candidates=%u strict=%u truncated-regions=%u PE-image=0x%08lX deep=%d queried-regions=%u cap-MiB=%u; diagnostic scan only",
    sections,matches,strict_matches,truncated,(unsigned long)pe->OptionalHeader.SizeOfImage,
    deep,regions,(unsigned)(max_bytes/(1024u*1024u)));
  report(line);
  if(disable_native_start) {
    /* A deliberate separate experiment. Fail closed if default 1 MiB
     * scan was used, any memory was skipped, multiple native patterns
     * were found, or the mapping doesn't match the entire original
     * factory structure. No COM/vtable hooks or PE code patching. */
    if(!deep || truncated || sections!=1u || matches!=1u ||
       strict_matches!=1u || !strict_candidate ||
       strict_candidate!=base+0x00168408u) {
      snprintf(line,sizeof(line),
        "GAMEPAD-PS1: NATIVE-START-UNBIND REFUSED deep=%d sections=%u matches=%u strict=%u truncated=%u (original BUTTON11 retained)",
        deep,sections,matches,strict_matches,truncated);
      report(line);
    } else {
      BYTE *slot=strict_candidate+SPIDER_PS1_START_BUTTON_OFFSET;
      MEMORY_BASIC_INFORMATION mbi;
      uintptr_t here=(uintptr_t)slot,region;
      DWORD prot;
      if((here&3u) ||
         VirtualQuery(slot,&mbi,sizeof(mbi))!=sizeof(mbi) ||
         mbi.State!=MEM_COMMIT ||
         (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))) {
        report("GAMEPAD-PS1: NATIVE-START-UNBIND REFUSED unsafe memory protection/alignment");
      } else {
        region=(uintptr_t)mbi.BaseAddress;
        prot=mbi.Protect&0xffu;
        if(here<region || here-region>=mbi.RegionSize ||
           mbi.RegionSize-(here-region)<sizeof(LONG) ||
           !(prot==PAGE_READWRITE || prot==PAGE_EXECUTE_READWRITE ||
             prot==PAGE_WRITECOPY || prot==PAGE_EXECUTE_WRITECOPY) ||
           !spider_ps1_start_strict_factory((const uint8_t *)strict_candidate,
                                          SPIDER_PS1_START_MAP_BYTES)) {
          report("GAMEPAD-PS1: NATIVE-START-UNBIND REFUSED mapping changed or slot not safely writable");
        } else if(InterlockedCompareExchange(
                    (volatile LONG *)(void *)slot,
                    (LONG)SPIDER_PS1_START_BUTTON_UNBOUND,
                    (LONG)SPIDER_PS1_START_BUTTON_FACTORY)==
                  (LONG)SPIDER_PS1_START_BUTTON_FACTORY) {
          snprintf(line,sizeof(line),
            "GAMEPAD-PS1: NATIVE-START-UNBIND EXPERIMENT ACTIVE RVA=0x%08lX Start BUTTON11 -> native unused 0x4000; one validated RAM dword; game restart restores; SHARE remains menu control",
            (unsigned long)((uintptr_t)slot-(uintptr_t)base));
          ps1_native_start_unbound=1;
          report(line);
        } else {
          report("GAMEPAD-PS1: NATIVE-START-UNBIND REFUSED Start slot modified concurrently");
        }
      }
    }
  }
}
/* Called instead of just the one original joystick-only sprintf at
 * 0x50D28C. The native CMenu::AddEntry that follows keeps ownership of
 * its own text and drawing, so we do not need a custom renderer. */
static int __cdecl spider_ps1_joystick_name(char *destination,
                                            const char *original_format,
                                            unsigned button) {
  const char *name=spider_ps1_native_button_label(button);
  (void)original_format;
  if(!destination)return 0;
  if(name)return snprintf(destination,SPIDER_PS1_JOYSTICK_LABEL_CAPACITY,
                          "%s",name);
  if(button==0x4000u)return snprintf(destination,
    SPIDER_PS1_JOYSTICK_LABEL_CAPACITY,"NONE");
  return snprintf(destination,SPIDER_PS1_JOYSTICK_LABEL_CAPACITY,
                  "button %u",button);
}

/* Native processControllerScreen at 0x50D2F0 has two distinct
 * joystick-assignment branches and an original per-frame cleanup at
 * 0x50D852. Jumping to the original cleanup at the two *post-poll*
 * assignment-entry sites makes controller key presses inert, but does
 * not change the menu's existing normal navigation and Escape/Back.
 *
 * The extra checked NOPs block native restore defaults and the
 * original setter as defense in depth. The 7-entry virtual profile and
 * persistent user mapping are independent of these menu-only edits.
 * All sites are verified BEFORE any code memory is written. */
static int ps1_patch_readonly_site(BYTE *address,const BYTE *replacement,
                                   unsigned n,BYTE *saved) {
  DWORD old,ignored;
  if(!address || !replacement || n<5u || n>6u)return 0;
  if(!VirtualProtect(address,n,PAGE_EXECUTE_READWRITE,&old))return 0;
  memcpy(saved,address,n);
  memcpy(address,replacement,n);
  FlushInstructionCache(GetCurrentProcess(),address,n);
  if(!VirtualProtect(address,n,old,&ignored)){
    /* Even a protection-restore failure must not leave an unknown
     * partial joystick menu patch installed. */
    memcpy(address,saved,n);
    FlushInstructionCache(GetCurrentProcess(),address,n);
    VirtualProtect(address,n,old,&ignored);
    return 0;
  }
  return 1;
}
static int ps1_native_joystick_readonly_install(void) {
  static const BYTE nop5[5]={0x90u,0x90u,0x90u,0x90u,0x90u};
  BYTE replacements[SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT][6]={{0}};
  BYTE backups[SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT][6]={{0}};
  BYTE *base=(BYTE *)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  unsigned i,installed=0;
  char line[220];
  if(!ps1_preview_active || !detected || !ps1_readonly_menu_requested ||
     !base || (uintptr_t)base!=0x00400000u)return 0;
  dos=(IMAGE_DOS_HEADER *)(void *)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE ||
     dos->e_lfanew<0x40 || dos->e_lfanew>0x1000)return 0;
  nt=(IMAGE_NT_HEADERS32 *)(void *)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE ||
     nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     nt->OptionalHeader.ImageBase!=0x00400000u ||
     nt->OptionalHeader.SizeOfImage<0x00110000u ||
     nt->OptionalHeader.SizeOfImage>0x10000000u)return 0;

  /* Verify every exact original machine instruction and writable text
   * window, including the original input branch/callsite signatures.
   * This strict preflight prevents patches on unknown binaries. */
  for(i=0;i<SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT;i++){
    const SpiderPS1JoystickReadOnlySite *p=&spider_ps1_readonly_sites[i];
    const BYTE *location=(const BYTE *)(uintptr_t)p->virtual_address;
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t addr=(uintptr_t)location,region;
    DWORD prot;
    if(p->virtual_address<0x00400000u ||
       p->virtual_address-0x00400000u>nt->OptionalHeader.SizeOfImage-p->size ||
       VirtualQuery(location,&mbi,sizeof(mbi))!=sizeof(mbi) ||
       mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_NOACCESS|PAGE_GUARD))) {
      report("GAMEPAD-PS1: JOYSTICK-READONLY REFUSED invalid native instruction memory");
      return 0;
    }
    prot=mbi.Protect&0xffu;
    region=(uintptr_t)mbi.BaseAddress;
    if(addr<region || addr-region>=mbi.RegionSize ||
       mbi.RegionSize-(addr-region)<p->size ||
       !(prot==PAGE_EXECUTE || prot==PAGE_EXECUTE_READ ||
         prot==PAGE_EXECUTE_READWRITE || prot==PAGE_EXECUTE_WRITECOPY) ||
       memcmp(location,p->original,p->size)!=0) {
      snprintf(line,sizeof(line),
        "GAMEPAD-PS1: JOYSTICK-READONLY REFUSED unexpected original bytes/protection at VA=0x%08lX",
        (unsigned long)p->virtual_address);
      report(line);
      return 0;
    }
  }

  memcpy(replacements[1],nop5,5u); /* native restore-defaults */
  memcpy(replacements[4],nop5,5u); /* native controller action setter */
  replacements[0][0]=0xE8u; /* CALL joystick-only name formatter */
  {
    uint32_t rel=(uint32_t)(uintptr_t)&spider_ps1_joystick_name-
                 (spider_ps1_readonly_sites[0].virtual_address+5u);
    memcpy(&replacements[0][1],&rel,sizeof(rel));
  }
  for(i=2;i<=3u;i++){
    const SpiderPS1JoystickReadOnlySite *p=&spider_ps1_readonly_sites[i];
    uint32_t rel=0x0050D852u-(p->virtual_address+5u);
    replacements[i][0]=0xE9u; /* JMP game's own cleaned return */
    memcpy(&replacements[i][1],&rel,sizeof(rel));
    if(p->size==6u)replacements[i][5]=0x90u;
  }
  for(i=0;i<SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT;i++){
    const SpiderPS1JoystickReadOnlySite *p=&spider_ps1_readonly_sites[i];
    BYTE *location=(BYTE *)(uintptr_t)p->virtual_address;
    if(!ps1_patch_readonly_site(location,replacements[i],p->size,
                               backups[i]))break;
    installed++;
  }
  if(installed!=SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT){
    while(installed>0u){
      const SpiderPS1JoystickReadOnlySite *p=
        &spider_ps1_readonly_sites[--installed];
      BYTE *location=(BYTE *)(uintptr_t)p->virtual_address;
      DWORD old,ignored;
      if(VirtualProtect(location,p->size,PAGE_EXECUTE_READWRITE,&old)){
        memcpy(location,backups[installed],p->size);
        FlushInstructionCache(GetCurrentProcess(),location,p->size);
        VirtualProtect(location,p->size,old,&ignored);
      }
    }
    report("GAMEPAD-PS1: JOYSTICK-READONLY REFUSED install failed; rollback attempted, original menu remains editable");
    return 0;
  }
  ps1_readonly_menu_active=1;
  report("GAMEPAD-PS1: JOYSTICK-READONLY EXPERIMENT ACTIVE verified PCShell joystick-only branches; native menu kept, game rebind/restore calls blocked, CROSS/CIRCLE labels shown; RAM-only, real game validation required");
  return 1;
}
void SpiderGamepadUI_ActivatePS1(void){
  if(!detected || !ps1_preview_requested)return;
  ps1_preview_active=1;
  back=SPIDER_BACK_CIRCLE;
  report("GAMEPAD-UI: PS1-PREVIEW ACTIVE, fixed gameplay RAM only; original Joystick configuration menu NOT YET locked");
  ps1_probe_native_start_tables(); /* read-only scan; optional checked single-DWORD RAM experiment */
  if(ps1_readonly_menu_requested && !ps1_native_joystick_readonly_install())
    report("GAMEPAD-PS1: JOYSTICK-READONLY inactive; exact native signatures missing or failed to install");
}
void SpiderGamepadUI_OnFlip(HWND hwnd,int menu_2d,int gameplay_3d,
                            int bink_active,int native_pause_active) {
  DIJOYSTATE2 state;
  HRESULT hr;
  unsigned pressed,back_pressed;
  int menu_allowed,movie_allowed,confirm_allowed,gameplay_allowed,pause_pressed;
  int share_pressed,share_edge,native_start_pressed;
  if(!configured || !hwnd || !IsWindow(hwnd))return;
  /* Critical: release injected key before focus pause can block this thread. */
  release_active_key();
  if(GetForegroundWindow()!=hwnd || !IsWindowVisible(hwnd)) {
    previous_buttons=0;
    previous_back=0;
    previous_pause=0;
    previous_share=0;
    previous_native_start=0;
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
    previous_pause=0;
    previous_share=0;
    previous_native_start=0;
    return;
  }
  bad_reads=0;
  pressed=(state.rgbButtons[1]&0x80u?SPIDER_CONFIRM_CROSS:0) |
          (state.rgbButtons[9]&0x80u?SPIDER_CONFIRM_OPTIONS:0);
  back_pressed=(state.rgbButtons[2]&0x80u?SPIDER_BACK_CIRCLE:0) |
               (state.rgbButtons[8]&0x80u?SPIDER_BACK_SHARE:0);
  /* Original GameFMV_Active remains authoritative even outside the
   * widescreen menu presentation. 3D triangle/script input is untouched. */
  movie_allowed=bink_active!=0;
  menu_allowed=(menu_2d && !gameplay_3d) ||
    (ps1_preview_active && native_pause_active && !movie_allowed);
  confirm_allowed=menu_allowed || movie_allowed;
  gameplay_allowed=gameplay_3d && !menu_2d && !movie_allowed &&
                   !(ps1_preview_active && native_pause_active);
  pause_pressed=state.rgbButtons[9]&0x80u;
  share_pressed=state.rgbButtons[8]&0x80u;
  share_edge=share_pressed && !previous_share;
  native_start_pressed=(state.rgbButtons[11]&0x80u)!=0;
  /* Passive test of the original BUTTON11/camera press. The native
   * table modification being accepted does not by itself prove which
   * controller table the runtime actually reads. Do NOT synthesize
   * or suppress an input here; record only the physical press edge. */
  if(ps1_preview_active && native_start_pressed && !previous_native_start &&
     ps1_native_start_reports++<12u) {
    char line[210];
    snprintf(line,sizeof(line),
      "GAMEPAD-PS1: ORIGINAL-BUTTON11 PHYSICAL PRESS; Start-RAM-unbound=%d native-pause-current=%d [READ-ONLY; test old button separately from SHARE]",
      ps1_native_start_unbound,native_pause_active!=0);
    report(line);
  }
  if(ps1_preview_active && ps1_options_skip_requested &&
     gameplay_3d && !menu_2d && !movie_allowed && !native_pause_active &&
     pause_pressed && !previous_pause) {
    if(!ps1_request_native_options_cutscene_skip() &&
       ps1_cutscene_reports++<8u)
      report("GAMEPAD-UI: PS1-3D-OPTIONS refused: original CutSceneSkipAllow absent, inactive, unreadable or already requested");
  }
  if(ps1_preview_active && (gameplay_allowed || native_pause_active) &&
     !movie_allowed && share_edge) {
    begin_scancode(0x01u,"Escape");
    if(ps1_native_start_unbound)
      report("GAMEPAD-UI: PS1-PREVIEW SHARE -> Escape during 3D/pause; original Start RAM unbound (runtime behavior still to verify)");
    else
      report("GAMEPAD-UI: PS1-PREVIEW SHARE -> Escape during 3D/pause; original Start mapping retained");
  } else if(menu_allowed && (back_pressed & (unsigned)back) &&
     !(previous_back & (unsigned)back)) {
    begin_scancode(0x01u,"Escape");
  } else if(confirm_allowed &&
            (pressed & (unsigned)confirm) &&
            !(previous_buttons & (unsigned)confirm)) {
    begin_scancode(0x1Cu,"Return");
    if(movie_allowed && !menu_allowed && postmenu_bink_reports++<8u)
      report("GAMEPAD-UI: POST-MENU BINK Return attempted with original GameFMV_Active=1; native video skip gate remains authoritative");
  } else if(!ps1_preview_active && gameplay_allowed && pause_button==SPIDER_PAUSE_OPTIONS &&
            pause_pressed && !previous_pause &&
            options_free_of_native_actions()) {
    begin_scancode(0x01u,"Escape");
    report("GAMEPAD-UI: 3D PAUSE REQUEST OPTIONS -> Escape; no saved native BUTTON9 gameplay assignment");
  } else if(!menu_allowed && !movie_allowed && !gameplay_allowed &&
            (pressed & (unsigned)confirm) &&
            !(previous_buttons & (unsigned)confirm) &&
            outside_mode_reports++<4u) {
    report("GAMEPAD-UI: unclassified engine stage; no keyboard injection");
  }
  previous_buttons=(int)pressed;
  previous_back=(int)back_pressed;
  previous_pause=pause_pressed;
  previous_share=share_pressed;
  previous_native_start=native_start_pressed;
}
