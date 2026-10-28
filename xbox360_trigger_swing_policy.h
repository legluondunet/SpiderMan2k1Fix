/* Experimental XUSB-style shared trigger axis -> digital Swing gate.
 *
 * Holtek 045E:028E Wine trace, 2026-10-28:
 *  neutral lZ=32768, first tested trigger lZ=128 (negative),
 *  opposite trigger lZ=65408 (positive).
 * The maintainer was asked to press RT before LT; RT=negative is an
 * evidence-based hypothesis until they explicitly confirm press order.
 *
 * Never read either trigger as a button. Hysteresis avoids flicker at
 * boundaries. The positive side (LT candidate) ALWAYS releases Swing.
 * No global state, Win32 calls or native game modifications in this helper.
 */
#ifndef SPIDERMAN2K1FIX_XBOX360_TRIGGER_SWING_POLICY_H
#define SPIDERMAN2K1FIX_XBOX360_TRIGGER_SWING_POLICY_H
static inline int spider_xbox360_rt_swing_held(int was_held,long lz) {
  if(lz<0L || lz>65535L)return 0;
  /* roughly 33% depression to press, <15% to release */
  return was_held ? lz<28000L : lz<=22000L;
}
/* Independent SDL3 RT axis, 0 released .. 32767 fully depressed.
 * Keep legacy lZ thresholds untouched for older single-pad profiles.
 * Only called for the verified currently selected Xbox in native-owner
 * mode. Hysteresis avoids repeated presses around the trigger deadzone. */
static inline int spider_xbox360_sdl3_rt_swing_held(int was_held,int rt) {
  if(rt<0 || rt>32767)return 0;
  return was_held ? rt>=5000 : rt>=11000;
}
/* Hardware/driver compatibility: retain the previously validated XUSB
 * lZ fallback, OR use selected SDL3 normalized RT when available. The
 * caller must have strictly verified Xbox native owner before supplying
 * a valid SDL3 RT sample. A positive LT-only lZ must never engage RT. */
static inline int spider_xbox360_rt_swing_held_any(
    int was_held,long lz,int has_sdl3_rt,int normalized_rt) {
  return spider_xbox360_rt_swing_held(was_held,lz) ||
         (has_sdl3_rt &&
          spider_xbox360_sdl3_rt_swing_held(was_held,normalized_rt));
}
#endif
