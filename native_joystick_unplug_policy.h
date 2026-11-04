#ifndef SPIDERMAN2K1FIX_NATIVE_JOYSTICK_UNPLUG_POLICY_H
#define SPIDERMAN2K1FIX_NATIVE_JOYSTICK_UNPLUG_POLICY_H

/* Portable, tiny policy used to review the exact native x86 stub.
 * Do NOT suppress all failed HRESULTs. Windows and Wine use the same
 * DirectInput DIERR_UNPLUGGED numeric value. */
#include <stdint.h>
#define SPIDER_DIERR_UNPLUGGED 0x80040209u
#define SPIDER_NATIVE_JOYSTICK_BUTTON_BYTES 32u

static inline int spider_native_unplug_recoverable(uint32_t hr) {
  return hr==SPIDER_DIERR_UNPLUGGED;
}
#endif
