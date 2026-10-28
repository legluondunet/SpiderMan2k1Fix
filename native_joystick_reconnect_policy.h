#ifndef SPIDERMAN2K1FIX_NATIVE_JOYSTICK_RECONNECT_POLICY_H
#define SPIDERMAN2K1FIX_NATIVE_JOYSTICK_RECONNECT_POLICY_H
#include <stdint.h>
#define SPIDER_RECONNECT_INTERVAL_MS 2000u
/* Native DirectInput recovery supports known original gamepads.
 * XInput gamepads normally also expose a DirectInput device. This is
 * NOT an XInput-to-DirectInput emulation layer: its ABI/mapping comes
 * from the OS/Wine DirectInput driver and must be hardware-validated.
 * USB product IDs vary for third-party/virtual controllers; unknown
 * products are deliberately rejected rather than guessed. */
#define SPIDER_DS4_VID 0x054cu
#define SPIDER_XBOX360_VID 0x045eu
enum { SPIDER_RECONNECT_NONE=0, SPIDER_RECONNECT_DS4=1,
       SPIDER_RECONNECT_XBOX360=2 };
static inline int spider_reconnect_sony_ds4(unsigned vid,unsigned pid) {
  return vid==SPIDER_DS4_VID && (pid==0x05c4u || pid==0x09ccu || pid==0x0ba0u);
}
static inline int spider_reconnect_xbox360(unsigned vid,unsigned pid) {
  /* Microsoft wired Xbox 360 (028E), wireless Xbox 360 (028F);
   * 02A1 appears as a wireless receiver-backed controller device
   * on some Windows/Wine driver stacks. 0719 is a receiver interface,
   * NOT necessarily a joystick; intentionally reject it here. */
  return vid==SPIDER_XBOX360_VID &&
         (pid==0x028eu || pid==0x028fu || pid==0x02a1u);
}
static inline int spider_reconnect_kind(unsigned vid,unsigned pid) {
  if(spider_reconnect_sony_ds4(vid,pid))return SPIDER_RECONNECT_DS4;
  if(spider_reconnect_xbox360(vid,pid))return SPIDER_RECONNECT_XBOX360;
  return SPIDER_RECONNECT_NONE;
}
static inline int spider_reconnect_scan_due(uint32_t now,uint32_t last,int started) {
  return !started || (uint32_t)(now-last)>=SPIDER_RECONNECT_INTERVAL_MS;
}
static inline int spider_reconnect_eligible(int enabled,int guard_installed,
                                            int foreground,int native_factory) {
  return enabled && guard_installed && foreground && native_factory;
}
#endif
