/*
 * Adaptive widescreen layout for two previously uncorrected Spider-Man 2001
 * game HUD groups, grounded in three Wine 1920x1080 / F12 D3D7 captures:
 *
 *  - compass: two 64x64 textured mirrored halves (z=0, RHW=27.6000)
 *    and their untextured direction indicator (RHW=26.0678);
 *  - mission/instruction panels: untextured A0000060 / A0600000 backdrop,
 *    and small 16x8/16x32/32x32/64x32 glyph textures sharing the
 *    z=0.0012 / RHW=11.1764 source.
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
    return (hud_near(v[0].z, 0.0012f, 0.0002f) &&
            hud_near(v[0].rhw, 11.1764f, 0.10f)) ||
           (hud_near(v[0].z, 0.0f, 0.0001f) &&
            (hud_near(v[0].rhw, 27.6000f, 0.10f) ||
             hud_near(v[0].rhw, 26.0678f, 0.10f)));
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

    /* Both blue and red instruction backdrops; glyphs share exact depth
     * and RHW, even where their X extends into the probe's "compass" region.
     * Never classify the 3D decorative particles (z~0.9983, RHW~0.0229). */
    if (region == SPIDER_HUD_ADAPTIVE_NONE &&
        minx >= -64.0f &&
        maxx < (float)screen_w * 0.74f &&
        miny > (float)screen_h * 0.68f &&
        maxy < (float)screen_h * 0.96f &&
        hud_near(src[0].z, 0.0012f, 0.0002f) &&
        hud_near(src[0].rhw, 11.1764f, 0.10f)) {
        if (tex_w == 0u && tex_h == 0u &&
            (color == 0xA0000060u || color == 0xA0600000u) &&
            width > (float)screen_w * 0.22f &&
            width < (float)screen_w * 0.72f &&
            height >= (float)screen_h * 0.04f &&
            height < (float)screen_h * 0.16f)
            region = SPIDER_HUD_ADAPTIVE_INSTRUCTIONS;
        else if ((tex_w == 16u || tex_w == 32u || tex_w == 64u) &&
                 (tex_h == 8u || tex_h == 32u) &&
                 (color == 0xFF000000u || color == 0xFFC0C0FFu) &&
                 width >= 4.0f && width < (float)screen_h * 0.09f &&
                 height >= 4.0f && height < (float)screen_h * 0.09f)
            region = SPIDER_HUD_ADAPTIVE_INSTRUCTIONS;
    }

    if (region == SPIDER_HUD_ADAPTIVE_NONE)
        return region;

    scale = (4.0f * (float)screen_h) / (3.0f * (float)screen_w);
    memcpy(dst, src, 4u * sizeof(*dst));
    for (i = 0; i != 4u; ++i) {
        if (region == SPIDER_HUD_ADAPTIVE_COMPASS)
            dst[i].x = (float)screen_w + (src[i].x - (float)screen_w) * scale;
        else
            dst[i].x = src[i].x * scale;
    }
    (void)centery; /* Coordinates are kept here for future layout diagnostics. */
    return region;
}
