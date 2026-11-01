#ifndef SPIDERMAN2K1FIX_SDL3_GENERIC_SOURCE_ISOLATION_H
#define SPIDERMAN2K1FIX_SDL3_GENERIC_SOURCE_ISOLATION_H
/*
 * PHASE 1 RESEARCH ONLY - no runtime uses this code.
 *
 * Exact SHA-verified original SpideyPC.exe:
 * 0x0050A19F initializes outputs, 0x0050A1AC..0x0050A29D
 * constructs the keyboard mask; 0x0050A29E..0x0050A3F3
 * adds the original game-owned DirectInput joystick mask.
 * 0x0050A2AD calls the physical joystick event reader.
 * 0x0050A2B5 returns directly when the native reader says "absent",
 * preserving keyboard input. 0x0050A371 ORs native directions.
 *
 * A future game hook may selectively replace ONLY that controller
 * contribution. Never skip keyboard processing, and never combine
 * physical DirectInput and normalized SDL3 controller inputs.
 *
 * This standalone, pure-data policy deliberately requires an
 * externally authenticated proof that the physical joystick source
 * has been isolated. Nothing in this header obtains that proof or
 * installs a hook. Existing DS4/Xbox paths remain authoritative.
 */
#include <stdint.h>

enum {
  SPIDER_GENERIC_DIR_AND_ACTION_MASK = 0x07ffu,
  SPIDER_GENERIC_SOURCE_ORIGINAL = 0,
  SPIDER_GENERIC_SOURCE_PREVIEW = 1
};

typedef struct {
  uint32_t original_keyboard_held;
  uint32_t original_native_di_held;
  uint32_t normalized_sdl3_held;
  uint32_t owner_instance_id;
  unsigned user_opted_in;
  unsigned exact_exe_verified;
  unsigned callsite_verified;
  unsigned foreground;
  unsigned playable_3d;
  unsigned native_menu_or_bink_or_pause;
  unsigned sdl3_identity_verified;
  unsigned profile_verified;
  unsigned sampled_after_neutral;
  unsigned old_di_source_isolated;
} SpiderSDL3GenericSourceInput;

typedef struct {
  uint32_t held;
  uint32_t keyboard_retained;
  uint32_t controller_contribution;
  uint32_t owner_instance_id;
  unsigned source;
} SpiderSDL3GenericSourceResult;

static inline int spider_sdl3_generic_source_preview(
    const SpiderSDL3GenericSourceInput *input,
    SpiderSDL3GenericSourceResult *output) {
  int eligible;
  if(!output)return 0;
  output->held=0u;
  output->keyboard_retained=0u;
  output->controller_contribution=0u;
  output->owner_instance_id=0u;
  output->source=SPIDER_GENERIC_SOURCE_ORIGINAL;
  if(!input)return 0;

  output->keyboard_retained=input->original_keyboard_held;
  eligible=input->user_opted_in && input->exact_exe_verified &&
      input->callsite_verified && input->foreground &&
      input->playable_3d && !input->native_menu_or_bink_or_pause &&
      input->owner_instance_id && input->sdl3_identity_verified &&
      input->profile_verified && input->sampled_after_neutral &&
      input->old_di_source_isolated &&
      (input->normalized_sdl3_held &
       ~SPIDER_GENERIC_DIR_AND_ACTION_MASK)==0u;

  if(eligible) {
    output->source=SPIDER_GENERIC_SOURCE_PREVIEW;
    output->owner_instance_id=input->owner_instance_id;
    output->controller_contribution=input->normalized_sdl3_held;
  } else {
    output->controller_contribution=input->original_native_di_held;
  }
  output->held=output->keyboard_retained |
               output->controller_contribution;
  return eligible;
}
#endif
