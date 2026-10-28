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
#endif
