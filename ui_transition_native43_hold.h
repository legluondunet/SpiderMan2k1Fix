#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_HOLD_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_HOLD_H
/*
 * V22 test: two solid BLACK physical widescreen gutters while the
 * original game still draws its native iris (V19). No artificial teeth,
 * no polygon-edge extrapolation, no threshold guessed from a clock.
 *
 * Both 16:9 strips are retained until the first D3D7 scene with ZERO
 * verified native iris draws. Hence they stay black at the initial
 * tiny opening and during all intermediate hooked-limb animation.
 * After the game's iris ends, the normal 16:9 world is fully exposed.
 *
 * This helper builds ONLY two rectangles OUTSIDE the centered 4:3
 * canvas. It never touches game-owned native mask vertices, never
 * modifies cinema bars, HUD or ESC, and never changes D3D7 states.
 * The caller must use the SAME trusted original iris DrawPrimitive
 * state (no texture or alpha blending) and supply 4 verified vertices.
 *
 * A future visual test is required to detect whether the boundary
 * between last visible iris frame and the first source-free scene has
 * any perceptible hard cut. No timer, fades or stale cross-frame
 * geometry are inferred here.
 */
#include "menu_scale.h"
#include <math.h>
#include <stdint.h>

static inline float SpiderNativeHold_Cross(
    const SpiderMenuVertex *q,unsigned a,unsigned b,unsigned c)
{
    return (q[b].x-q[a].x)*(q[c].y-q[a].y)-
           (q[b].y-q[a].y)*(q[c].x-q[a].x);
}

static inline int SpiderNativeHold_Build(
    const SpiderMenuVertex *source,unsigned width,unsigned height,
    SpiderMenuVertex gutters[2][4])
{
    unsigned i;
    float half,cut,area;
    if(!source || !gutters || width<640u || height<480u ||
       width>16384u || height>16384u ||
       (uint64_t)width*3u<=(uint64_t)height*4u)
        return 0;
    half=(float)width*.5f;
    cut=half-(float)height*2.0f/3.0f;
    if(!(cut>0.0f && cut<half))return 0;
    for(i=0u;i<4u;i++){
        if(!isfinite(source[i].x) || !isfinite(source[i].y) ||
           source[i].diffuse!=0x00000000u ||
           !isfinite(source[i].z) || !isfinite(source[i].rhw))
            return 0;
    }
    area=SpiderNativeHold_Cross(source,0,1,2);
    if(fabsf(area)<.00001f)
        area=SpiderNativeHold_Cross(source,0,2,3);
    /* Wait for the first nondegenerate verified source fan in this
     * scene; its winding matches the native opaque black material. */
    if(fabsf(area)<.00001f)return 0;
    for(i=0u;i<2u;i++){
        float l=i==0u?0.0f:(float)width-cut;
        float r=i==0u?cut:(float)width;
        unsigned j;
        for(j=0u;j<4u;j++)gutters[i][j]=source[0];
        gutters[i][0].x=l;gutters[i][0].y=0.0f;
        if(area>0.0f){
            gutters[i][1].x=r;gutters[i][1].y=0.0f;
            gutters[i][2].x=r;gutters[i][2].y=(float)height;
            gutters[i][3].x=l;gutters[i][3].y=(float)height;
        }else{
            gutters[i][1].x=l;gutters[i][1].y=(float)height;
            gutters[i][2].x=r;gutters[i][2].y=(float)height;
            gutters[i][3].x=r;gutters[i][3].y=0.0f;
        }
    }
    return 1;
}
#endif
