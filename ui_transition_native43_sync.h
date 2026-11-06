#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_SYNC_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_SYNC_H
/*
 * V24 experimental: synchronize the black physical side curtains with
 * the CURRENT complete set of original game-owned iris fans.
 *
 * V23c draws the side curtains on the first usable black fan and then
 * observes the opening radius in the remaining fans. The end-of-scene
 * log therefore shows current radius but PRIOR-FRAME curtain width.
 * This is a real one-native-frame mismatch, not necessarily a bad
 * smoothstep timing threshold.
 *
 * A frame first OBSERVES every strict verified native V19 source fan,
 * without altering its original DrawPrimitive. Its first nondegenerate
 * black fan is retained as a material/vertex prototype, and the largest
 * central-fan opening radius observed in THAT SAME FRAME is measured.
 * Only after the entire family is observed does Finish calculate the
 * two side-curtain rectangles using V23c's unchanged smoothstep.
 *
 * In the game, Finish happens inside original D3D7 EndScene. The caller
 * must save/restore D3D7 device state and render only those two physical
 * rectangles; this header never issues D3D calls. No geometry/time
 * extrapolation, latched previous-frame progress, or synthetic teeth.
 */
#include "ui_transition_native43_shrink.h"
#include <string.h>

typedef struct SpiderNativeSync24Frame {
    SpiderMenuVertex prototype[4];
    float radius;
    unsigned native_fans,anchors,ready;
} SpiderNativeSync24Frame;

static inline void SpiderNativeSync24_Reset(SpiderNativeSync24Frame *frame)
{
    if(frame)memset(frame,0,sizeof(*frame));
}

static inline void SpiderNativeSync24_Observe(
    SpiderNativeSync24Frame *frame,const SpiderMenuVertex *source,
    unsigned width,unsigned height)
{
    float r=0.0f;
    SpiderMenuVertex sides[2][4];
    if(!frame || !source)return;
    /* The caller has *already* authenticated original source caller,
     * material, D3D7 state, scene and native DrawPrimitive success. */
    ++frame->native_fans;
    if(SpiderNativeShrink_AnchorHalfWidth(
         source,width,height,&r)){
        ++frame->anchors;
        if(r>frame->radius)frame->radius=r;
    }
    if(!frame->ready &&
       SpiderNativeHold_Build(source,width,height,sides)){
        memcpy(frame->prototype,source,sizeof(frame->prototype));
        frame->ready=1u;
    }
}

/* 1=valid two physical black rectangles, 2=fully revealed (no draw),
 * 0=no verified material/iris source this scene, -1=invalid geometry.
 * Output width is the CURRENT scene's exact V23c radius-based band.
 * The caller must not draw when status is 0, 2 or -1.
 */
static inline int SpiderNativeSync24_Finish(
    const SpiderNativeSync24Frame *frame,unsigned width,unsigned height,
    SpiderMenuVertex sides[2][4],float *band_width)
{
    float band=0.0f;
    if(band_width)*band_width=0.0f;
    if(!frame || !sides || !band_width)return -1;
    if(!frame->native_fans || !frame->ready)return 0;
    if(!SpiderNativeShrink_Width(width,height,frame->radius,&band))
        return -1;
    *band_width=band;
    if(band<.5f)return 2;
    return SpiderNativeShrink_Build(
        frame->prototype,width,height,band,sides)?1:-1;
}
#endif
