/* Verified, fail-closed DirectInput product identity selection.
 * VID/PID from DIDEVICEINSTANCEA.guidProduct.Data1, not user-provided
 * marketing names. A clone can expose the same ID; identify compatible
 * hardware, never claim authenticated manufacturer identity.
 *
 * Xbox 360-compatible wired controller: 045E:028E. This includes
 * third-party hardware reusing the Microsoft VID/PID (e.g., Holtek).
 * Wireless adapter 045E:0719 is intentionally NOT accepted here:
 * a receiver USB ID does not identify an attached controller instance.
 * Discover only DI8DEVCLASS_GAMECTRL and exactly one device.
 */
#ifndef SPIDER_CONTROLLER_DEVICE_IDENTITY_H
#define SPIDER_CONTROLLER_DEVICE_IDENTITY_H
enum {SPIDER_PAD_NONE=0, SPIDER_PAD_DS4=1, SPIDER_PAD_XBOX360=2};
static inline unsigned spider_pad_identify(unsigned vid,unsigned pid) {
  if(vid==0x054cu && (pid==0x05c4u || pid==0x09ccu || pid==0x0ba0u))
    return SPIDER_PAD_DS4;
  if(vid==0x045eu && pid==0x028eu)
    return SPIDER_PAD_XBOX360;
  return SPIDER_PAD_NONE;
}
/* Xbox DirectInput's conventional indices are a candidate, not a
 * cross-OS ABI. XInput is the standard API for guaranteed A/B identity.
 * Keep the Xbox UI bridge opt-in until the reported DI indices have
 * been checked on Wine and Windows hardware. */
enum {SPIDER_XBOX_DI_A=0u,SPIDER_XBOX_DI_B=1u};
static inline unsigned spider_xbox_2d_edges(unsigned held,unsigned prev) {
  return held & ~prev & 0x3u;
}
#endif
