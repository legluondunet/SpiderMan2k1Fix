#ifndef SPIDERMAN2K1FIX_SDL3_UNIVERSAL_POLICY_H
#define SPIDERMAN2K1FIX_SDL3_UNIVERSAL_POLICY_H
/* Portable, manufacturer-independent SDL3 semantic -> 2001 keyboard action
 * policy. Actual key scanning/sending and DI ownership stay Win32-only. */
#include <stdint.h>
#include <string.h>
#include "sdl3_controller_profile.h"
enum {
  SPIDER_UNIVERSAL_UP=0, SPIDER_UNIVERSAL_DOWN=1,
  SPIDER_UNIVERSAL_LEFT=2, SPIDER_UNIVERSAL_RIGHT=3,
  SPIDER_UNIVERSAL_JUMP=4, SPIDER_UNIVERSAL_KICK=5,
  SPIDER_UNIVERSAL_PUNCH=6, SPIDER_UNIVERSAL_WEB=7,
  SPIDER_UNIVERSAL_AIM=8, SPIDER_UNIVERSAL_ZIPLINE=9,
  SPIDER_UNIVERSAL_SWING=10, SPIDER_UNIVERSAL_KEY_COUNT=11
};
enum { SPIDER_UNIVERSAL_DPAD_UP=11, SPIDER_UNIVERSAL_DPAD_DOWN=12,
       SPIDER_UNIVERSAL_DPAD_LEFT=13, SPIDER_UNIVERSAL_DPAD_RIGHT=14 };
/* Wine/DirectInput may not observe a SendInput KEYUP delivered while the
 * original game's keyboard device is unacquired after Alt+Tab. Record
 * only OUR injected scans at focus loss and reassert KEYUP after the
 * foreground focus-pause loop exits, for several consecutive Flip frames.
 * Gameplay remains blocked during every replay frame. */
#define SPIDER_UNIVERSAL_FOCUS_RELEASE_FRAMES 12u
typedef struct {
  unsigned scans[SPIDER_UNIVERSAL_KEY_COUNT+1];
  unsigned count;
  unsigned frames_remaining;
} SpiderSDL3UniversalFocusRelease;
static inline void spider_sdl3_universal_focus_release_capture(
    SpiderSDL3UniversalFocusRelease *recovery,
    const unsigned held_scans[SPIDER_UNIVERSAL_KEY_COUNT],
    unsigned sent_mask,unsigned pulse_scan) {
  unsigned i,j,scan;
  if(!recovery || recovery->frames_remaining)return;
  recovery->count=0;
  for(i=0;i<SPIDER_UNIVERSAL_KEY_COUNT+1u;i++) {
    if(i==SPIDER_UNIVERSAL_KEY_COUNT)scan=pulse_scan;
    else if(sent_mask&(1u<<i))scan=held_scans[i];
    else continue;
    if(!scan)continue;
    for(j=0;j<recovery->count;j++)
      if(recovery->scans[j]==scan)break;
    if(j==recovery->count)
      recovery->scans[recovery->count++]=scan;
  }
  if(recovery->count)
    recovery->frames_remaining=SPIDER_UNIVERSAL_FOCUS_RELEASE_FRAMES;
}
static inline void spider_sdl3_universal_focus_release_complete_frame(
    SpiderSDL3UniversalFocusRelease *recovery,int delivered) {
  if(!recovery || !recovery->frames_remaining || !delivered)return;
  if(--recovery->frames_remaining==0) {
    recovery->count=0;
    memset(recovery->scans,0,sizeof(recovery->scans));
  }
}

/* Only game-action scan codes can be forwarded through SendInput.
 * Exclude Windows modifiers, lock toggles, Pause, Enter/Escape and the
 * game's always-active F9 screenshot / F10-F12 global shortcuts.
 * Extended 0x80 is a zero low scan and cannot be sent safely. This is
 * NOT a general-purpose keyboard remapper. */
static inline int spider_sdl3_universal_scan_safe(unsigned scan) {
  if(scan<2u || scan>0xD3u || !(scan&0x7fu))return 0;
  switch(scan) {
    case 0x0Eu: /* Backspace */
    case 0x0Fu: /* Tab */
    case 0x1Cu: /* Enter */
    case 0x1Du: /* LCtrl */
    case 0x2Au: /* LShift */
    case 0x36u: /* RShift */
    case 0x38u: /* LAlt */
    case 0x3Au: /* CapsLock */
    case 0x43u: /* F9 global PNG shortcut */
    case 0x44u: /* F10 */
    case 0x45u: /* NumLock */
    case 0x46u: /* ScrollLock */
    case 0x57u: /* F11 */
    case 0x58u: /* F12 */
    case 0x9Cu: /* Numpad Enter */
    case 0x9Du: /* RCtrl */
    case 0xB7u: /* PrintScreen */
    case 0xB8u: /* RAlt */
    case 0xC5u: /* Pause/Break, nonstandard E1 prefix */
      return 0;
  }
  return 1;
}
/* One bounded buffered-keyboard resynchronization. Repeating KEYUP
 * alone did not clear the game-owned held action on GE-Proton11-12:
 * the original engine uses DirectInput GetDeviceData, not a pure
 * real-time keyboard state poll. A fresh DOWN, followed by UP on a
 * later original Flip, can yield an actual buffered release edge.
 *
 * Limit this to our four 3D movement directions held at focus loss,
 * never to Jump, combat actions, or menu confirmation. Attempt-count
 * hints from the game's own keyboard Acquire branch are not an ACK:
 * use a bounded fallback and a small settle period. */
enum {
  SPIDER_FOCUS_DIR_IDLE=0,
  SPIDER_FOCUS_DIR_WAIT=1,
  SPIDER_FOCUS_DIR_SETTLE=2,
  SPIDER_FOCUS_DIR_DOWN=3,
  SPIDER_FOCUS_DIR_UP=4
};
#define SPIDER_FOCUS_DIR_WAIT_LIMIT 24u
#define SPIDER_FOCUS_DIR_SETTLE_FRAMES 3u
typedef struct {
  unsigned scans[4];
  unsigned count;
  unsigned phase;
  unsigned index;
  unsigned baseline_attempts;
  unsigned wait_frames;
  unsigned settle_frames;
} SpiderSDL3FocusDirectionResync;
static inline void spider_sdl3_focus_dir_capture(
    SpiderSDL3FocusDirectionResync *s,
    const unsigned held_scans[SPIDER_UNIVERSAL_KEY_COUNT],
    unsigned held_mask,unsigned keyboard_attempts) {
  unsigned i;
  if(!s || s->phase || !held_scans)return;
  memset(s,0,sizeof(*s));
  for(i=SPIDER_UNIVERSAL_UP;i<=SPIDER_UNIVERSAL_RIGHT;i++)
    if((held_mask&(1u<<i)) && held_scans[i] &&
       spider_sdl3_universal_scan_safe(held_scans[i]))
      s->scans[s->count++]=held_scans[i];
  if(s->count) {
    s->baseline_attempts=keyboard_attempts;
    s->phase=SPIDER_FOCUS_DIR_WAIT;
  }
}
static inline int spider_sdl3_focus_dir_ready(
    SpiderSDL3FocusDirectionResync *s,unsigned keyboard_attempts) {
  if(!s || !s->phase)return 0;
  if(s->phase==SPIDER_FOCUS_DIR_WAIT) {
    if(keyboard_attempts>s->baseline_attempts ||
       ++s->wait_frames>=SPIDER_FOCUS_DIR_WAIT_LIMIT) {
      s->phase=SPIDER_FOCUS_DIR_SETTLE;
      s->settle_frames=SPIDER_FOCUS_DIR_SETTLE_FRAMES;
    }
    return 0;
  }
  if(s->phase==SPIDER_FOCUS_DIR_SETTLE) {
    if(s->settle_frames)--s->settle_frames;
    if(!s->settle_frames)s->phase=SPIDER_FOCUS_DIR_DOWN;
    return 0;
  }
  return s->phase==SPIDER_FOCUS_DIR_DOWN;
}
static inline void spider_sdl3_focus_dir_complete(
    SpiderSDL3FocusDirectionResync *s) {
  if(s)memset(s,0,sizeof(*s));
}
/* Original Spider-Man does not switch to its 640x480 menu renderer
 * when ESC opens the native 3D Pause menu. The renderer still reports
 * gameplay=1, while the original Pause ribbon/context confirms Pause.
 * Always choose menu navigation ahead of gameplay-action evaluation,
 * regardless of the gameplay renderer flag. Bink remains independent. */
enum {
  SPIDER_UNIVERSAL_CTX_3D=0,
  SPIDER_UNIVERSAL_CTX_MENU=1,
  SPIDER_UNIVERSAL_CTX_BINK=2,
  SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE=3
};
/* Keep a menu-confirm synthetic key down during one additional original
 * Flip so DirectInput can observe it. Never defer release more than once.
 * This applies ONLY to menu Confirm, not Bink/3D/Game Over/Pause pulses. */
static inline int spider_sdl3_menu_confirm_defer_keyup(unsigned *flips) {
  if(!flips || !*flips)return 0;
  --*flips;
  return 1;
}
static inline unsigned spider_sdl3_universal_route_context(
    int menu,int gameplay,int bink,int native_pause) {
  if(native_pause)return SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE;
  if(bink)return SPIDER_UNIVERSAL_CTX_BINK;
  if(menu || !gameplay)return SPIDER_UNIVERSAL_CTX_MENU;
  return SPIDER_UNIVERSAL_CTX_3D;
}
typedef struct {
  uint32_t selected_id;
  uint32_t held_actions;
  unsigned armed;
} SpiderSDL3UniversalState;
static inline void spider_sdl3_universal_reset(SpiderSDL3UniversalState *state) {
  if(state)memset(state,0,sizeof(*state));
}
static inline int spider_sdl3_universal_neutral(
    const SpiderSDL3ControllerSample *sample) {
  if(!sample || !spider_sdl3_profile_neutral(sample))return 0;
  return sample->axis[SPIDER_SDL3_AXIS_LEFTX]>-9000 &&
         sample->axis[SPIDER_SDL3_AXIS_LEFTX]<9000 &&
         sample->axis[SPIDER_SDL3_AXIS_LEFTY]>-9000 &&
         sample->axis[SPIDER_SDL3_AXIS_LEFTY]<9000;
}
/* Return 1 if live, 0 while waiting for neutral, -1 if unsupported.
 * Keyboard desired held actions have eleven bit positions. Pause is an
 * independent edge, never a permanently held keyboard scancode. */
static inline int spider_sdl3_universal_step(SpiderSDL3UniversalState *state,
    const SpiderSDL3ControllerProfile *profile,
    const SpiderSDL3ControllerSample *sample,uint32_t id,int native_pause,
    uint32_t *desired,unsigned *pause_edge) {
  uint32_t actions=0,mask=0;
  const int16_t x=sample?sample->axis[SPIDER_SDL3_AXIS_LEFTX]:0;
  const int16_t y=sample?sample->axis[SPIDER_SDL3_AXIS_LEFTY]:0;
  if(desired)*desired=0;
  if(pause_edge)*pause_edge=0;
  if(!state || !profile || !sample || !desired || !pause_edge || !id ||
     !spider_sdl3_profile_valid(profile)) {
    spider_sdl3_universal_reset(state);
    return -1;
  }
  if(state->selected_id!=id) {
    spider_sdl3_universal_reset(state);
    state->selected_id=id;
  }
  if(!state->armed) {
    if(spider_sdl3_universal_neutral(sample))state->armed=1;
    return 0;
  }
  if(!spider_sdl3_profile_actions(profile,sample,state->held_actions,
                                  &actions)) {
    spider_sdl3_universal_reset(state);return -1;
  }
  *pause_edge=((actions&(1u<<SPIDER_SDL3_ACTION_PAUSE))!=0u) &&
              ((state->held_actions&(1u<<SPIDER_SDL3_ACTION_PAUSE))==0u);
  state->held_actions=actions;
  if(native_pause)return 1; /* no movement/combat while native menu open */
  if(y<-12000 || (sample->buttons&(1u<<SPIDER_UNIVERSAL_DPAD_UP)))
    mask|=1u<<SPIDER_UNIVERSAL_UP;
  if(y>12000 || (sample->buttons&(1u<<SPIDER_UNIVERSAL_DPAD_DOWN)))
    mask|=1u<<SPIDER_UNIVERSAL_DOWN;
  if(x<-12000 || (sample->buttons&(1u<<SPIDER_UNIVERSAL_DPAD_LEFT)))
    mask|=1u<<SPIDER_UNIVERSAL_LEFT;
  if(x>12000 || (sample->buttons&(1u<<SPIDER_UNIVERSAL_DPAD_RIGHT)))
    mask|=1u<<SPIDER_UNIVERSAL_RIGHT;
  if((mask&3u)==3u)mask&=~3u;
  if((mask&12u)==12u)mask&=~12u;
  mask|=(actions&0x7fu)<<SPIDER_UNIVERSAL_JUMP;
  *desired=mask;
  return 1;
}

/* Strictly opt-in GAME OVER/RETRY/QUIT Enter trial.
 * The native pending request 2 is NOT a unique scene identifier. The
 * caller must additionally verify exact runtime state and foreground;
 * we only produce one edge after six stable candidate frames and a full
 * neutral frame, never by carrying an already held South across scenes.
 * 0=normal gameplay, 1=Enter once, 2=consume until fully neutral. */
#define SPIDER_GAMEOVER_TRIAL_STABLE_FRAMES 6u
typedef struct {
  unsigned stable_frames;
  unsigned armed;
  unsigned consumed;
} SpiderSDL3GameOverTrial;
static inline void spider_sdl3_gameover_trial_reset(
    SpiderSDL3GameOverTrial *s) {
  if(s)memset(s,0,sizeof(*s));
}
static inline unsigned spider_sdl3_gameover_trial_step(
    SpiderSDL3GameOverTrial *s,int candidate,int neutral,
    int confirm_only) {
  if(!s)return 0;
  if(!candidate) {
    spider_sdl3_gameover_trial_reset(s);
    return 0;
  }
  if(s->stable_frames<SPIDER_GAMEOVER_TRIAL_STABLE_FRAMES)
    ++s->stable_frames;
  if(s->stable_frames<SPIDER_GAMEOVER_TRIAL_STABLE_FRAMES)
    return 0;
  if(neutral) {
    s->armed=1;
    s->consumed=0;
    return 0;
  }
  if(s->consumed)return 2;
  if(!s->armed || !confirm_only)return 0;
  s->armed=0;
  s->consumed=1;
  return 1;
}

#endif
