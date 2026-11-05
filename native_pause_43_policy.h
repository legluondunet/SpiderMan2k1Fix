#ifndef SPIDERMAN2K1FIX_NATIVE_PAUSE_43_POLICY_H
#define SPIDERMAN2K1FIX_NATIVE_PAUSE_43_POLICY_H

/* Pure geometry policy for the original game's 4:3 ESC artwork.
 * The world remains full-width. Only exact, independently verified D3D7
 * native pause signatures may use this mapping; these helpers do not
 * classify textures, alter vertices or interact with Windows state. */
#include <stdint.h>

static inline int SpiderNativePause43_Widescreen(unsigned w,unsigned h)
{
    return w>=640u && w<=16384u &&
           h>=480u && h<=16384u &&
           (uint64_t)w*3u > (uint64_t)h*4u;
}

/* The preserved menu is always centered, irrespective of resolution.
 * At 1920x1080: 4:3 content width=1440; scale=.75.
 * At 1920x1200: width=1600; scale=5/6.
 * At 2560x1440: width=1920; scale=.75.
 * At the native 4:3 aspect: unchanged (scale=1). */
static inline float SpiderNativePause43_Scale(unsigned w,unsigned h)
{
    if(!SpiderNativePause43_Widescreen(w,h)) return 1.0f;
    return (4.0f*(float)h)/(3.0f*(float)w);
}

static inline float SpiderNativePause43_MapX(
    float x,unsigned w,unsigned h)
{
    float center=(float)w*0.5f;
    return center+(x-center)*SpiderNativePause43_Scale(w,h);
}

/* Three independently observed, tightly isolated native ESC depth layers.
 * The original game varies RHW with game context; the same original
 * caller, geometry, texture state and depth still must match in the hook.
 *  - earlier 1920x1080 validated ESC: ribbon 11.176, motif 9.988
 *  - 2026-11-04 GE-Proton second scene: ribbon 8.9615, motif 7.8952
 *  - 2026-11-05 Rhino boss: ribbon 6.8041, motif 5.9130
 * Never interpolate between these readings or apply to generic HUD.
 * Comparisons fail closed for NaN without depending on libm. */
static inline float SpiderNativePause43_Abs(float x)
{
    return x < 0.0f ? -x : x;
}
static inline int SpiderNativePause43_RibbonLayer(float z,float rhw)
{
    return SpiderNativePause43_Abs(z-0.00122f)<0.00010f &&
           (SpiderNativePause43_Abs(rhw-11.176f)<0.25f ||
            SpiderNativePause43_Abs(rhw-8.9615f)<0.060f ||
            SpiderNativePause43_Abs(rhw-6.8041f)<0.060f);
}
static inline int SpiderNativePause43_MotifLayer(float z,float rhw)
{
    return SpiderNativePause43_Abs(z-0.00146f)<0.00004f &&
           (SpiderNativePause43_Abs(rhw-9.988f)<0.060f ||
            SpiderNativePause43_Abs(rhw-7.8952f)<0.060f ||
            SpiderNativePause43_Abs(rhw-5.9130f)<0.060f);
}

/* Shadow and face of native Pause letters have three independently
 * measured source-depth pairs:
 *  - original: shadow z=.00122/rhw=11.176, face z=.00098/rhw=12.686
 *  - second scene: shadow 8.9615, face 10.3609
 *  - Rhino boss: shadow 6.8041, face 8.0113.
 * Rhino scrn0045.png / Log=1 (2026-11-05 01:59:35) confirms 48
 * corrected black-shadow samples and 32 missed colored-face samples
 * (FFFE3242 title, FF8A78D6 options) at z=.00098/rhw=8.0113.
 * Never infer arbitrary RHW from other objects or use an interval
 * between discrete source variants: the original game caller,
 * same-Flip verified ribbon, 16/32/64x32 texture and tight glyph
 * geometry/position gates must all agree before mapping. */
static inline int SpiderNativePause43_TextLayer(float z,float rhw)
{
    return (SpiderNativePause43_Abs(z-0.00122f)<0.00010f &&
            (SpiderNativePause43_Abs(rhw-11.176f)<0.30f ||
             SpiderNativePause43_Abs(rhw-8.9615f)<0.060f ||
             SpiderNativePause43_Abs(rhw-6.8041f)<0.060f)) ||
           (SpiderNativePause43_Abs(z-0.00098f)<0.00010f &&
            (SpiderNativePause43_Abs(rhw-12.686f)<0.30f ||
             SpiderNativePause43_Abs(rhw-10.3609f)<0.060f ||
             SpiderNativePause43_Abs(rhw-8.0113f)<0.060f));
}

/* Geometry-only *candidate* for the native game ESC purple ribbon.
 * Original and latest level-2 1920x1080 F9 screenshots both show
 * visible borders at approximately y=398..681 (not 342..585: the
 * earlier estimate came from a scaled image preview). Thus the prior
 * >=55% lower bound was NOT the cause of the missed match. Preserve
 * that conservative bound while investigating exact caller/Z/RHW,
 * untextured state and game stage through read-only diagnostics. */
static inline int SpiderNativePause43_RibbonBounds(
    float left,float right,float top,float bottom,unsigned w,unsigned h)
{
    if(!SpiderNativePause43_Widescreen(w,h))
        return 0;
    return left>=-(float)w*0.016f &&
           left<=(float)w*0.011f &&
           right>=(float)w*0.987f &&
           right<=(float)w*1.02f &&
           top>=(float)h*0.30f &&
           top<=(float)h*0.52f &&
           bottom>=(float)h*0.55f &&
           bottom<(float)h*0.72f &&
           bottom-top>(float)h*0.11f;
}
#endif
