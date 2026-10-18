#ifndef SPIDERMAN2K1FIX_DISPLAY_POLICY_H
#define SPIDERMAN2K1FIX_DISPLAY_POLICY_H
#include <stddef.h>
/* Strictly accepts Auto or WIDTHxHEIGHT. The caller provides the desktop
 * resolution captured before exclusive fullscreen changes it. */
int SpiderDisplayPolicy_Resolve(const char *setting,
    unsigned native_width, unsigned native_height,
    unsigned *width, unsigned *height);
/* Apply the requested display aspect to the selected output dimensions.
 * 4:3 uses the largest fitting 4:3 mode, not a cropped 16:9 framebuffer.
 * Unknown ratios are rejected; Auto leaves the selected resolution alone.
 */
int SpiderDisplayPolicy_ResolveAspect(const char *resolution,const char *aspect,
    unsigned native_width,unsigned native_height,
    unsigned *width,unsigned *height);
/* Runtime LOGICAL framebuffer resolution. Forced 4:3 fits within the
 * requested physical resolution; it NEVER enumerates physical 4:3 modes. */
int SpiderDisplayPolicy_ResolveRuntime(const char *resolution,const char *aspect,
    unsigned native_width,unsigned native_height,
    unsigned *width,unsigned *height);

typedef struct {unsigned width,height,depth;} SpiderDisplayMode;
int SpiderDisplayPolicy_SelectAvailable43(unsigned desired_width,unsigned desired_height,
    unsigned native_width,unsigned native_height,
    const SpiderDisplayMode *modes,size_t count,unsigned *width,unsigned *height);
/* The game uses 32-bit little-endian settings: W at 0, H at 4, depth at 8.
 * Never touch the remaining controls/audio/unidentified configuration. */
int SpiderDisplayPolicy_Enforce(unsigned char *data, size_t length,
    unsigned width, unsigned height);
#endif
