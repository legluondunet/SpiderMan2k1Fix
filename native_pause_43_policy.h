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

/* Geometry-only *candidate* for the native game ESC purple ribbon.
 * Historic level-1 trace: y~400..675 at 1080 height.
 * New level-2 screenshot: actual visible border y~342..585.
 * The former bottom >= 55% screen-height requirement excluded this
 * valid, higher ribbon; 50% accommodates both. Exact original caller,
 * untextured state, FVF, 4-vertex fan and Z/RHW are STILL mandatory
 * in the render hook before any mapping is applied. */
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
           bottom>=(float)h*0.50f &&
           bottom<(float)h*0.72f &&
           bottom-top>(float)h*0.11f;
}
#endif
