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
#include "input_notacquired_retry.h"

#define SPIDER_UNIVERSAL_DI_SLOT ((PVOID volatile *)(uintptr_t)0x006B7A2Cu)
#define SPIDER_UNIVERSAL_KEY_TABLE 0x00568398u

static int requested=0,suspended=0,active=0;
/* Universal opt-in must not expose the game's legacy joystick merely
 * because the explicitly selected SDL3 controller disconnected. */
static int native_unclaimed_guard=0;
static unsigned native_guard_reports=0;
static DWORD game_thread=0;
static uint32_t selected_id=0;
static PVOID borrowed_native=NULL;
static unsigned held_scans[SPIDER_UNIVERSAL_KEY_COUNT];
static unsigned sent_mask=0;
static unsigned pulse_scan=0,trace=0;
static unsigned key_transition_reports=0,owner_reports=0,refusal_reports=0;
static unsigned menu_previous=0,menu_arm=0;
static unsigned previous_context=SPIDER_UNIVERSAL_CTX_3D;
static unsigned pause_context_reports=0,pause_command_reports=0;
static SpiderSDL3UniversalState state;
static SpiderSDL3UniversalFocusRelease focus_release;
static SpiderSDL3FocusDirectionResync focus_dir;
static uint32_t focus_dir_controller_id=0;
static unsigned focus_latch_follow_scan=0;
static unsigned focus_latch_follow_frames=0;
static unsigned focus_release_reports=0;
/* Native Linux host focus is authoritative when the X11 observer has
 * already been verified. Wine can keep GetForegroundWindow()==game even
 * while Firefox owns real X11 focus. */
static int native_host_focus_blocked=0;
static void (*report)(const char *)=NULL;
static void log_universal(const char *msg) {
  if(report && trace++<100u)report(msg);
}
static void log_focus_release(const char *msg) {
  /* Focus transitions are safety-critical; preserve diagnostics even
   * after the normal 100-line game-action trace budget is exhausted. */
  if(report && focus_release_reports++<24u)report(msg);
}
/* Exact retail PE32: keyboard GetDeviceData writes its own 256-byte
 * buffered key latch at 0x006B792C. This is DISTINCT from the original
 * joystick slot. Read-only diagnostic: never modify game-owned input. */
#define SPIDER_NATIVE_KEY_LATCH 0x006B792Cu
static int read_native_keyboard_latch(unsigned scan,unsigned *value) {
  static const BYTE reader_signature[6]={0x8A,0x98,0x2C,0x79,0x6B,0x00};
  static const BYTE keyup_signature[6]={0x88,0x9F,0x2C,0x79,0x6B,0x00};
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t p,b;
  DWORD prot;
  if(!value || scan>255u || sizeof(void*)!=4 ||
     (uintptr_t)GetModuleHandleA(NULL)!=0x00400000u ||
     memcmp((const void *)(uintptr_t)0x00501C56u,
            reader_signature,sizeof(reader_signature))!=0 ||
     memcmp((const void *)(uintptr_t)0x00501C8Fu,
            keyup_signature,sizeof(keyup_signature))!=0)
    return 0;
  p=(uintptr_t)SPIDER_NATIVE_KEY_LATCH+scan;
  if(VirtualQuery((const void *)p,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))
    return 0;
  b=(uintptr_t)mbi.BaseAddress;
  prot=mbi.Protect&0xffu;
  if(p<b || p-b>=mbi.RegionSize || 
     (prot!=PAGE_READONLY && prot!=PAGE_READWRITE &&
      prot!=PAGE_WRITECOPY && prot!=PAGE_EXECUTE_READ &&
      prot!=PAGE_EXECUTE_READWRITE && prot!=PAGE_EXECUTE_WRITECOPY))
    return 0;
  *value=*(const volatile BYTE *)p;
  return 1;
}
static void trace_native_keyboard_latch(const char *stage,unsigned scan) {
  char msg[210];
  unsigned value;
  if(!stage || !scan || !report)return;
  if(read_native_keyboard_latch(scan,&value)) {
    snprintf(msg,sizeof(msg),
      "SDL3-UNIVERSAL: DI-LATCH stage=%s DIK=0x%02X raw=0x%02X held=%u (original game read-only; action-latch unverified)",
      stage,scan,value,(value&0x7Fu)!=0u);
  } else {
    snprintf(msg,sizeof(msg),
      "SDL3-UNIVERSAL: DI-LATCH stage=%s DIK=0x%02X unavailable (original game unchanged)",
      stage,scan);
  }
  log_focus_release(msg);
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
    /* Never forward a modifier, toggle or global F9 screenshot
     * shortcut. Only a well-formed keyboard action code is safe. */
    if(mask!=masks[i] || !spider_sdl3_universal_scan_safe(scan))
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
  char line[155];
  /* Release all no-longer-wanted keys before pressing others. */
  for(i=0;i<SPIDER_UNIVERSAL_KEY_COUNT;i++) {
    if(!(sent_mask&(1u<<i)))continue;
    if((wanted&(1u<<i)) && held_scans[i]==scans[i])continue;
    if(!send_scan(held_scans[i],1))return 0;
    if(key_transition_reports++<48u) {
      snprintf(line,sizeof(line),
        "SDL3-UNIVERSAL: SendInput KEYUP action=%u DIK=0x%02X (OS accepted, game reception unverified)",
        i,held_scans[i]);
      log_universal(line);
    }
    sent_mask&=~(1u<<i);held_scans[i]=0;
  }
  for(i=0;i<SPIDER_UNIVERSAL_KEY_COUNT;i++) {
    if(!(wanted&(1u<<i)) || (sent_mask&(1u<<i)))continue;
    if(!send_scan(scans[i],0))return 0;
    if(key_transition_reports++<48u) {
      snprintf(line,sizeof(line),
        "SDL3-UNIVERSAL: SendInput KEYDOWN action=%u DIK=0x%02X (OS accepted, game reception unverified)",
        i,scans[i]);
      log_universal(line);
    }
    held_scans[i]=scans[i];
    sent_mask|=1u<<i;
  }
  return 1;
}
static void restore_original(const char *reason) {
  PVOID old;
  if(reason &&
     (!strcmp(reason,"focus or window lost") ||
      !strcmp(reason,"game HWND not foreground"))) {
    unsigned previous=focus_release.frames_remaining;
    /* Capture BEFORE release_all clears OS-acknowledged keys. Wine may
     * fail to transfer those background KEYUP events into the original
     * game's reacquired DirectInput keyboard state. */
    spider_sdl3_universal_focus_release_capture(
      &focus_release,held_scans,sent_mask,pulse_scan);
    if(previous_context==SPIDER_UNIVERSAL_CTX_3D) {
      if(!focus_dir.phase) {
        spider_sdl3_focus_dir_capture(
          &focus_dir,held_scans,sent_mask,
          SpiderDI8Retry_KeyboardRecoveryAttempts());
        if(focus_dir.phase) {
          char msg[180];
          focus_dir_controller_id=selected_id;
          snprintf(msg,sizeof(msg),
            "SDL3-UNIVERSAL: FOCUS-DIR captured owned 3D keys=%u selected-id=%lu for fresh buffered release",
            focus_dir.count,(unsigned long)selected_id);
          log_focus_release(msg);
          trace_native_keyboard_latch("focus-loss",focus_dir.scans[0]);
          focus_latch_follow_scan=0;focus_latch_follow_frames=0;
        }
      }
    }
    if(!previous && focus_release.frames_remaining) {
      char msg[170];
      snprintf(msg,sizeof(msg),
        "SDL3-UNIVERSAL: FOCUS-KEYUP deferred foreground replay armed scans=%u frames=%u",
        focus_release.count,focus_release.frames_remaining);
      log_focus_release(msg);
    }
  }
  if(focus_dir.phase==SPIDER_FOCUS_DIR_UP &&
     focus_dir.index<focus_dir.count) {
    /* The fresh buffered recovery DOWN is an injected keyboard key,
     * even though it is not part of ordinary gameplay sent_mask.
     * Every owner-loss/modal/device-disconnect path must release it. */
    if(send_scan(focus_dir.scans[focus_dir.index],1)) {
      if(reason &&
         (!strcmp(reason,"focus or window lost") ||
          !strcmp(reason,"game HWND not foreground"))) {
        focus_dir.phase=SPIDER_FOCUS_DIR_WAIT;
        focus_dir.wait_frames=0;
        focus_dir.baseline_attempts=
          SpiderDI8Retry_KeyboardRecoveryAttempts();
      } else {
        log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR interrupted recovery press safely released and cancelled");
        spider_sdl3_focus_dir_complete(&focus_dir);
        focus_dir_controller_id=0;
      }
    } else
      log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR interrupted KEYUP retry pending; new gameplay blocked");
  }
  if(reason && (suspended || active || sent_mask || pulse_scan)) {
    char line[200];
    snprintf(line,sizeof(line),"SDL3-UNIVERSAL: EXIT reason=%s",reason);
    log_universal(line);
  }
  /* Native pointer restoration has priority on focus loss even if a
   * synthetic KEYUP failed. Keep the pending scan bits for retry on the
   * next safe Flip, but never leave the original game joystick suspended
   * solely because an OS keyboard event could not be delivered. */
  if(!release_all())
    log_universal("SDL3-UNIVERSAL: pending KEYUP retry after owner restoration");
  if(suspended && borrowed_native) {
    old=InterlockedCompareExchangePointer(SPIDER_UNIVERSAL_DI_SLOT,
                                           borrowed_native,NULL);
    if(old!=NULL)
      log_universal("SDL3-UNIVERSAL: WARNING native DI owner changed while suspended; original pointer not blindly overwritten");
    else
      log_universal("SDL3-UNIVERSAL: original game-owned DirectInput pointer RESTORED on exit/focus change");
  }
  suspended=0;active=0;selected_id=0;borrowed_native=NULL;
  native_unclaimed_guard=0;
  /* Preserve sent_mask and pulse_scan if KEYUP failed. They are retried
   * in BeforeUI and before any new joystick gameplay injection. */
  menu_previous=0;menu_arm=0;
  previous_context=SPIDER_UNIVERSAL_CTX_3D;
  spider_sdl3_universal_reset(&state);
}
void SpiderSDL3Universal_SetHostFocusBlocked(int blocked) {
  if(!requested || !game_thread ||
     game_thread!=GetCurrentThreadId())return;
  blocked=blocked!=0;
  if(blocked==native_host_focus_blocked)return;
  native_host_focus_blocked=blocked;
  if(blocked) {
    log_focus_release(
      "SDL3-UNIVERSAL: FOCUS-X11 host focus blocked; releasing injected 3D keys despite stale Win32 foreground");
    /* Exact existing focus-loss pathway: snapshot held directions,
     * release injected keys, restore original game-owned DI pointer,
     * then defer bounded keyboard-latch recovery until fresh foreground. */
    restore_original("focus or window lost");
  } else
    log_focus_release(
      "SDL3-UNIVERSAL: FOCUS-X11 GAME focus confirmed; neutral-only manual reentry");
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
  log_universal(old?
    "SDL3-UNIVERSAL: game-owned native DI pointer SUSPENDED (experimental; lifetime unproven)":
    "SDL3-UNIVERSAL: original game-owned DI slot already NULL; no pointer overwritten");
  return 1;
}
void SpiderSDL3Universal_Init(const char *ini,void (*logger)(const char *)) {
  requested=0;report=logger;trace=0;game_thread=0;
  key_transition_reports=0;owner_reports=0;refusal_reports=0;
  focus_release_reports=0;
  native_host_focus_blocked=0;
  memset(&focus_release,0,sizeof(focus_release));
  spider_sdl3_focus_dir_complete(&focus_dir);
  focus_dir_controller_id=0;
  focus_latch_follow_scan=0;focus_latch_follow_frames=0;
  active=selected_id=0;suspended=0;borrowed_native=NULL;
  native_unclaimed_guard=0;native_guard_reports=0;
  memset(held_scans,0,sizeof(held_scans));
  sent_mask=0;pulse_scan=0;menu_previous=menu_arm=0;
  previous_context=SPIDER_UNIVERSAL_CTX_3D;
  pause_context_reports=0;pause_command_reports=0;
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
void SpiderSDL3Universal_BeforeModal(HWND game_hwnd) {
  if(!requested || !game_thread ||
     game_thread!=GetCurrentThreadId())return;
  /* This is called BEFORE creating/activating the owned setup dialog.
   * Its modal GetMessage loop can block the next original-game Flip
   * indefinitely, so we MUST restore the engine's DI owner first. */
  restore_original("opening controller configuration modal");
  log_universal(game_hwnd?
    "SDL3-UNIVERSAL: setup modal entered; synthetic keys released and native owner restored":
    "SDL3-UNIVERSAL: setup modal entered without owner HWND; restoration attempted");
}
/* The focus-pause detector may use native X11 even when Wine's Win32
 * foreground HWND is still the game. By the time the detector enters its
 * blocking PAUSED loop, no further ordinary SDL3 Flip cleanup can run.
 * Capture owned held scans and restore native DI right at that boundary.
 * Do not use Win32 foreground as a gate: native X11 has already verified
 * the loss, and restore_original is idempotent on repeated focus events. */
void SpiderSDL3Universal_OnConfirmedFocusPause(void) {
  char msg[220];
  unsigned i,dir_mask=0;
  /* Always report the actual boundary state, even if a previous X11
   * OTHER sample already released the keys. These bounded diagnostics
   * distinguish "no SDL3 owner" from "late release" without mutating
   * the original game's keyboard latch or accepting a stale HWND. */
  if(!requested)return;
  if(!game_thread || game_thread!=GetCurrentThreadId()) {
    if(report && focus_release_reports++<24u)
      report("SDL3-UNIVERSAL: HOST-FOCUS-LOSS skipped: not on initialized game thread");
    return;
  }
  for(i=0;i<4u;i++)
    if(sent_mask&(1u<<i))dir_mask|=(1u<<i);
  snprintf(msg,sizeof(msg),
    "SDL3-UNIVERSAL: HOST-FOCUS-LOSS confirmed boundary active=%d suspended=%d selected=%lu sent=0x%03X dir=0x%X recovery-phase=%u pending-frames=%u X11-blocked=%d",
    active,suspended,(unsigned long)selected_id,sent_mask,dir_mask,
    (unsigned)focus_dir.phase,focus_release.frames_remaining,
    native_host_focus_blocked);
  log_focus_release(msg);
  if(dir_mask)
    for(i=0;i<4u;i++)
      if((dir_mask&(1u<<i)) && held_scans[i])
        trace_native_keyboard_latch("pre-focus-release",held_scans[i]);
  /* Confirmed X11 loss is authoritative. Block re-injection even if
   * GetForegroundWindow still incorrectly points at the game. */
  native_host_focus_blocked=1;
  restore_original("focus or window lost");
}
void SpiderSDL3Universal_BeforeUI(HWND hwnd) {
  if(!requested)return;
  if(!game_thread)game_thread=GetCurrentThreadId();
  if(game_thread!=GetCurrentThreadId())return;
  /* MUST run before focus pause blocks the original game render thread.
   * Critical: no externally held synthetic keys may survive Alt+Tab. */
  if(native_host_focus_blocked || !hwnd || !IsWindowVisible(hwnd) ||
     GetForegroundWindow()!=hwnd) {
    if(active || suspended || sent_mask || pulse_scan)
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
  unsigned menu_buttons=0,context;
  int result;
  if(!requested || game_thread!=GetCurrentThreadId())return;
  /* X11 can report OTHER/UNKNOWN even when Wine's HWND stays foreground.
   * Never synthesize a direction or reclaim native DI during this state. */
  if(native_host_focus_blocked) {
    if(active || suspended || sent_mask || pulse_scan)
      restore_original("focus or window lost");
    return;
  }
  if(!hwnd || !IsWindowVisible(hwnd) || GetForegroundWindow()!=hwnd) {
    restore_original("game HWND not foreground");
    return;
  }
  if(focus_latch_follow_frames && focus_latch_follow_scan) {
    char stage[52];
    snprintf(stage,sizeof(stage),"post-fresh-up-flip-%u",
      5u-focus_latch_follow_frames);
    trace_native_keyboard_latch(stage,focus_latch_follow_scan);
    if(!--focus_latch_follow_frames)focus_latch_follow_scan=0;
  }
  if(!SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample)) {
    if(refusal_reports++<6u)
      log_universal("SDL3-UNIVERSAL: waiting for selected SDL3 mapped pad (press Start on desired pad)");
    /* A passive SDL3 singleton selection is NOT an explicit Start owner.
     * Previously restore_original() exposed the old DirectInput joystick
     * here, allowing the remaining PS3 to control Spider-Man with its
     * unconfigured native layout after DS4 unplug.
     *
     * First release all keyboard events and the prior selected owner.
     * Then borrow the same verified game-owned pointer slot in an
     * intentionally unclaimed state, before the original Flip returns
     * to the game. Keyboard input remains independent. This isolation
     * applies only when ExperimentalSDL3UniversalGameplay=1. */
    if(active || sent_mask || pulse_scan ||
       (focus_dir.phase==SPIDER_FOCUS_DIR_UP) ||
       (suspended && !native_unclaimed_guard))
      restore_original("no selected mapped controller");
    if(native_unclaimed_guard && suspended &&
       (!writable_slot() || *SPIDER_UNIVERSAL_DI_SLOT!=NULL)) {
      if(report && native_guard_reports++<16u)
        report("SDL3-UNIVERSAL: UNCLAIMED guard displaced; refusing stale native owner");
      restore_original("unclaimed native DI slot changed");
    }
    if(!suspended && !sent_mask && !pulse_scan &&
       writable_slot() && suspend_original()) {
      native_unclaimed_guard=1;
      if(report && native_guard_reports++<16u)
        report("SDL3-UNIVERSAL: UNCLAIMED native DirectInput joystick held inactive until explicit SDL3 Start (keyboard unaffected)");
    }
    return;
  }
  profile=SpiderSDL3Probe_SetupProfile(vid,pid);
  if(!profile || !spider_sdl3_profile_valid(profile)) {
    if(refusal_reports++<6u)
      log_universal("SDL3-UNIVERSAL: REFUSED no valid normalized profile for selected pad");
    restore_original("invalid gamepad profile");
    return;
  }
  if(!writable_slot()) {
    if(refusal_reports++<6u)
      log_universal("SDL3-UNIVERSAL: REFUSED unverified writable native joystick slot");
    restore_original("native DI slot not proven");
    return;
  }
  if(!read_scans(scans)) {
    if(refusal_reports++<6u)
      log_universal("SDL3-UNIVERSAL: REFUSED original keyboard table invalid or not yet loaded");
    restore_original("game keyboard table not proven");
    return;
  }
  if(active && selected_id!=id)restore_original("SDL3 Start owner changed");
  /* A failed KEYUP after focus loss/disconnect must never be treated as
   * a new playable frame. Retry ALL previously injected keys BEFORE the
   * native DI pointer can be borrowed again. Previous mask and pulse are
   * kept intact until Win32 accepts their releases. */
  if(!active && (sent_mask || pulse_scan)) {
    if(!release_all()) {
      if(refusal_reports++<12u)
        log_universal("SDL3-UNIVERSAL: REENTRY BLOCKED pending synthetic KEYUP from prior focus/device owner");
      return;
    }
    if(owner_reports++<12u)
      log_universal("SDL3-UNIVERSAL: prior synthetic keys released before safe neutral reentry");
  }
  if(active && pulse_scan) {
    /* BeforeUI attempts the pulse KEYUP on every Flip; if Proton/Win32
     * refused it, do not process a new action while Escape/Enter is held. */
    if(!send_scan(pulse_scan,1)) {
      restore_original("pulse KEYUP failed before next gameplay action");
      return;
    }
    pulse_scan=0;
  }
  /* No native pointer manipulation or key injection until the physical
   * gamepad has returned to neutral after Start/selection/menu changes.
   * The policy also enforces neutral before producing 3D action bits. */
  /* A native joystick held in the unclaimed guard is suspended already,
   * but this must NOT bypass the fresh neutral gate after physical Start. */
  if(native_unclaimed_guard && !spider_sdl3_universal_neutral(&sample)) {
    if(owner_reports++<10u)
      log_universal("SDL3-UNIVERSAL: explicit Start selected pad awaits neutral before unclaimed DI guard can transfer");
    return;
  }
  if(!suspended && !spider_sdl3_universal_neutral(&sample)) {
    if(owner_reports++<10u)
      log_universal("SDL3-UNIVERSAL: selected pad awaits complete neutral before any DI suspension/injection");
    return;
  }
  if(!suspend_original()) {
    restore_original("unsafe native DI pointer");
    return;
  }
  if(native_unclaimed_guard) {
    native_unclaimed_guard=0;
    if(report && native_guard_reports++<16u)
      report("SDL3-UNIVERSAL: UNCLAIMED native DI guard transferred to explicitly Started SDL3 controller");
  }
  /* Do not share the neutral-wait suppression budget with the first
   * successful ACTIVE report: a held Start can consume all ten entries. */
  if(!active) {
    char line[165];
    snprintf(line,sizeof(line),
      "SDL3-UNIVERSAL: ACTIVE selected-id=%lu model=%04X:%04X (opt-in test; no proof of keyboard reception)",
      (unsigned long)id,(unsigned)vid,(unsigned)pid);
    log_universal(line);
  }
  active=1;selected_id=id;
  if(focus_dir.phase && id!=focus_dir_controller_id) {
    /* A different explicitly selected pad must never inherit a
     * synthesized movement press intended for the old owner. */
    if(focus_dir.phase==SPIDER_FOCUS_DIR_UP &&
       !send_scan(focus_dir.scans[focus_dir.index],1)) {
      log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR old-owner KEYUP retry");
      return;
    }
    log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR owner changed; fresh key pulse cancelled");
    spider_sdl3_focus_dir_complete(&focus_dir);
    focus_dir_controller_id=0;
  }
  /* Keep original native DI joystick suspended during foreground
   * KEYUP replay: otherwise the original game can sample a competing
   * PS3/DS4 joystick while awaiting its keyboard reacquisition. */
  if(focus_release.frames_remaining) {
    unsigned i,was=focus_release.frames_remaining;
    unsigned prior_count=focus_release.count;
    int delivered=1;
    /* Only AFTER the focus-pause loop returned. First, the verified
     * selected SDL3 pad has returned to neutral and the original
     * game-owned JOYSTICK pointer has been suspended again. The separate
     * DI keyboard may still be unacquired on the first resumed Flip in
     * Wine: repeat KEYUPs across subsequent Flip boundaries so it can
     * observe them after reacquisition. No new actions are injected. */
    for(i=0;i<focus_release.count;i++)
      if(!send_scan(focus_release.scans[i],1))delivered=0;
    spider_sdl3_universal_focus_release_complete_frame(
      &focus_release,delivered);
    if(!focus_release.frames_remaining && focus_dir.phase)
      trace_native_keyboard_latch("post-12-keyup",focus_dir.scans[0]);
    if(was==SPIDER_UNIVERSAL_FOCUS_RELEASE_FRAMES ||
       !focus_release.frames_remaining ||
       (!delivered && was==1u)) {
      char msg[180];
      snprintf(msg,sizeof(msg),
        "SDL3-UNIVERSAL: FOCUS-KEYUP foreground cleanup %s scans=%u passes-left=%u (DI reception unverified)",
        delivered?"accepted":"retry",prior_count,
        focus_release.frames_remaining);
      log_focus_release(msg);
    }
    return;
  }
  if(focus_dir.phase &&
     spider_sdl3_universal_route_context(menu,gameplay,bink,native_pause)
       !=SPIDER_UNIVERSAL_CTX_3D) {
    if(focus_dir.phase==SPIDER_FOCUS_DIR_UP &&
       !send_scan(focus_dir.scans[focus_dir.index],1)) {
      log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR context changed, pending KEYUP retry");
      return;
    }
    log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR context changed; fresh movement pulse cancelled");
    spider_sdl3_focus_dir_complete(&focus_dir);
    focus_dir_controller_id=0;
  }
  if(focus_dir.phase) {
    unsigned attempt=SpiderDI8Retry_KeyboardRecoveryAttempts();
    if(focus_dir.phase==SPIDER_FOCUS_DIR_UP) {
      trace_native_keyboard_latch("after-fresh-down-before-up",
        focus_dir.scans[focus_dir.index]);
      /* DOWN happened on the preceding original Flip. The game has
       * now had a frame to sample the buffered press, so the following
       * real UP event can clear its stale internal held-action latch. */
      if(!send_scan(focus_dir.scans[focus_dir.index],1)) {
        log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR fresh KEYUP failed; retry before gameplay");
        return;
      }
      focus_latch_follow_scan=focus_dir.scans[focus_dir.index];
      focus_latch_follow_frames=4u; /* next original input frames */
      focus_dir.index++;
      if(focus_dir.index>=focus_dir.count) {
        log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR fresh DOWN/UP completed after foreground keyboard recovery (game reception unverified)");
        spider_sdl3_focus_dir_complete(&focus_dir);
        focus_dir_controller_id=0;
      } else focus_dir.phase=SPIDER_FOCUS_DIR_DOWN;
      return;
    }
    if(!spider_sdl3_focus_dir_ready(&focus_dir,attempt))
      return;
    if(focus_dir.phase==SPIDER_FOCUS_DIR_DOWN) {
      trace_native_keyboard_latch("before-fresh-down",
        focus_dir.scans[focus_dir.index]);
      if(!send_scan(focus_dir.scans[focus_dir.index],0)) {
        log_focus_release("SDL3-UNIVERSAL: FOCUS-DIR fresh KEYDOWN failed; retry before gameplay");
        return;
      }
      if(focus_dir.index==0) {
        char msg[200];
        snprintf(msg,sizeof(msg),
          "SDL3-UNIVERSAL: FOCUS-DIR fresh keyboard DOWN after %u wait frames (retry attempts %u->%u; not an Acquire ACK)",
          focus_dir.wait_frames,focus_dir.baseline_attempts,attempt);
        log_focus_release(msg);
      }
      focus_dir.phase=SPIDER_FOCUS_DIR_UP;
      return;
    }
    return;
  }
  context=spider_sdl3_universal_route_context(
    menu,gameplay,bink,native_pause);
  if(context!=previous_context) {
    /* Entering the native 3D Pause screen is NOT a gameplay frame.
     * Reset action edges; a fully released controller must arm menu
     * navigation so the physical Pause button cannot instantly
     * close the menu again on the same held press. */
    spider_sdl3_universal_reset(&state);
    menu_arm=0;menu_previous=0;
    if((context==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE ||
        previous_context==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE) &&
       pause_context_reports++<14u) {
      char msg[180];
      snprintf(msg,sizeof(msg),
        "SDL3-UNIVERSAL: PAUSE-MENU context transition %u -> %u (menu arm reset; held keys released on routing)",
        previous_context,context);
      log_universal(msg);
    }
    previous_context=context;
  }
  if(context!=SPIDER_UNIVERSAL_CTX_3D) {
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
      /* The configured Pause control must ALSO resume the original
       * native Pause menu. This does not affect Pause in gameplay,
       * generic 2D menus or Bink. Circle/Back still closes Pause. */
      int resume=context==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE &&
        binding_held(profile->actions[SPIDER_SDL3_ACTION_PAUSE],&sample);
      if(confirm)menu_buttons|=1u<<4;
      if(back || resume)menu_buttons|=1u<<5;
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
      if(context==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE &&
         menu_buttons!=menu_previous && pause_command_reports++<24u) {
        char msg[185];
        snprintf(msg,sizeof(msg),
          "SDL3-UNIVERSAL: PAUSE-MENU INPUT held=0x%02X previous=0x%02X (Up/Down/Left/Right/Confirm/Back)",
          menu_buttons,menu_previous);
        log_universal(msg);
      }
      if((menu_buttons&(1u<<5)) && !(menu_previous&(1u<<5)))
        pulse(0x01u,"menu Back/Pause resume");
      else if((menu_buttons&(1u<<4)) && !(menu_previous&(1u<<4)))
        pulse(0x1cu,"menu Confirm");
    }
    if(!update_keys(menu_buttons&0x0fu,scans)) {
      log_universal("SDL3-UNIVERSAL: menu SendInput failure; restoring native owner");
      restore_original("menu SendInput failed");
      return;
    }
    menu_previous=menu_buttons;
    return;
  }
  menu_arm=0;menu_previous=0;
  result=spider_sdl3_universal_step(&state,profile,&sample,id,
                                      native_pause,&desired,&pause_edge);
  if(result!=1) {
    /* An identity/context rearm can request no 3D actions while an old
     * synthetic direction/action is still held. A failed KEYUP must
     * restore native ownership, not silently continue with a stuck key. */
    if(!update_keys(0u,scans)) {
      log_universal("SDL3-UNIVERSAL: neutral rearm KEYUP failed; restoring native owner");
      restore_original("neutral rearm KEYUP failed");
    }
    return;
  }
  if(!update_keys(desired,scans)) {
    log_universal("SDL3-UNIVERSAL: SendInput failure; releasing and restoring native owner");
    restore_original("SendInput failed");
    return;
  }
  if(pause_edge)pulse(0x01u,"3D Pause");
}
