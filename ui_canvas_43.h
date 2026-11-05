#ifndef SPIDERMAN2K1FIX_UI_CANVAS_43_H
#define SPIDERMAN2K1FIX_UI_CANVAS_43_H

/* Experimental common 4:3 canvas for *screen-space* native 3D-gameplay UI.
 *
 * Source vertices are the original, uncorrected game-owned D3D7 XYZRHW
 * quads, already stretched across a physical widescreen viewport. For a
 * target W x H wider than 4:3, restore their original 4:3 X positions:
 *
 *       x' = W/2 + (x-W/2) * (4H/3W).
 *
 * Y, Z, RHW, colour, UV and draw order are untouched. This is a pure,
 * independent out-of-place transformation, never a D3D7 state change.
 *
 * First trial: a conservative *candidate* screen-space overlay gate. Some
 * world sprites share DrawPrimitive caller RVA 0x00102C72. Until a verified
 * native UI/3D ownership boundary is found, the gate is intentionally
 * opt-in and can leave unknown graphics unchanged. DO NOT promote this
 * classifier to a proven universal owner or turn it on by default.
 *
 * Standalone 640x480 menus already use SpiderMenu_MakeMapping and must NOT
 * pass through this pipeline. A reticle is tied to a 3D projected target:
 * preserve its centre and use the separately verified reticle transform.
 */
#include "menu_scale.h"
#include <stdint.h>
#include <string.h>

#define SPIDER_UI_CANVAS_43_CALLER_RVA 0x00102C72u

static inline float SpiderUICanvas_Abs(float x)
{
    return x < 0.0f ? -x : x;
}

static inline int SpiderUICanvas_43Transform(const SpiderMenuVertex *src,
                  SpiderMenuVertex *dst, unsigned count,
                  unsigned primitive_type, uint32_t fvf,
                  uint32_t caller_rva, unsigned screen_w,
                  unsigned screen_h)
{
    float minx, maxx, miny, maxy, scale, z0, rhw0;
    unsigned i;

    if (!src || !dst || src == dst || count != 4u ||
        primitive_type != 6u /* D3DPT_TRIANGLEFAN */ ||
        fvf != 0x144u /* D3DFVF_XYZRHW|DIFFUSE|TEX1 */ ||
        caller_rva != SPIDER_UI_CANVAS_43_CALLER_RVA ||
        screen_w < 640u || screen_h < 480u ||
        screen_w > 16384u || screen_h > 16384u ||
        (uint64_t)screen_w * 3u <= (uint64_t)screen_h * 4u)
        return 0;

    /* These two translucent native aim sprites require LOCAL horizontal
     * correction about their own centre, not a screen-centre relocation.
     * Other target-locked sprites remain a known investigation area. */
    if (src[0].diffuse == 0xB0FF4040u ||
        src[0].diffuse == 0xB040FF40u)
        return 0;

    minx = maxx = src[0].x;
    miny = maxy = src[0].y;
    z0 = src[0].z;
    rhw0 = src[0].rhw;
    for (i = 0u; i < 4u; ++i) {
        /* Known game screen-space overlays use near-zero postprojection Z
         * and RHW significantly above world geometry. Reject world-like,
         * inconsistent and non-finite vertices. This is NOT an exhaustive
         * UI classifier, only a safe trial candidate filter. */
        if (!(src[i].z >= 0.0f && src[i].z <= 0.010f &&
              src[i].rhw >= 2.0f && src[i].rhw <= 64.0f &&
              SpiderUICanvas_Abs(src[i].z-z0) <= 0.00015f &&
              SpiderUICanvas_Abs(src[i].rhw-rhw0) <= 0.15f))
            return 0;
        /* Explicit comparisons reject NaN/Inf XY coordinates, which
         * otherwise evade min/max reductions and may poison a draw. */
        if (!(src[i].x >= -(float)screen_w * 0.05f &&
              src[i].x <= (float)screen_w * 1.05f &&
              src[i].y >= -(float)screen_h * 0.05f &&
              src[i].y <= (float)screen_h * 1.05f))
            return 0;
        if (src[i].x < minx) minx = src[i].x;
        if (src[i].x > maxx) maxx = src[i].x;
        if (src[i].y < miny) miny = src[i].y;
        if (src[i].y > maxy) maxy = src[i].y;
    }
    if (!(minx >= -(float)screen_w * 0.05f &&
          maxx <= (float)screen_w * 1.05f &&
          miny >= -(float)screen_h * 0.05f &&
          maxy <= (float)screen_h * 1.05f &&
          maxx > minx && maxy > miny))
        return 0;

    scale = (4.0f * (float)screen_h) / (3.0f * (float)screen_w);
    memcpy(dst, src, 4u * sizeof(*dst));
    for (i = 0u; i < 4u; ++i)
        dst[i].x = 0.5f * (float)screen_w +
                   (src[i].x - 0.5f * (float)screen_w) * scale;
    return 1;
}

#endif
