#ifndef SPIDERMAN2K1FIX_NATIVE_MENU_RETURN_GATE_H
#define SPIDERMAN2K1FIX_NATIVE_MENU_RETURN_GATE_H
/*
 * Original game's request 7 (leave 3D -> real main menu) retains ownership
 * of any physical CONFIRM buttons still down on the first returned-menu
 * sample. Those buttons become eligible for SYNTHETIC Return only after
 * their own release; there is no arbitrary time/frame deadline.
 *
 * Pure C99, independent of Sony/Xbox VID/PID, SendInput and Win32.
 * Both controller adapters share exactly the same ownership policy.
 * This gate cannot suppress the original game's DirectInput reads:
 * a native-input root cause requires separately verified evidence.
 */
typedef struct {
  unsigned seen_3d;
  unsigned inherited_buttons;
} SpiderNativeMenuReturnGate;

static inline unsigned spider_native_menu_return_gate_flip(
    SpiderNativeMenuReturnGate *owner,
    int enabled,int menu2d,int gameplay3d,int movie,
    unsigned native_request,unsigned pressed_confirm_mask) {
  if(!owner)return 0;
  if(!enabled) {
    owner->seen_3d=0u;
    owner->inherited_buttons=0u;
    return 0;
  }
  if(gameplay3d && !menu2d && !movie) {
    owner->seen_3d=1u;
    owner->inherited_buttons=0u;
    return 0;
  }
  if(menu2d && !gameplay3d && !movie) {
    if(owner->seen_3d) {
      owner->seen_3d=0u;
      if(native_request==7u)
        owner->inherited_buttons=pressed_confirm_mask;
    }
    owner->inherited_buttons &= pressed_confirm_mask;
    return owner->inherited_buttons;
  }
  return 0;
}
#endif
