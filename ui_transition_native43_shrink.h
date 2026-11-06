#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_SHRINK_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_SHRINK_H
/*
 * V23 experimental improvement over V22:
 *
 * Do NOT touch the V19 game-owned spider / original D3D7 DrawPrimitive.
 * The previously FULL, 240px-wide physical gutters should shrink
 * smoothly as the game's actual iris grows, instead of vanishing
 * abruptly when the final iris DrawPrimitive disappears.
 *
 * The first game-owned central lower-right (or lower-left) iris fan is
 * a stable size witness: at the recorded, genuine original native mask
 * frame, the horizontal width of that fan is 22.5 source pixels, while
 * the native spider aperture extends 213.75px from center. Their ratio
 * is 9.5. The code measures that fan in EACH frame and estimates the
 * physical spider half-width with the very same V19 4:3 X scale:
 *
 *  estimated_opening_half_x = fan_source_width * 9.5 * 4H/(3W).
 *
 * The estimate is clamped monotonically for this one animation,
 * so changes in offscreen native culling cannot make black strips
 * grow back. Preserve flat black, original material and winding.
 *
 * Begin shrinking only when the native opening approaches the 4:3
 * edge (72% of the centered 4:3 half width); finish just before
 * the aperture fills the physical display (96% of W/2).
 * Smoothstep avoids a visible abrupt change of the band width.
 * A source-free scene releases any remaining bands immediately.
 *
 * The 9.5 calibration comes from the captured REAL source geometry,
 * not a guessed elapsed time or scale animation. This is a VISUAL
 * approximation of the widescreen outside the 4:3 source iris, not
 * a mathematically exact mask fill. It is isolated by a separate
 * opt-in. No synthetic teeth, no fullscreen scale, no altered HUD.
 */
#include "ui_transition_native43_hold.h"
#include <math.h>

static inline int SpiderNativeShrink_AnchorHalfWidth(
    const SpiderMenuVertex *source,unsigned width,unsigned height,
    float *half_width)
{
    float minx,maxx,miny,maxy,span,hspan,centerx,centery,ratio;
    unsigned i,columns[2]={0,0},rows[2]={0,0};
    if(half_width)*half_width=0.0f;
    if(!source || !half_width || width<640u || height<480u)
        return 0;
    minx=maxx=source[0].x;
    miny=maxy=source[0].y;
    for(i=0u;i<4u;i++){
        if(!isfinite(source[i].x)||!isfinite(source[i].y))
            return 0;
        if(source[i].x<minx)minx=source[i].x;
        if(source[i].x>maxx)maxx=source[i].x;
        if(source[i].y<miny)miny=source[i].y;
        if(source[i].y>maxy)maxy=source[i].y;
    }
    span=maxx-minx;
    hspan=maxy-miny;
    centerx=(float)width*.5f;
    centery=(float)height*.5f;
    if(!(span>0.1f && hspan>0.1f &&
         hspan/span>.85f && hspan/span<1.55f &&
         miny>centery && maxy>miny))
        return 0;
    /* Only an actual center-attached inner iris fan is eligible,
     * never a giant outer-matte rectangle or another distant tooth. */
    if(!(fabsf(minx-centerx)<.51f || fabsf(maxx-centerx)<.51f))
        return 0;
    for(i=0u;i<4u;i++){
        int col=fabsf(source[i].x-minx)<.51f ? 0 :
                (fabsf(source[i].x-maxx)<.51f ? 1 : -1);
        int row=fabsf(source[i].y-miny)<.51f ? 0 :
                (fabsf(source[i].y-maxy)<.51f ? 1 : -1);
        if(col<0 || row<0)return 0;
        ++columns[col];
        ++rows[row];
    }
    if(columns[0]!=2u || columns[1]!=2u ||
       rows[0]<1u || rows[1]<1u)
        return 0;
    ratio=4.0f*(float)height/(3.0f*(float)width);
    *half_width=span*9.5f*ratio;
    return isfinite(*half_width) && *half_width>0.0f;
}

/* 0=hidden side curtains fully transparent, max=full native side
 * curtains. This is a pure, deterministic function of real source
 * geometry. Returns false only if output aspect is unsupported.
 */
static inline int SpiderNativeShrink_Width(
    unsigned width,unsigned height,float estimated_half_width,
    float *out_width)
{
    float max_gutter,start,end,p,t,fit_half;
    if(out_width)*out_width=0.0f;
    if(!out_width || width<640u || height<480u ||
       width>16384u || height>16384u ||
       !isfinite(estimated_half_width) ||
       estimated_half_width<0.0f ||
       (uint64_t)width*3u<=(uint64_t)height*4u)
        return 0;
    fit_half=(float)height*2.0f/3.0f;
    max_gutter=(float)width*.5f-fit_half;
    start=fit_half*.72f;
    end=(float)width*.48f;
    if(!(max_gutter>0.0f && end>start))
        return 0;
    p=(estimated_half_width-start)/(end-start);
    if(p<0.0f)p=0.0f;
    if(p>1.0f)p=1.0f;
    t=p*p*(3.0f-2.0f*p);
    *out_width=max_gutter*(1.0f-t);
    /* Avoid unstable <1px gutter draw at the final frame. */
    if(*out_width<.5f)*out_width=0.0f;
    return 1;
}

/* Reuses V22's verified black-material, native-winding rectangles;
 * only the X borders of the two physical side strips may move.
 * width=0 intentionally generates NO polygons; caller skips draw.
 */
static inline int SpiderNativeShrink_Build(
    const SpiderMenuVertex *source,unsigned width,unsigned height,
    float desired_width,SpiderMenuVertex gutters[2][4])
{
    float max_gutter;
    unsigned i,j;
    if(!source || !gutters || !isfinite(desired_width) ||
       desired_width<0.0f)return 0;
    max_gutter=(float)width*.5f-(float)height*2.0f/3.0f;
    if(!(max_gutter>0.0f && desired_width<=max_gutter+.5f))
        return 0;
    if(!SpiderNativeHold_Build(source,width,height,gutters))
        return 0;
    for(i=0u;i<2u;i++){
        for(j=0u;j<4u;j++){
            if(i==0u){
                if(gutters[i][j].x>0.0f)
                    gutters[i][j].x=desired_width;
            }else{
                if(gutters[i][j].x<(float)width)
                    gutters[i][j].x=(float)width-desired_width;
            }
        }
    }
    return 1;
}
#endif
