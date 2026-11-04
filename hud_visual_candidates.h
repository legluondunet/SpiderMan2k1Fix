#ifndef SPIDERMAN2K1FIX_HUD_VISUAL_CANDIDATES_H
#define SPIDERMAN2K1FIX_HUD_VISUAL_CANDIDATES_H

/* Read-only, deliberately broad candidate sampler for HUD investigation.
 * Classification is NOT proof of object identity and NEVER edits vertices.
 * Exact caller, Z/RHW, texture and color are captured in diagnostic logs
 * for later hardware-backed refinement. Keep separated from the actual
 * strict, SHA-gated adaptive and native ESC geometry corrections. */
#include "menu_scale.h"
#include <stdint.h>

enum SpiderHudVisualCandidate {
    SPIDER_HUD_VISUAL_NONE = 0,
    SPIDER_HUD_VISUAL_AIM = 1,
    SPIDER_HUD_VISUAL_PAUSE_RIBBON = 2
};

static inline int SpiderHudVisual_Classify(
    const SpiderMenuVertex *v, unsigned count,
    unsigned primitive_type, uint32_t fvf,
    unsigned screen_w, unsigned screen_h)
{
    float left, right, top, bottom, width, height, centerx, centery;
    unsigned i;

    if (!v || count != 4u || primitive_type != 6u ||
        fvf != 0x144u ||
        screen_w < 640u || screen_h < 480u ||
        screen_w > 16384u || screen_h > 16384u ||
        (uint64_t)screen_w * 3u <= (uint64_t)screen_h * 4u)
        return SPIDER_HUD_VISUAL_NONE;

    left = right = v[0].x;
    top = bottom = v[0].y;
    for (i = 1u; i != 4u; ++i) {
        if (v[i].x < left) left = v[i].x;
        if (v[i].x > right) right = v[i].x;
        if (v[i].y < top) top = v[i].y;
        if (v[i].y > bottom) bottom = v[i].y;
    }
    /* Reject invalid, wildly out-of-screen and zero-area quads. The
     * comparisons also fail closed for NaN in the original geometry. */
    if (!(left >= -(float)screen_w * 0.10f &&
          right <= (float)screen_w * 1.10f &&
          top >= -(float)screen_h * 0.10f &&
          bottom <= (float)screen_h * 1.10f &&
          right > left && bottom > top))
        return SPIDER_HUD_VISUAL_NONE;
    width = right - left;
    height = bottom - top;
    centerx = (left + right) * 0.5f;
    centery = (top + bottom) * 0.5f;

    /* Typical large native ESC ribbon: deliberately tolerate other
     * level-specific dimensions and layers, so we can learn why the
     * existing exact Z/RHW/banner match failed. Not a transformation. */
    if (width >= (float)screen_w * 0.78f &&
        width <= (float)screen_w * 1.15f &&
        top >= (float)screen_h * 0.22f &&
        top <= (float)screen_h * 0.60f &&
        bottom >= (float)screen_h * 0.49f &&
        bottom <= (float)screen_h * 0.82f &&
        height >= (float)screen_h * 0.12f &&
        height <= (float)screen_h * 0.42f)
        return SPIDER_HUD_VISUAL_PAUSE_RIBBON;

    /* The first GE-Proton 2026-11-04 read-only samples were polluted
     * by a confirmed untextured ESC motif layer: alpha-0x80 dark
     * red/teal quads at Z=0.00146 RHW=7.8952; these are NOT the
     * green/red aiming rings. Similarly an invisible untextured quad
     * with diffuse=0 and RHW=27.6 is not a visible reticle.
     * Exclude just the measured signatures, not unknown gamepads/UI. */
    if ((v[0].diffuse == 0x80208686u ||
         v[0].diffuse == 0x80041010u) &&
        v[0].z > 0.00142f && v[0].z < 0.00150f &&
        v[0].rhw > 7.84f && v[0].rhw < 7.96f)
        return SPIDER_HUD_VISUAL_NONE;
    if (v[0].diffuse == 0u &&
        v[0].z > -0.00005f && v[0].z < 0.00005f &&
        v[0].rhw > 27.5f && v[0].rhw < 27.7f)
        return SPIDER_HUD_VISUAL_NONE;

    /* Visually documented aiming rings span ~148x106 or 172x135 at
     * 1920x1080. The previous broad 45px gate emitted ONLY false AIM
     * samples for the player health/web icon's 32x32 sprite
     * (x=292.5..345.9, y=94.5..162, z=.00024, rhw=19.4920)
     * in the maintainer's level-2 Pause session. Restrict this
     * read-only *candidate* region away from the upper-left HUD without
     * guessing reticle colors, textures or D3D7 depth signatures. */
    if (width >= (float)screen_h * 0.065f &&
        width <= (float)screen_h * 0.35f &&
        height >= (float)screen_h * 0.065f &&
        height <= (float)screen_h * 0.35f &&
        width >= height * 0.70f &&
        width <= height * 2.50f &&
        centerx >= (float)screen_w * 0.25f &&
        centerx <= (float)screen_w * 0.90f &&
        centery >= (float)screen_h * 0.14f &&
        centery <= (float)screen_h * 0.94f)
        return SPIDER_HUD_VISUAL_AIM;

    return SPIDER_HUD_VISUAL_NONE;
}
#endif
