/* Experimental Xbox360-compatible DirectInput gameplay overlay.
 * Uses the SAME 7 verified Spidey.cfg DWORD offsets as the Sony-only
 * PS1 preview, but with an entirely separate input layout.
 *
 * Wine's XInput->DirectInput export usually exposes A/B/X/Y as
 * rgbButtons[0..3], LB/RB as [4..5], Back/Start as [6..7],
 * L3/R3 as [8..9]. The Holtek 045E:028E controller has 10 buttons,
 * but button order has NOT been established from hardware logs.
 *
 * The game maps exactly 7 actions to digital buttons. The original
 * left/right Xbox triggers are typically an analog Z axis in
 * DirectInput, NOT two independent digital buttons. Therefore Swing
 * temporarily uses L3 (index 8) instead of stealing Start (index 7).
 * We must NOT claim RT works until a separately tested trigger bridge.
 *
 * Apply only after an unambiguous Xbox device was independently
 * acquired and the verified game's virtual settings reloaded.
 * Keep all seven persistent user settings unchanged.
 */
#ifndef SPIDER_XBOX360_GAMEPLAY_MAPPING_H
#define SPIDER_XBOX360_GAMEPLAY_MAPPING_H
#include <stdint.h>
#include <stddef.h>
#include "ps1_gamepad_mapping.h"
enum {SPIDER_XBOX360_CFG_SIZE=SPIDER_PS1_CFG_SIZE,
      SPIDER_XBOX360_ACTION_COUNT=SPIDER_PS1_BUTTON_COUNT};
static const uint32_t spider_xbox360_game_buttons[SPIDER_XBOX360_ACTION_COUNT]={
  0u, /* Jump: A */
  1u, /* Kick: B */
  2u, /* Punch: X */
  3u, /* Web: Y */
  4u, /* Aim: LB */
  5u, /* Zipline: RB */
  8u  /* Swing: L3; RT is analog, avoid START=index 7 */
};
static inline int spider_xbox360_apply_gameplay(uint8_t *cfg,size_t length) {
  size_t i;
  if(!cfg || length!=SPIDER_XBOX360_CFG_SIZE)return 0;
  for(i=0;i<SPIDER_XBOX360_ACTION_COUNT;i++)
    spider_ps1_write_word(cfg,spider_ps1_offsets[i],spider_xbox360_game_buttons[i]);
  return 1;
}
#endif
