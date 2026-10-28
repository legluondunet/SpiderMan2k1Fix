#ifndef SPIDERMAN2K1FIX_SDL3_GAMEPAD_PROBE_H
#define SPIDERMAN2K1FIX_SDL3_GAMEPAD_PROBE_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include "sdl3_controller_profile.h"

/* OFF-by-default SDL3 discovery and selection telemetry only.
 * NO game input writes, synthetic keys, hooks or native DI device mutations.
 * SDL3.dll is optional and is only loaded from the executable directory. */
void SpiderSDL3Probe_Init(const char *ini,void (*logger)(const char *));
void SpiderSDL3Probe_OnFlip(long flip,HWND game_hwnd);
/* Opt-in native ownership experiment. Read only from the same Flip thread.
 * 'Available' means SDL initialized and selected an instance; it does NOT
 * mean DirectInput has been rebound. Duplicate VID/PID must fail closed. */
int SpiderSDL3Probe_NativeOwnerEnabled(void);
unsigned SpiderSDL3Probe_MappedCount(void);
int SpiderSDL3Probe_SelectedIdentity(uint16_t *vid,uint16_t *pid,
                                     uint32_t *instance_id);
/* Passive normalized RT value 0..32767 for the current selected SDL pad.
 * No events consumed, no SDL/window state created; fails closed when
 * no active mapped controller is available or RT axis is invalid. */
int SpiderSDL3Probe_SelectedRT(int *normalized_rt);
/* Passive selected SDL3 input snapshot, only on the original Flip thread
 * after its SDL3 event pump. Includes both triggers and semantic buttons.
 * No native DirectInput ownership writes, no event queue consumption. */
int SpiderSDL3Probe_SelectedSample(uint32_t *instance,uint16_t *vid,
                                  uint16_t *pid,
                                  SpiderSDL3ControllerSample *sample);
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
unsigned SpiderSDL3Probe_SetupCount(void);
int SpiderSDL3Probe_SetupRead(unsigned slot,uint32_t *instance_id,
    uint16_t *vid,uint16_t *pid,char *name,size_t capacity,
    SpiderSDL3ControllerSample *sample);


#endif
