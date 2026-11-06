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

/* Five independently observed, tightly isolated native ESC depth layers.
 * The original game varies RHW with game context; the same original
 * caller, geometry, texture state and depth still must match in the hook.
 *  - earlier 1920x1080 validated ESC: ribbon 11.176, motif 9.988
 *  - 2026-11-04 GE-Proton second scene: ribbon 8.9615, motif 7.8952
 *  - 2026-11-05 Rhino boss: ribbon 6.8041, motif 5.9130
 *  - 2026-11-05 later Mysterio stage (scrn0048 + Log=1):
 *    ribbon 11.7760, motif candidate 10.5646. The latter has the
 *    original caller, alpha-80 diffuse and untextured motif geometry;
 *    its HUD-VISUAL label "AIM" is only a generic candidate label.
 *  - 2026-11-05 Spider-Man vs Venom (2), Windows 11 native 16:10:
 *    ribbon .00122/10.6651 and untextured alpha-80 motif candidate
 *    .00146/9.4993, same original D3D7 caller and source geometry.
 *    Follow-up source log + F9 scrn0004 (2026-11-05 14:17):
 *    8x8 ESC border and black glyph shadow at .00122/10.6651,
 *    red/white/purple letter face at .00098/12.1569 (all corrected=0).
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
            SpiderNativePause43_Abs(rhw-6.8041f)<0.060f ||
            SpiderNativePause43_Abs(rhw-11.7760f)<0.060f ||
            SpiderNativePause43_Abs(rhw-10.6651f)<0.060f);
}
static inline int SpiderNativePause43_MotifLayer(float z,float rhw)
{
    return SpiderNativePause43_Abs(z-0.00146f)<0.00004f &&
           (SpiderNativePause43_Abs(rhw-9.988f)<0.060f ||
            SpiderNativePause43_Abs(rhw-7.8952f)<0.060f ||
            SpiderNativePause43_Abs(rhw-5.9130f)<0.060f ||
            SpiderNativePause43_Abs(rhw-10.5646f)<0.060f ||
            SpiderNativePause43_Abs(rhw-9.4993f)<0.060f);
}

/* Shadow and face of native Pause letters have five independently
 * measured source-depth pairs:
 *  - original: shadow z=.00122/rhw=11.176, face z=.00098/rhw=12.686
 *  - second scene: shadow 8.9615, face 10.3609
 *  - Rhino boss: shadow 6.8041, face 8.0113.
 *  - Later Mysterio encounter: shadow z=.00122/rhw=11.7760,
 *    face z=.00098/rhw=13.3012 (2026-11-05 scrn0049.png).
 *  - Venom (2) Windows 11 F9 scrn0004.png: shadow .00122/10.6651,
 *    face .00098/12.1569 and identical-depth 8x8 edge border.
 *    All were observed by PAUSE-NATIVE-DETAIL family=2 and
 *    PAUSE-NATIVE-BORDER-PROBE with corrected=0.
 * The Mysterio depth pair is now directly confirmed by the second
 * source-RHW-family read-only probe: 35 shadow quads (FF000000,
 * Z=.00122/RHW=11.7760) and 18 red-title face quads (FFFE3242,
 * Z=.00098/RHW=13.3012) were uncorrected in the latest screenshot.
 * Identical 8x8 original ESC frame tiles share the shadow depth,
 * but still require exact same-Flip native ribbon and border bounds.
 * Rhino scrn0045.png / Log=1 (2026-11-05 01:59:35) confirms 48
 * corrected black-shadow samples and 32 missed colored-face samples
 * (FFFE3242 title, FF8A78D6 options) at z=.00098/rhw=8.0113.
 * Venom (2) depths are now source-verified; before this follow-up,
 * recognizing its untextured ribbon did NOT authorize glyph layers.
 * Never infer arbitrary RHW from other objects or use an interval
 * between discrete source variants: the original game caller,
 * same-Flip verified ribbon, 16/32/64x32 texture and tight glyph
 * geometry/position gates must all agree before mapping. */
static inline int SpiderNativePause43_TextLayer(float z,float rhw)
{
    return (SpiderNativePause43_Abs(z-0.00122f)<0.00010f &&
            (SpiderNativePause43_Abs(rhw-11.176f)<0.30f ||
             SpiderNativePause43_Abs(rhw-8.9615f)<0.060f ||
             SpiderNativePause43_Abs(rhw-6.8041f)<0.060f ||
             SpiderNativePause43_Abs(rhw-11.7760f)<0.060f ||
             SpiderNativePause43_Abs(rhw-10.6651f)<0.060f)) ||
           (SpiderNativePause43_Abs(z-0.00098f)<0.00010f &&
            (SpiderNativePause43_Abs(rhw-12.686f)<0.30f ||
             SpiderNativePause43_Abs(rhw-10.3609f)<0.060f ||
             SpiderNativePause43_Abs(rhw-8.0113f)<0.060f ||
             SpiderNativePause43_Abs(rhw-13.3012f)<0.060f ||
             SpiderNativePause43_Abs(rhw-12.1569f)<0.060f));
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
/* Compositor-only classification: original ESC perspective varies with
 * camera and level. Verify caller/geometry/texture and current ESC menu
 * context in the host before applying these depth and tint predicates. */
static inline int SpiderNativePause43_Perspective(float z,float rhw,float target)
{
    return SpiderNativePause43_Abs(z-target)<0.000045f &&
           rhw>0.5f && rhw<=128.0f;
}
static inline int SpiderNativePause43_CompositorRibbon(float z,float rhw,unsigned argb)
{
    unsigned alpha=(argb>>24)&255u;
    unsigned red=(argb>>16)&255u;
    unsigned green=(argb>>8)&255u;
    unsigned blue=argb&255u;
    return SpiderNativePause43_Perspective(z,rhw,0.00122f) &&
           alpha>=32u && alpha<=208u &&
           blue>red+20u && blue>green+20u;
}
static inline int SpiderNativePause43_CompositorMotif(float z,float rhw,unsigned argb)
{
    return SpiderNativePause43_Perspective(z,rhw,0.00146f) &&
           ((argb>>24)&255u)==128u;
}
static inline int SpiderNativePause43_CompositorText(float z,float rhw)
{
    return SpiderNativePause43_Perspective(z,rhw,0.00122f) ||
           SpiderNativePause43_Perspective(z,rhw,0.00098f);
}

#endif
