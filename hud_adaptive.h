#ifndef SPIDERMAN2K1FIX_HUD_ADAPTIVE_H
#define SPIDERMAN2K1FIX_HUD_ADAPTIVE_H

#include "menu_scale.h"
#include <stdint.h>

/* Pure 2D gameplay HUD filter/transform. No Windows headers, memory patching,
 * texture reads or side effects. Caller uses the original game-owned vertices;
 * dst is a separate four-vertex scratch buffer. All attributes except X are
 * bit-preserved. Return 0 on any uncertain match, preserving the original draw.
 */
enum SpiderHudAdaptiveRegion {
    SPIDER_HUD_ADAPTIVE_NONE = 0,
    SPIDER_HUD_ADAPTIVE_INSTRUCTIONS = 1,
    SPIDER_HUD_ADAPTIVE_COMPASS = 2,
    SPIDER_HUD_ADAPTIVE_BOSS = 3
};

/* Cheap prefilter before querying a D3D7 texture. */
int SpiderHudAdaptive_MightMatch(const SpiderMenuVertex *vertices);

/* Tested source EXE only; caller additionally requires its SHA-gated HUD
 * setting to be enabled and gameplay (not movies/menus) to be active. */
int SpiderHudAdaptive_Transform(const SpiderMenuVertex *src,
                                SpiderMenuVertex *dst,
                                unsigned count,
                                unsigned primitive_type, uint32_t fvf,
                                uint32_t caller_rva,
                                unsigned tex_w, unsigned tex_h,
                                unsigned screen_w, unsigned screen_h);

#endif
