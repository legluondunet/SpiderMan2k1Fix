/*
 * Adaptive widescreen gameplay HUD layout, grounded in Wine 1920x1080
 * instruction/compass traces and Windows 11 1920x1200 boss F12 traces:
 *
 *  - compass: two 64x64 textured mirrored halves (z=0, RHW=27.6000)
 *    and their untextured direction indicator (RHW=26.0678);
 *  - mission/instruction panels: untextured A0000060 / A0600000 backdrop,
 *    and small 16x8/16x32/32x32/64x32 glyph textures. The black
 *    shadow is z=0.0012/RHW=11.1764; the light foreground is a second
 *    pass at z=0.0010/RHW=12.6862, with ARGB FFC0C0FF.
 *    Both must undergo IDENTICAL X mapping to stay registered.
 *  - boss group(s): top-right health fills (textureless z≈.0010, opaque
 *    gradient colors), 32x32 border tiles and 64x64 portrait (z≈.0002),
 *    plus optional short black fill marker (z≈.0007). Scorpion/J. Jonah
 *    Jameson draw two such rows, Venom one. RHW changes across boss
 *    scenes, so ranges are required; exact caller/region/texture/size/
 *    color/layer combinations prevent transforming unrelated effects.
 *
 * The generic game DrawPrimitive caller RVA 0x00102C72 is NOT sufficient
 * to identify HUD: 3D world particles also go through it. Therefore we
 * require depth, RHW, texture type, region and dimensions together.
 * Only X changes: screen Y, depth/RHW, color and UV are bit preserved.
 */
#include "hud_adaptive.h"
#include <string.h>

#define HUD_CALLER_RVA 0x00102C72u
#define HUD_FVF        0x144u
#define HUD_TRI_FAN    6u /* D3DPT_TRIANGLEFAN */

static int hud_near(float value, float expected, float tolerance)
{
    return value >= expected - tolerance && value <= expected + tolerance;
}

int SpiderHudAdaptive_MightMatch(const SpiderMenuVertex *v)
{
    if (!v) return 0;
    return (hud_near(v[0].z, 0.0012f, 0.0001f) &&
            hud_near(v[0].rhw, 11.1764f, 0.10f)) ||
           (hud_near(v[0].z, 0.0010f, 0.0001f) &&
            hud_near(v[0].rhw, 12.6862f, 0.10f)) ||
           (hud_near(v[0].z, 0.0f, 0.0001f) &&
            (hud_near(v[0].rhw, 27.6000f, 0.10f) ||
             hud_near(v[0].rhw, 26.0678f, 0.10f))) ||
           /* Windows 11 boss trace: depth layers are stable, but RHW
            * varied slightly between Venom and Scorpion/Jameson. */
           (hud_near(v[0].z, 0.0002f, 0.00012f) &&
            v[0].rhw >= 19.0f && v[0].rhw <= 26.0f) ||
           (hud_near(v[0].z, 0.0010f, 0.00012f) &&
            v[0].rhw >= 10.0f && v[0].rhw <= 17.0f) ||
           (hud_near(v[0].z, 0.0007f, 0.00012f) &&
            v[0].rhw >= 13.0f && v[0].rhw <= 19.0f);
}

int SpiderHudAdaptive_Transform(const SpiderMenuVertex *src,
                                SpiderMenuVertex *dst,
                                unsigned count,
                                unsigned primitive_type, uint32_t fvf,
                                uint32_t caller_rva,
                                unsigned tex_w, unsigned tex_h,
                                unsigned screen_w, unsigned screen_h)
{
    float minx, maxx, miny, maxy, width, height;
    float scale, centerx, centery;
    int region = SPIDER_HUD_ADAPTIVE_NONE;
    unsigned i;
    uint32_t color;

    if (!src || !dst || src == dst || count != 4u ||
        primitive_type != HUD_TRI_FAN || fvf != HUD_FVF ||
        caller_rva != HUD_CALLER_RVA ||
        screen_w < 640u || screen_h < 480u ||
        screen_w > 16384u || screen_h > 16384u ||
        (uint64_t)screen_w * 3u <= (uint64_t)screen_h * 4u ||
        !SpiderHudAdaptive_MightMatch(src))
        return SPIDER_HUD_ADAPTIVE_NONE;

    minx = maxx = src[0].x;
    miny = maxy = src[0].y;
    for (i = 1; i != 4u; ++i) {
        if (src[i].x < minx) minx = src[i].x;
        if (src[i].x > maxx) maxx = src[i].x;
        if (src[i].y < miny) miny = src[i].y;
        if (src[i].y > maxy) maxy = src[i].y;
    }
    /* Comparisons also reject NaN. No out-of-bounds or full-screen comic. */
    if (!(minx >= -64.0f && miny >= -64.0f &&
          maxx <= (float)screen_w + 64.0f &&
          maxy <= (float)screen_h + 64.0f) ||
        !(maxx > minx && maxy > miny))
        return SPIDER_HUD_ADAPTIVE_NONE;

    width = maxx - minx;
    height = maxy - miny;
    centerx = (minx + maxx) * 0.5f;
    centery = (miny + maxy) * 0.5f;
    color = src[0].diffuse;

    /* Compass has its own depth layers. Restrict to right/bottom section.
     * 64x64 halves are horizontally mirrored in UV, not arbitrary sprites. */
    if (minx > (float)screen_w * 0.65f &&
        centerx > (float)screen_w * 0.75f &&
        miny > (float)screen_h * 0.70f &&
        maxy < (float)screen_h * 0.96f &&
        hud_near(src[0].z, 0.0f, 0.0001f)) {
        if (tex_w == 64u && tex_h == 64u &&
            hud_near(src[0].rhw, 27.6000f, 0.10f) &&
            width > (float)screen_h * 0.12f &&
            width < (float)screen_h * 0.30f &&
            height > (float)screen_h * 0.12f &&
            height < (float)screen_h * 0.24f &&
            ((hud_near(src[0].u, 0.95f, 0.05f) &&
              hud_near(src[2].u, 0.05f, 0.05f)) ||
             (hud_near(src[0].u, 0.05f, 0.05f) &&
              hud_near(src[2].u, 0.95f, 0.05f))))
            region = SPIDER_HUD_ADAPTIVE_COMPASS;
        else if (tex_w == 0u && tex_h == 0u &&
                 hud_near(src[0].rhw, 26.0678f, 0.10f) &&
                 width < (float)screen_h * 0.19f &&
                 height < (float)screen_h * 0.19f)
            region = SPIDER_HUD_ADAPTIVE_COMPASS;
    }

    /* Both blue and red backdrops share the shadow-pass depth and RHW.
     * Text is two-pass: black shadow z=.0012/RHW=11.1764, foreground
     * lavender z=.0010/RHW=12.6862 (confirmed in Wine F12 trace).
     * Map both to the same X coordinates or bright text overruns its box.
     * Never accept player HUD upper-left glyphs, 3D world particles
     * (z~0.9983/RHW~0.0229), or unrelated sprites. */
    if (region == SPIDER_HUD_ADAPTIVE_NONE &&
        minx >= -64.0f &&
        maxx < (float)screen_w * 0.74f &&
        miny > (float)screen_h * 0.68f &&
        maxy < (float)screen_h * 0.96f &&
        ((hud_near(src[0].z, 0.0012f, 0.0001f) &&
          hud_near(src[0].rhw, 11.1764f, 0.10f)) ||
         (hud_near(src[0].z, 0.0010f, 0.0001f) &&
          hud_near(src[0].rhw, 12.6862f, 0.10f)))) {
        if (hud_near(src[0].z, 0.0012f, 0.0001f) &&
            hud_near(src[0].rhw, 11.1764f, 0.10f) &&
            tex_w == 0u && tex_h == 0u &&
            (color == 0xA0000060u || color == 0xA0600000u) &&
            width > (float)screen_w * 0.22f &&
            width < (float)screen_w * 0.72f &&
            height >= (float)screen_h * 0.04f &&
            height < (float)screen_h * 0.16f)
            region = SPIDER_HUD_ADAPTIVE_INSTRUCTIONS;
        else if ((tex_w == 16u || tex_w == 32u || tex_w == 64u) &&
                 (tex_h == 8u || tex_h == 32u) &&
                 width >= 4.0f && width < (float)screen_h * 0.09f &&
                 height >= 4.0f && height < (float)screen_h * 0.09f &&
                 ((color == 0xFF000000u &&
                   hud_near(src[0].z, 0.0012f, 0.0001f) &&
                   hud_near(src[0].rhw, 11.1764f, 0.10f)) ||
                  (color == 0xFFC0C0FFu &&
                   hud_near(src[0].z, 0.0010f, 0.0001f) &&
                   hud_near(src[0].rhw, 12.6862f, 0.10f))))
            region = SPIDER_HUD_ADAPTIVE_INSTRUCTIONS;
    }

    /* Upper-right boss UI. Venom is a single bar/portrait; Scorpion
     * also draws a shorter Jameson health bar underneath. Preserve the
     * exact same right-edge anchor for every fill, border tile, marker
     * and portrait. See native 1920x1200 F12 measurements 2026-10-21.
     *
     * Do not rely on RHW exact equality: Venom used 21.3309 and 12.6862
     * while Scorpion/Jameson used 21.9693 and 13.6283. Their color,
     * layer, texture dimensions, quad extent and screen region together
     * form a safer composite signature than any single property. */
    if (region == SPIDER_HUD_ADAPTIVE_NONE &&
        minx >= (float)screen_w * 0.48f &&
        centerx > (float)screen_w * 0.53f &&
        miny >= (float)screen_h * 0.045f &&
        maxy <= (float)screen_h * 0.34f &&
        (src[0].diffuse >> 24) == 0xFFu) {
        /* Solid gradient health segments, either of the two rows.
         * Source screenshot: 40px high at h=1200; width depends on HP. */
        if (tex_w == 0u && tex_h == 0u &&
            hud_near(src[0].z, 0.0010f, 0.00012f) &&
            src[0].rhw >= 10.0f && src[0].rhw <= 17.0f &&
            height >= (float)screen_h * 0.026f &&
            height <= (float)screen_h * 0.041f &&
            width >= (float)screen_h * 0.008f &&
            width <= (float)screen_h * 0.56f &&
            (color & 0x00FFFFFFu) != 0u &&
            ((color & 0x0000FF00u) != 0u ||
             (color & 0x00FF0000u) != 0u))
            region = SPIDER_HUD_ADAPTIVE_BOSS;
        /* Black missing-health mask. In the Scorpion+Jameson trace it
         * occupied just 15px, but its width is DYNAMIC: after Venom takes
         * damage it can occupy hundreds of pixels. The previous <=4% H
         * bound left that larger mask at its original x=1080 while all
         * frame/fill/portrait quads moved right, producing a detached
         * black rectangle outside the corrected bar (2026-10-21 Windows
         * screenshot). Permit the full measured boss-bar width, but keep
         * the exact textureless opaque-black layer, depth/RHW, narrow
         * health-bar height and one of the two measured boss rows. */
        else if (tex_w == 0u && tex_h == 0u &&
                 color == 0xFF000000u &&
                 hud_near(src[0].z, 0.0007f, 0.00012f) &&
                 src[0].rhw >= 13.0f && src[0].rhw <= 19.0f &&
                 width >= (float)screen_h * 0.006f &&
                 width <= (float)screen_h * 0.56f &&
                 height >= (float)screen_h * 0.025f &&
                 height <= (float)screen_h * 0.041f &&
                 ((miny >= (float)screen_h * 0.10f &&
                   miny <= (float)screen_h * 0.15f) ||
                  (miny >= (float)screen_h * 0.22f &&
                   miny <= (float)screen_h * 0.27f)))
            region = SPIDER_HUD_ADAPTIVE_BOSS;
        /* 32x32 tiled frame segments, including shorter Jameson bar.
         * Portrait sprites are 64x64; both share foreground z/RHW. */
        else if (color == 0xFFFFFFFFu &&
                 hud_near(src[0].z, 0.0002f, 0.00012f) &&
                 src[0].rhw >= 19.0f && src[0].rhw <= 26.0f &&
                 ((tex_w == 32u && tex_h == 32u &&
                   width >= (float)screen_h * 0.045f &&
                   width <= (float)screen_h * 0.095f &&
                   height >= (float)screen_h * 0.055f &&
                   height <= (float)screen_h * 0.075f) ||
                  (tex_w == 64u && tex_h == 64u &&
                   width >= (float)screen_h * 0.125f &&
                   width <= (float)screen_h * 0.16f &&
                   height >= (float)screen_h * 0.115f &&
                   height <= (float)screen_h * 0.14f)))
            region = SPIDER_HUD_ADAPTIVE_BOSS;
    }

    if (region == SPIDER_HUD_ADAPTIVE_NONE)
        return region;

    scale = (4.0f * (float)screen_h) / (3.0f * (float)screen_w);
    memcpy(dst, src, 4u * sizeof(*dst));
    for (i = 0; i != 4u; ++i) {
        if (region == SPIDER_HUD_ADAPTIVE_COMPASS ||
            region == SPIDER_HUD_ADAPTIVE_BOSS)
            dst[i].x = (float)screen_w + (src[i].x - (float)screen_w) * scale;
        else
            dst[i].x = src[i].x * scale;
    }
    (void)centery; /* Coordinates are kept here for future layout diagnostics. */
    return region;
}
