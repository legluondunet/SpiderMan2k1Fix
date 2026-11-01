/* Default-on, guarded DS4 + verified wired Xbox 360 3D action-map bridge. No COM vtable interception.
 * The ORIGINAL game-owned DirectInput reader keeps movement/POV, and
 * consumes the same SHA-verified seven (mask, button) DWORD slots that
 * the already-tested canonical Sony/Xbox native swap uses.
 *
 * This cannot represent SDL3 D-pad/extra-paddle inputs as DirectInput
 * rgbButtons, unverified DS4 L2, or Xbox analog triggers as buttons.
 * The Xbox RT Swing action stays UNBOUND in native RAM and continues
 * through the separately proven guarded RT-keyboard bridge.
 * Reject the whole unsupported
 * profile; NEVER guess physical indices or synthesize duplicate game
 * action keys. Pause is one bounded Escape edge via the existing
 * gamepad UI key owner; other seven actions use native hardware ONLY.
 *
 * Invariant: only replace exact canonical selected DS4/Xbox table; restore its exact
 * snapshot on exiting playable 3D, focus loss, identity/profile changes.
 * Strictly OFF unless the user selects the opt-in key in the main INI.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdl3_gameplay_live.h"
#include "sdl3_gameplay_native_map.h"
#include "sdl3_gamepad_probe.h"
#include "native_joystick_reconnect.h"
#include "native_joystick_swap_probe.h"
#include "gamepad_ui_profile.h"

#define SPIDER_GAMEPLAY_NATIVE_TABLE ((uintptr_t)0x00568408u)
#define SPIDER_GAMEPLAY_NATIVE_DEVICE ((uintptr_t)0x006B7A2Cu)
#define SPIDER_GAMEPLAY_NATIVE_FFB ((uintptr_t)0x006B7A68u)

static int enabled=0,applied=0,paused_edge_armed=0;
static unsigned reports=0;
static DWORD native_thread=0;
static void *owner=NULL;
static uint32_t owner_id=0;
static uint16_t owner_vid=0,owner_pid=0;
static uint32_t saved[SPIDER_PS1_BUTTON_COUNT],mapped[SPIDER_PS1_BUTTON_COUNT];
static SpiderSDL3Binding pause_binding;
static unsigned pause_held=0;
static void (*logline)(const char *)=NULL;
static void msg(const char *s) {if(logline && reports++<48u)logline(s);}
static int same7(const uint32_t *a,const uint32_t *b) {
  return memcmp(a,b,SPIDER_PS1_BUTTON_COUNT*sizeof(uint32_t))==0;
}
/* This is the same 14x8 RAM table used by the existing native hot-swap.
 * Require a writable DATA page with complete mapping integrity; never
 * change page protection or executable code just for this experiment. */
static int native_read(uint32_t out[SPIDER_PS1_BUTTON_COUNT]) {
  MEMORY_BASIC_INFORMATION m;
  uintptr_t p=SPIDER_GAMEPLAY_NATIVE_TABLE,b;
  if(sizeof(void*)!=4 || (uintptr_t)GetModuleHandleA(NULL)!=0x00400000u)
    return 0;
  if(VirtualQuery((const void *)p,&m,sizeof(m))!=sizeof(m) ||
     m.State!=MEM_COMMIT ||
     ((m.Protect&0xffu)!=PAGE_READWRITE &&
      (m.Protect&0xffu)!=PAGE_WRITECOPY) ||
     (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;
  b=(uintptr_t)m.BaseAddress;
  if(p<b || p-b>=m.RegionSize ||
     m.RegionSize-(size_t)(p-b)<SPIDER_NATIVE_SWAP_SLOTS*8u)return 0;
  return spider_native_swap_read((const uint8_t *)p,
                    SPIDER_NATIVE_SWAP_SLOTS*8u,out);
}
/* The legacy RT→Swing guard checks the native Xbox preset. A
 * *verified* customized six-button Xbox map is also safe for RT Swing
 * if its saved seventh (Swing) action still equals RT and the in-memory
 * native seven-action table exactly matches what THIS bridge installed.
 * Never treat arbitrary custom memory as a verified Xbox preset. */
int SpiderSDL3GameplayLive_VerifiedXboxSwingMap(void) {
  uint32_t current[SPIDER_PS1_BUTTON_COUNT],id=0;
  uint16_t vid=0,pid=0;
  if(!enabled || !applied || owner_vid!=0x045eu ||
     owner_pid!=0x028eu || !owner_id || !owner ||
     mapped[SPIDER_SDL3_ACTION_SWING]!=0x4000u ||
     *(void *volatile *)(uintptr_t)SPIDER_GAMEPLAY_NATIVE_DEVICE!=owner ||
     !SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id) ||
     id!=owner_id || vid!=owner_vid || pid!=owner_pid ||
     !SpiderNativeReconnect_SDL3SelectionReady(vid,pid) ||
     !native_read(current) || !same7(current,mapped))return 0;
  return 1;
}
/* On the original rendering thread, with exact previous bytes verified.
 * Transactional post-write check and best-effort rollback. No INI write. */
static int native_write(const uint32_t expected[SPIDER_PS1_BUTTON_COUNT],
                        const uint32_t desired[SPIDER_PS1_BUTTON_COUNT]) {
  uint32_t current[SPIDER_PS1_BUTTON_COUNT];
  unsigned i;
  uintptr_t p=SPIDER_GAMEPLAY_NATIVE_TABLE;
  if(!native_read(current) || !same7(current,expected))return 0;
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    memcpy((void *)(p+8u*(SPIDER_NATIVE_SWAP_FIRST_ACTION+i)+4u),
           &desired[i],sizeof(desired[i]));
  if(native_read(current) && same7(current,desired))return 1;
  /* Failed write: if the fixed mask table is still valid, restore the
   * exact bytes we just read before the mutation. */
  for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
    memcpy((void *)(p+8u*(SPIDER_NATIVE_SWAP_FIRST_ACTION+i)+4u),
           &expected[i],sizeof(expected[i]));
  msg("SDL3-3D-LIVE: failed native mapping verification, rolled back seven action DWORDs");
  return 0;
}
static void disarm_pause(void) {
  paused_edge_armed=0;
  pause_held=0;
}
/* The seven-action map must be normal during native Pause, while the
 * chosen eighth Pause binding and its press/release history survive. */
static int restore_native_table(void) {
  uint32_t current[SPIDER_PS1_BUTTON_COUNT];
  if(!applied)return 1;
  if(native_read(current) && same7(current,mapped) &&
     native_write(mapped,saved)) {
    applied=0;
    msg("SDL3-3D-LIVE: original seven selected-pad action bindings RESTORED (RAM only)");
    return 1;
  }
  applied=0;
  msg("SDL3-3D-LIVE: restore REFUSED, original table changed unexpectedly; no blind overwrite");
  return 0;
}
static void restore(const char *why) {
  (void)restore_native_table();
  (void)why;
  owner=NULL;owner_id=0;owner_vid=owner_pid=0;
  disarm_pause();
}
/* Must run immediately BEFORE native reconnect: the original rebind
 * code only accepts known canonical source tables. Never leave our
 * temporarily customized DS4 table active when the selected SDL3 pad
 * changes or a second/duplicate device appears. */
void SpiderSDL3GameplayLive_PreNativeOnFlip(HWND hwnd) {
  uint16_t vid=0,pid=0;
  uint32_t id=0;
  if(!enabled || !applied)return;
  if(native_thread && native_thread!=GetCurrentThreadId())return;
  if(!hwnd || GetForegroundWindow()!=hwnd ||
     SpiderSDL3Probe_MappedCount()==0u ||
     !SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id) ||
     id!=owner_id || vid!=owner_vid || pid!=owner_pid ||
     *(void *volatile *)(uintptr_t)SPIDER_GAMEPLAY_NATIVE_DEVICE!=owner)
    restore("pre-native SDL3 selection/foreground change");
}
int SpiderSDL3GameplayLive_OwnsPause(void) {
  uint16_t vid=0,pid=0;
  uint32_t id=0;
  /* The legacy handler runs before the live bridge's frame callback.
   * During a native pause the seven mapping DWORDs have been restored:
   * 'applied' must NOT allow duplicate Sony SHARE/OPTIONS Escape pulses.
   * Reserve Pause for the selected, verified DS4 throughout 3D/pause. */
  /* Ownership begins ONLY after a verified seven-action map was applied.
   * Keep it during native Pause (applied=0, edge armed) so our own
   * Escape pulse remains the only Sony pause source.  A refused map
   * must not suppress the existing safe SHARE fallback. */
  return enabled && (applied || paused_edge_armed) &&
    SpiderSDL3Probe_NativeOwnerEnabled() &&
    SpiderSDL3Probe_MappedCount()>0u &&
    SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id) &&
    id!=0u && vid==0x054cu &&
    (pid==0x05c4u || pid==0x09ccu || pid==0x0ba0u) &&
    SpiderNativeReconnect_SDL3SelectionReady(vid,pid);
}
void SpiderSDL3GameplayLive_Init(const char *ini,void (*logger)(const char *)) {
  logline=logger;
  if(applied)restore("initialization");
  enabled=0;reports=0;native_thread=0;
  enabled=ini && GetPrivateProfileIntA("Input",
       "ExperimentalSDL3GameplayBindings",1,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",1,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalSDL3NativeOwner",1,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalNativeJoystickUnplugGuard",1,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalNativeJoystickReconnect",1,ini)==1 &&
    GetPrivateProfileIntA("Input","ExperimentalNativeJoystickMappingSwap",1,ini)==1;
  if(enabled)msg("SDL3-3D-LIVE: EXPERIMENT enabled: DS4 native seven buttons + Pause, verified Xbox360 six face/shoulder buttons with RT Swing unbound; reject other SDL3 inputs, 3D only");
}
void SpiderSDL3GameplayLive_OnFlip(HWND hwnd,int menu2d,int gameplay3d,
                                   int bink,int native_pause) {
  uint32_t id=0,desired[SPIDER_PS1_BUTTON_COUNT],current[SPIDER_PS1_BUTTON_COUNT];
  const uint32_t *canonical;
  uint16_t vid=0,pid=0;
  SpiderSDL3ControllerSample sample;
  SpiderSDL3ControllerProfile *profile;
  void *device;
  int neutral,pressed,xbox360;
  if(!enabled)return;
  if(!native_thread)native_thread=GetCurrentThreadId();
  if(native_thread!=GetCurrentThreadId())return;
  /* Menu/Bink/focus loss disarms everything. A native in-game pause
   * restores the seven native actions without dropping the eighth
   * saved Pause edge latch, allowing that button to resume 3D. */
  if(!hwnd || !IsWindowVisible(hwnd) ||
     GetForegroundWindow()!=hwnd || menu2d || !gameplay3d || bink ||
     !SpiderSDL3Probe_NativeOwnerEnabled() ||
     SpiderSDL3Probe_MappedCount()==0u) {
    if(applied)restore("context or foreground changed");
    else disarm_pause();
    return;
  }
  if(!SpiderSDL3Probe_SelectedSample(&id,&vid,&pid,&sample) ||
     !SpiderNativeReconnect_SDL3SelectionReady(vid,pid)) {
    if(applied)restore("unverified owner");
    else disarm_pause();
    return;
  }
  profile=SpiderSDL3Probe_SetupProfile(vid,pid);
  xbox360=vid==0x045eu && pid==0x028eu;
  if(xbox360) {
    if(!spider_sdl3_native_xbox360_seven(profile,desired)) {
      if(applied)restore("unsupported Xbox360 mapping");
      else disarm_pause();
      if(reports<10u)msg("SDL3-3D-LIVE: REFUSED Xbox360 profile: only A/B/X/Y/LB/RB supported for six native actions and Swing must remain RT; LT/POV/unverified buttons refused");
      return;
    }
    canonical=spider_xbox360_buttons_sdl3_rt;
  } else {
    if(!spider_sdl3_native_ds4_seven(profile,desired)) {
      if(applied)restore("unsupported Sony mapping");
      else disarm_pause();
      if(reports<10u)msg("SDL3-3D-LIVE: REFUSED unsupported Sony binding/driver; only verified DS4 buttons and R2; L2/POV refused");
      return;
    }
    canonical=spider_ps1_buttons;
  }
  device=*(void *volatile *)(uintptr_t)SPIDER_GAMEPLAY_NATIVE_DEVICE;
  if(!device || *(void *volatile *)(uintptr_t)SPIDER_GAMEPLAY_NATIVE_FFB) {
    if(applied)restore("native device changed or force feedback");
    else disarm_pause();
    return;
  }
  if(paused_edge_armed && (device!=owner || id!=owner_id ||
     vid!=owner_vid || pid!=owner_pid ||
     !same7(desired,mapped) ||
     !spider_sdl3_binding_equal(
        profile->actions[SPIDER_SDL3_ACTION_PAUSE],pause_binding)))
    restore("owner or profile changed");
  if(native_pause) {
    if(applied && !restore_native_table()) {
      restore("native pause table restoration failed");
      return;
    }
    if(!paused_edge_armed || !native_read(current) ||
       !same7(current,saved)) {
      restore("native pause mapping no longer trustworthy");
      return;
    }
    /* Xbox native Pause is handled by the existing gamepad UI. */
    if(xbox360)return;
    goto sample_pause;
  }
  if(applied) {
    if(!native_read(current) || !same7(current,mapped)) {
      restore("mapping modified externally");
      return;
    }
  } else {
    /* Never inherit a held button or trigger after focus, hotplug, menu
     * or SDL3 profile changes. Game retains canonical bindings until
     * player returns to neutral. */
    neutral=spider_sdl3_profile_neutral(&sample);
    if(!neutral)return;
    if(!native_read(current)) {
      if(reports<10u)msg("SDL3-3D-LIVE: REFUSED native action table unreadable or unsupported protection/layout; no mapping writes");
      return;
    }
    if(!same7(current,canonical)) {
      if(reports<10u)msg("SDL3-3D-LIVE: REFUSED native seven-action table not exact selected-model canonical preset; preserve external/custom game controls");
      return;
    }
    memcpy(saved,current,sizeof(saved));
    memcpy(mapped,desired,sizeof(mapped));
    if(!same7(current,desired) && !native_write(current,desired)) {
      disarm_pause();
      return;
    }
    applied=1;owner=device;owner_id=id;owner_vid=vid;owner_pid=pid;
    pause_binding=profile->actions[SPIDER_SDL3_ACTION_PAUSE];
    paused_edge_armed=1;
    pause_held=0;
    msg(xbox360?
      "SDL3-3D-LIVE: APPLIED verified Xbox360 six-button SDL3 custom mapping in RAM; Swing=RT via guarded keyboard bridge, native Swing unbound; 3D foreground, neutral-first":
      "SDL3-3D-LIVE: APPLIED verified Sony DS4 seven-action native mapping in RAM; selected owner, 3D foreground, neutral-first; original axes and table masks retained");
  }
  /* Xbox Back/Start and PAUSE follow their existing gamepad UI policy.
   * Do not introduce a second independent Escape pulse from this bridge. */
  if(xbox360)return;
sample_pause:
  /* Pause is the eighth semantic action. It is NOT one of the native
   * seven button mappings. Use the existing gamepad UI's one-frame
   * Escape key owner rather than writing any eighth native slot.
   * The saved Pause binding may be a button or LT/RT, or NONE. */
  if(pause_binding.kind==SPIDER_SDL3_BIND_BUTTON)
    pressed=(sample.buttons&(1u<<pause_binding.index))!=0;
  else if(pause_binding.kind==SPIDER_SDL3_BIND_TRIGGER) {
    int v=sample.axis[pause_binding.index];
    if(v<0){disarm_pause();return;}
    pressed=v>=(pause_held?5000:11000);
  } else pressed=0;
  if(paused_edge_armed && pressed && !pause_held)
    SpiderGamepadUI_SDL3GameplayPause(hwnd);
  pause_held=pressed!=0;
}
