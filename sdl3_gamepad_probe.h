#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_PROBE_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_PROBE_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include "sdl3_controller_profile.h"

/* Built-in SDL3 mapped-controller discovery and selection.
 * SDL3 is the sole supported gamepad mapping backend. The original
 * keyboard remains independent, including when no controller is found.
 * SDL3 input is only polled on the verified game/Flip thread. */
void SpiderSDL3Probe_Init(const char *ini,void (*logger)(const char *));
void SpiderSDL3Probe_OnFlip(long flip,HWND game_hwnd);
/* The Flip-blocking Alt+Tab PAUSED screen uses the same selected SDL3
 * handle on the same game thread, with no key injection or DirectInput
 * Acquire. Begin requires a verified game-owned selected device; Poll
 * requires foreground and a new neutral->press->release digital sequence. */
int SpiderSDL3Probe_PauseWake_Begin(void);
unsigned SpiderSDL3Probe_PauseWake_Poll(HWND game_hwnd);
void SpiderSDL3Probe_PauseWake_End(void);
/* The universal adapter accepts only a selected, connected,
 * uniquely identified SDL3-mapped pad (explicit Start, neutral first).
 * No legacy DirectInput gamepad mapping or device rebind is available. */
/* Built-in keyboard adapter: one selected, uniquely identified mapped SDL3
 * controller, including explicit PS3 selection (never native DI ownership).
 * Reject disconnected or ambiguous generic devices without fallback. */
int SpiderSDL3Probe_SelectedMappedSample(uint32_t *instance,
    uint16_t *vid,uint16_t *pid,SpiderSDL3ControllerSample *sample);
int SpiderSDL3Probe_GenericSelectedSample(uint32_t *instance,
    uint16_t *vid,uint16_t *pid,SpiderSDL3ControllerSample *sample);
/* Verified New Game Bink #4 -> first 3D input handoff only.
 * A held SDL3-mapped South/A/Cross may affect the first Jump/skip action.
 * Refresh the ALREADY-open selected SDL3 gamepad on the same game
 * thread and wait (max 5s) for SOUTH to release before first 3D input.
 * Return 1 on observed held->released, 0 when no pad/SOUTH held,
 * -1 on lost identity/focus, -2 on bounded timeout. Diagnostics log
 * any actual held state. No game-owned DirectInput access or writes. */
int SpiderSDL3Probe_NewGameBinkWaitSouthRelease(HWND game_window);
/* Explicit controller-setup dialog only. Called synchronously on the
 * original game thread while its Flip/tick is stopped in the native
 * Joystick menu. Poll mapped SDL3 devices without altering game-owned
 * selection, native joystick or UI synthetic events. No auto init. */
/* Shared IN-MEMORY profile bank with optional SDL3BindingTrace. */
SpiderSDL3ControllerProfile *SpiderSDL3Probe_SetupProfile(uint16_t vid,uint16_t pid);
/* Transactional per-model profile commit. On write failure the current
 * in-memory profile stays unchanged; game-owned DirectInput untouched. */
int SpiderSDL3Probe_SetupCommit(const SpiderSDL3ControllerProfile *candidate);
int SpiderSDL3Probe_SetupPump(void);
/* After the modal window closes, invalidate Start edges sampled while
 * gameplay Flip was suspended. An explicit Start OR physical Circle/B
 * (or Cross on the Close button) from the displayed pad can request a
 * verified owner handoff on the NEXT foreground Flip. L1/R1 browsing
 * alone, Escape and mouse/Win32 Close do not change game ownership. */
void SpiderSDL3Probe_SetupModalReturn(uint32_t requested_id,
                                      uint16_t vendor,uint16_t product);
unsigned SpiderSDL3Probe_SetupCount(void);
int SpiderSDL3Probe_SetupRead(unsigned slot,uint32_t *instance_id,
    uint16_t *vid,uint16_t *pid,char *name,size_t capacity,
    SpiderSDL3ControllerSample *sample);


/* Keyboard-operated raw joystick calibration. Device access is strictly
 * separate from the original game's DirectInput joystick and is allowed
 * only on the existing SDL3 game/UI thread. No gameplay injection. */
typedef struct {
  uint32_t buttons;             /* first 32 raw physical buttons */
  int16_t axes[16];             /* first 16 raw joystick axes */
  uint8_t hats[4];              /* first four POV hats */
  unsigned button_count,axis_count,hat_count;
} SpiderSDL3RawSample;
unsigned SpiderSDL3Probe_RawList(uint32_t *ids,unsigned capacity);
/* List ALL SDL joysticks (mapped as gamepads or not). The wizard can
 * recalibrate a device even when SDL has a pre-existing, broken mapping.
 * mapped_flags receives 1 for SDL-mapped gamepads and 0 for raw-only pads.
 * Never changes native game ownership or DirectInput state. */
unsigned SpiderSDL3Probe_RawListAll(uint32_t *ids,unsigned *mapped_flags,
                                    unsigned capacity);
int SpiderSDL3Probe_RawName(uint32_t id,char *name,size_t capacity);
int SpiderSDL3Probe_RawBegin(uint32_t id,char guid[33],
                            char *name,size_t capacity);
int SpiderSDL3Probe_RawRead(SpiderSDL3RawSample *out);
void SpiderSDL3Probe_RawEnd(void);
/* Applies a validated mapping and saves a GUID-specific file atomically.
 * Returns 1 only if both SDL mapping and persistence succeeded. */
int SpiderSDL3Probe_RawRegisterAndSave(const char *mapping,const char *guid);

#endif
