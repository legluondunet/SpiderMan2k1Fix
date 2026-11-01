/* SDL3 generic controller -> original Spider-Man keyboard-action bridge.
 * EXPERIMENTAL and OFF by default. No VID/PID-dependent button arrays.
 *
 * All physical gamepad buttons are read from one SDL3 selected sample, with
 * profile-defined gameplay actions and normalized stick/D-pad directions.
 * The retail game's independently driven DirectInput joystick is temporarily
 * removed from its one game-owned pointer slot to avoid DOUBLE input.
 * This is a borrowed pointer, not a COM Release/Unacquire or a vtable hook.
 * It is restored on focus loss, loss of selected identity or game shutdown.
 *
 * Both hardware and software are strictly game-thread and foreground-only.
 * SendInput keyboard events may not reach every Wine/Windows game keyboard
 * reader: the experiment MUST be hardware validated before default-on.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_universal_gameplay.h"
#include "sdl3_universal_policy.h"
#include "sdl3_gamepad_probe.h"
#include "virtual_spidey_cfg.h"

#define SPIDER_UNIVERSAL_DI_SLOT ((PVOID volatile *)(uintptr_t)0x006B7A2Cu)
#define SPIDER_UNIVERSAL_KEY_TABLE 0x00568398u

static int requested=0,suspended=0,active=0;
static DWORD game_thread=0;
static uint32_t selected_id=0;
static PVOID borrowed_native=NULL;
static unsigned held_scans[SPIDER_UNIVERSAL_KEY_COUNT];
static unsigned sent_mask=0;
static unsigned pulse_scan=0,trace=0;
static unsigned menu_previous=0,menu_arm=0;
static SpiderSDL3UniversalState state;
static void (*report)(const char *)=NULL;
static void log_universal(const char *msg) {
  if(report && trace++<100u)report(msg);
}
static int writable_slot(void) {
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t p=(uintptr_t)SPIDER_UNIVERSAL_DI_SLOT,b;
  DWORD protect;
  if(sizeof(void*)!=4 || (uintptr_t)GetModuleHandleA(NULL)!=0x00400000u ||
     VirtualQuery((const void *)p,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))
    return 0;
  b=(uintptr_t)mbi.BaseAddress;
  protect=mbi.Protect&0xffu;
  return p>=b && p-b<mbi.RegionSize &&
    mbi.RegionSize-(size_t)(p-b)>=sizeof(PVOID) &&
    (protect==PAGE_READWRITE || protect==PAGE_WRITECOPY ||
     protect==PAGE_EXECUTE_READWRITE || protect==PAGE_EXECUTE_WRITECOPY);
}
static int read_scans(unsigned out[SPIDER_UNIVERSAL_KEY_COUNT]) {
  static const uint32_t masks[SPIDER_UNIVERSAL_KEY_COUNT]={
    1u,2u,4u,8u,16u,32u,64u,128u,256u,512u,1024u
  };
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t p=SPIDER_UNIVERSAL_KEY_TABLE,b;
  unsigned i,j;
  DWORD protect;
  if(!SpiderCfg_NativeConfigLoaded() ||
     sizeof(void*)!=4 || (uintptr_t)GetModuleHandleA(NULL)!=0x00400000u ||
     VirtualQuery((const void *)p,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))
    return 0;
  b=(uintptr_t)mbi.BaseAddress;protect=mbi.Protect&0xffu;
  if(p<b || p-b>=mbi.RegionSize ||
     mbi.RegionSize-(size_t)(p-b)<SPIDER_UNIVERSAL_KEY_COUNT*8u ||
     (protect!=PAGE_READONLY && protect!=PAGE_READWRITE &&
      protect!=PAGE_WRITECOPY && protect!=PAGE_EXECUTE_READ &&
      protect!=PAGE_EXECUTE_READWRITE && protect!=PAGE_EXECUTE_WRITECOPY))
    return 0;
  for(i=0;i<SPIDER_UNIVERSAL_KEY_COUNT;i++) {
    uint32_t mask,scan;
    memcpy(&mask,(const void *)(p+8u*i),sizeof(mask));
    memcpy(&scan,(const void *)(p+8u*i+4u),sizeof(scan));
    /* Disallow Escape, Tab, Alt, Ctrl, F10-F12 and ambiguous/unbound
     * values so gameplay keys cannot invoke global menu/system actions. */
    if(mask!=masks[i] || scan<2u || scan>0xD3u ||
       scan==0x0Fu || scan==0x1Cu || scan==0x38u ||
       scan==0x9Du || scan==0xB8u ||
       scan==0x44u || scan==0x57u || scan==0x58u)
      return 0;
    for(j=0;j<i;j++)if(out[j]==scan)return 0;
    out[i]=(unsigned)scan;
  }
  return 1;
}
static int send_scan(unsigned dik,int keyup) {
  INPUT in;
  DWORD flags=KEYEVENTF_SCANCODE|(keyup?KEYEVENTF_KEYUP:0u);
  /* DirectInput DIK uses high-bit E0 for extended arrows and navigation.
   * Win32 SendInput takes raw 7-bit scan plus EXTENDEDKEY separately. */
  if(dik&0x80u)flags|=KEYEVENTF_EXTENDEDKEY;
  memset(&in,0,sizeof(in));
  in.type=INPUT_KEYBOARD;
  in.ki.wScan=(WORD)(dik&0x7fu);
  in.ki.dwFlags=flags;
  return SendInput(1,&in,sizeof(in))==1;
}
static int release_all(void) {
  unsigned i,remaining=0;
  for(i=0;i<SPIDER_UNIVERSAL_KEY_COUNT;i++) {
    if(!(sent_mask&(1u<<i)))continue;
    if(send_scan(held_scans[i],1)) {
      sent_mask&=~(1u<<i);held_scans[i]=0;
    } else remaining++;
  }
  if(pulse_scan) {
    if(send_scan(pulse_scan,1))pulse_scan=0;
    else remaining++;
  }
  if(remaining)log_universal("SDL3-UNIVERSAL: WARNING synthetic key release failed; retry scheduled on next Flip");
  return remaining==0;
}
static int update_keys(unsigned wanted,const unsigned *scans) {
  unsigned i;
  /* Release all no-longer-wanted keys before pressing others. */
  for(i=0;i<SPIDER_UNIVERSAL_KEY_COUNT;i++) {
    if(!(sent_mask&(1u<<i)))continue;
    if((wanted&(1u<<i)) && held_scans[i]==scans[i])continue;
    if(!send_scan(held_scans[i],1))return 0;
    sent_mask&=~(1u<<i);held_scans[i]=0;
  }
  for(i=0;i<SPIDER_UNIVERSAL_KEY_COUNT;i++) {
    if(!(wanted&(1u<<i)) || (sent_mask&(1u<<i)))continue;
    if(!send_scan(scans[i],0))return 0;
    held_scans[i]=scans[i];
    sent_mask|=1u<<i;
  }
  return 1;
}
static void restore_original(const char *reason) {
  PVOID old;
  if(!release_all())return; /* retry; never lose an outstanding KEYUP */
  if(suspended && borrowed_native) {
    old=InterlockedCompareExchangePointer(SPIDER_UNIVERSAL_DI_SLOT,
                                           borrowed_native,NULL);
    if(old!=NULL)
      log_universal("SDL3-UNIVERSAL: WARNING native DI owner changed while suspended; original pointer not blindly overwritten");
    else
      log_universal("SDL3-UNIVERSAL: original game-owned DirectInput pointer RESTORED on exit/focus change");
  }
  suspended=0;active=0;selected_id=0;borrowed_native=NULL;
  sent_mask=0;menu_previous=0;menu_arm=0;
  spider_sdl3_universal_reset(&state);
  (void)reason;
}
static int suspend_original(void) {
  PVOID old;
  if(suspended)return *SPIDER_UNIVERSAL_DI_SLOT==NULL;
  if(!writable_slot())return 0;
  old=InterlockedCompareExchangePointer(SPIDER_UNIVERSAL_DI_SLOT,NULL,NULL);
  if(old && InterlockedCompareExchangePointer(
      SPIDER_UNIVERSAL_DI_SLOT,NULL,old)!=old)return 0;
  borrowed_native=old;
  suspended=1;
  log_universal("SDL3-UNIVERSAL: game-owned native DI pointer safely SUSPENDED; SDL3 keyboard bridge is sole gameplay input source");
  return 1;
}
void SpiderSDL3Universal_Init(const char *ini,void (*logger)(const char *)) {
  requested=0;report=logger;trace=0;game_thread=0;
  active=0;selected_id=0;suspended=0;borrowed_native=NULL;
  memset(held_scans,0,sizeof(held_scans));
  sent_mask=0;pulse_scan=0;menu_previous=menu_arm=0;
  spider_sdl3_universal_reset(&state);
  requested=ini &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",1,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3ControllerSetup",1,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3UniversalGameplay",0,ini)==1;
  if(requested)log_universal(
    "SDL3-UNIVERSAL: OPT-IN preview enabled for ANY mapped SDL3 gamepad; "
    "11 game keyboard actions + native Pause; original DirectInput "
    "suspended only while selected, strict neutral-first and focus release");
}
int SpiderSDL3Universal_Enabled(void) {return requested;}
int SpiderSDL3Universal_SuppressesNative(void) {return requested && suspended;}
void SpiderSDL3Universal_BeforeUI(HWND hwnd) {
  if(!requested)return;
  if(!game_thread)game_thread=GetCurrentThreadId();
  if(game_thread!=GetCurrentThreadId())return;
  /* MUST run before focus pause blocks the original game render thread.
   * Critical: no externally held synthetic keys may survive Alt+Tab. */
  if(!hwnd || !IsWindowVisible(hwnd) || GetForegroundWindow()!=hwnd) {
    restore_original("focus or window lost");
  } else if(pulse_scan && !send_scan(pulse_scan,1)) {
    log_universal("SDL3-UNIVERSAL: warning Pause/Return KEYUP failed, retry before next action");
  } else pulse_scan=0;
}
static int binding_held(SpiderSDL3Binding b,
                        const SpiderSDL3ControllerSample *sample) {
  if(b.kind==SPIDER_SDL3_BIND_BUTTON &&
     b.index<SPIDER_SDL3_BUTTON_COUNT)
    return (sample->buttons&(1u<<b.index))!=0u;
  if(b.kind==SPIDER_SDL3_BIND_TRIGGER &&
     b.index<SPIDER_SDL3_AXIS_COUNT)
    return sample->axis[b.index]>=11000;
  return 0;
}
static void pulse(unsigned scan,const char *context) {
  if(pulse_scan || !send_scan(scan,0))return;
  pulse_scan=scan;
  (void)context;
}
void SpiderSDL3Universal_OnFlip(HWND hwnd,int menu,int gameplay,
                                 int bink,int native_pause) {
  uint32_t id=0,desired=0;
  uint16_t vid=0,pid=0;
  unsigned scans[SPIDER_UNIVERSAL_KEY_COUNT]={0},pause_edge=0;
  SpiderSDL3ControllerSample sample;
  SpiderSDL3ControllerProfile *profile;
  unsigned menu_buttons=0;
  int result;
  if(!requested || game_thread!=GetCurrentThreadId())return;
  if(!hwnd || !IsWindowVisible(hwnd) || GetForegroundWindow()!=hwnd ||
     !SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample)) {
    restore_original("no selected mapped controller");
    return;
  }
  profile=SpiderSDL3Probe_SetupProfile(vid,pid);
  if(!profile || !spider_sdl3_profile_valid(profile) ||
     !writable_slot() || !read_scans(scans)) {
    restore_original("game config or identity not proven");
    return;
  }
  if(active && selected_id!=id)restore_original("SDL3 Start owner changed");
  if(!suspend_original()) {
    restore_original("unsafe native DI pointer");
    return;
  }
  active=1;selected_id=id;
  if(menu || bink || !gameplay) {
    unsigned i;
    /* Context controls are independent from the seven 3D game actions.
     * Keyboard remains available throughout menus and cinematics. */
    spider_sdl3_universal_reset(&state);
    if(!menu_arm) {
      if(spider_sdl3_universal_neutral(&sample))menu_arm=1;
      menu_previous=0;
    } else {
      int confirm=binding_held(
        profile->context[SPIDER_SDL3_CONTEXT_CONFIRM],&sample);
      int back=binding_held(
        profile->context[SPIDER_SDL3_CONTEXT_BACK],&sample);
      if(confirm)menu_buttons|=1u<<4;
      if(back)menu_buttons|=1u<<5;
      for(i=0;i<4u;i++) {
        if(sample.buttons&(1u<<(SPIDER_UNIVERSAL_DPAD_UP+i)))
          menu_buttons|=1u<<i;
      }
      if(sample.axis[SPIDER_SDL3_AXIS_LEFTY]<-12000)menu_buttons|=1u<<0;
      if(sample.axis[SPIDER_SDL3_AXIS_LEFTY]>12000)menu_buttons|=1u<<1;
      if(sample.axis[SPIDER_SDL3_AXIS_LEFTX]<-12000)menu_buttons|=1u<<2;
      if(sample.axis[SPIDER_SDL3_AXIS_LEFTX]>12000)menu_buttons|=1u<<3;
      if((menu_buttons&3u)==3u)menu_buttons&=~3u;
      if((menu_buttons&12u)==12u)menu_buttons&=~12u;
      if((menu_buttons&(1u<<5)) && !(menu_previous&(1u<<5)))
        pulse(0x01u,"menu Back");
      else if((menu_buttons&(1u<<4)) && !(menu_previous&(1u<<4)))
        pulse(0x1cu,"menu Confirm");
    }
    (void)update_keys(menu_buttons&0x0fu,scans);
    menu_previous=menu_buttons;
    return;
  }
  menu_arm=0;menu_previous=0;
  result=spider_sdl3_universal_step(&state,profile,&sample,id,
                                      native_pause,&desired,&pause_edge);
  if(result!=1) {
    (void)update_keys(0u,scans);
    return;
  }
  if(!update_keys(desired,scans)) {
    log_universal("SDL3-UNIVERSAL: KEYDOWN/KEYUP failed; gameplay input suspended, retry pending");
    return;
  }
  if(pause_edge)pulse(0x01u,"3D Pause");
}
