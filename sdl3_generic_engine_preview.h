#ifndef SPIDERMAN2K1FIX_SDL3_GENERIC_ENGINE_PREVIEW_H
#define SPIDERMAN2K1FIX_SDL3_GENERIC_ENGINE_PREVIEW_H

/*
 * PHASE 1 RESEARCH ONLY. Pure, portable C99 preview of semantic SDL3
 * actions as the game's observed 3D input-direction/action bitmask.
 * Derived from genuine SHA-verified SpideyPC.exe disassembly:
 *   0x0050A2F9..0x0050A32D axis/POV -> direction 1,2,4,8
 *   0x0050A377..0x0050A3F3 native 14-slot action table
 *   0x00568408 masks for actions 4..10: 0x10..0x400
 *
 * This NEVER hooks the game, touches DirectInput, mutates memory,
 * synthesizes keyboard actions, or grants native game ownership.
 * The original game consumes buffered events, not SDL3 directly.
 * A future authenticated bridge must independently validate context,
 * ownership, mode, focus and analog semantics before applying masks.
 *
 * Preview API retains the pre-existing validated model-profile requirement.
 * Zero-VID/PID and identical model profiles need a separate migration.
 */
#include <stdint.h>
#include "sdl3_controller_profile.h"

enum {
  SPIDER_SDL3_ENGINE_DIR_UP=0x1u,
  SPIDER_SDL3_ENGINE_DIR_DOWN=0x2u,
  SPIDER_SDL3_ENGINE_DIR_LEFT=0x4u,
  SPIDER_SDL3_ENGINE_DIR_RIGHT=0x8u,
  SPIDER_SDL3_ENGINE_FIRST_ACTION=4u,
  SPIDER_SDL3_ENGINE_LAST_ACTION=10u,
  SPIDER_SDL3_ENGINE_DPAD_UP=11u,
  SPIDER_SDL3_ENGINE_DPAD_DOWN=12u,
  SPIDER_SDL3_ENGINE_DPAD_LEFT=13u,
  SPIDER_SDL3_ENGINE_DPAD_RIGHT=14u,
  /* SDL3's full signed stick scale maps the game's ±1000 native
   * axis to ±32767; original threshold was ±250 (~one quarter).
   * This is a research approximation, not physically calibrated. */
  SPIDER_SDL3_ENGINE_AXIS_THRESHOLD=8192
};

static inline int spider_sdl3_generic_engine_preview(
    const SpiderSDL3ControllerProfile *profile,
    const SpiderSDL3ControllerSample *sample,
    uint32_t previous_actions,
    uint32_t *engine_mask,
    uint32_t *new_actions) {
  uint32_t actions=0,dir=0,button;
  int x,y;
  unsigned up,down,left,right;
  if(engine_mask)*engine_mask=0;
  if(new_actions)*new_actions=0;
  if(!engine_mask || !new_actions || !sample || !profile ||
     !spider_sdl3_profile_actions(profile,sample,previous_actions,&actions))
    return 0;
  /* D-pad intentionally takes directional priority over left stick when
   * the user presses both; opposite D-pad directions cancel for safety. */
  button=sample->buttons;
  x=sample->axis[SPIDER_SDL3_AXIS_LEFTX];
  y=sample->axis[SPIDER_SDL3_AXIS_LEFTY];
  up=(button>>SPIDER_SDL3_ENGINE_DPAD_UP)&1u;
  down=(button>>SPIDER_SDL3_ENGINE_DPAD_DOWN)&1u;
  left=(button>>SPIDER_SDL3_ENGINE_DPAD_LEFT)&1u;
  right=(button>>SPIDER_SDL3_ENGINE_DPAD_RIGHT)&1u;
  if(!up && !down) {
    up=y< -SPIDER_SDL3_ENGINE_AXIS_THRESHOLD;
    down=y>SPIDER_SDL3_ENGINE_AXIS_THRESHOLD;
  }
  if(!left && !right) {
    left=x< -SPIDER_SDL3_ENGINE_AXIS_THRESHOLD;
    right=x>SPIDER_SDL3_ENGINE_AXIS_THRESHOLD;
  }
  if(up && !down)dir|=SPIDER_SDL3_ENGINE_DIR_UP;
  if(down && !up)dir|=SPIDER_SDL3_ENGINE_DIR_DOWN;
  if(left && !right)dir|=SPIDER_SDL3_ENGINE_DIR_LEFT;
  if(right && !left)dir|=SPIDER_SDL3_ENGINE_DIR_RIGHT;
  /* The verified first seven semantic actions correspond to the
   * seven native 14-slot masks 0x10..0x400 (Jump..Swing).
   * Pause is handled by the existing context owner, never forwarded
   * as a fictitious eighth game-native button action. */
  *new_actions=actions;
  *engine_mask=dir|((actions&0x7fu)<<SPIDER_SDL3_ENGINE_FIRST_ACTION);
  return 1;
}
#endif
