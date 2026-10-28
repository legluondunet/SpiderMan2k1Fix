#ifndef SPIDERMAN2K1FIX_NATIVE_JOYSTICK_RECONNECT_POLICY_H
#define SPIDERMAN2K1FIX_NATIVE_JOYSTICK_RECONNECT_POLICY_H
#include <stdint.h>
#define SPIDER_RECONNECT_INTERVAL_MS 2000u
#define SPIDER_DS4_VID 0x054cu
static inline int spider_reconnect_sony_ds4(unsigned vid,unsigned pid) {
  return vid==SPIDER_DS4_VID && (pid==0x05c4u || pid==0x09ccu || pid==0x0ba0u);
}
static inline int spider_reconnect_scan_due(uint32_t now,uint32_t last,int started) {
  return !started || (uint32_t)(now-last)>=SPIDER_RECONNECT_INTERVAL_MS;
}
static inline int spider_reconnect_eligible(int enabled,int guard_installed,
                                            int foreground,int native_factory) {
  return enabled && guard_installed && foreground && native_factory;
}
#endif
