#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_BOUNDS25_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_BOUNDS25_H
/*
 * V25 native-geometry-driven physical 4:3 side curtains (opt-in).
 *
 * V24 drew two full-height black D3D7 rectangles with width controlled
 * by a radius-based ease curve, even when an actual spider tooth had
 * already entered those margins. It clipped hooked tips at a straight
 * vertical edge. This extension uses the current frame's REAL original
 * iris triangles, *after* V19's X correction, to find the furthest
 * onscreen tooth on each side.
 *
 * Core invariants:
 *  - do NOT remap, replace, suppress, or replay native V19 triangles;
 *  - ignore the four huge flat outer-side/top/bottom matte rectangles
 *    (their early X extrema are not the actual spider silhouette);
 *  - clip native tooth triangles to the physical vertical display
 *    range before measuring X (offscreen tips do not force early clear);
 *  - each side-curtain rectangle ends NO FURTHER than the most extreme
 *    visible source tooth (plus a one-PHYSICAL-pixel anti-seam overlap);
 *  - no 1920x1080-specific constants; 4:3 has zero side rectangles;
 *  - widths are clamped nonincreasing across frames of one transition,
 *    so culling of an original fan cannot make black margins regrow.
 *
 * This is a conservative frame-wide silhouette bound. A full nonlinear
 * per-Y contour would require independent source-hole reconstruction;
 * this test deliberately does not reintroduce that failure-prone
 * renderer. The visual quality must still be verified on actual D3D7.
 */
#include "ui_transition_native43_sync.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define SPIDER_NATIVE_BOUNDS25_OVERLAP_PX 1.0f

typedef struct SpiderNativeBounds25Frame {
    float min_x,max_x;
    unsigned eligible,excluded_matte,offscreen,observed;
} SpiderNativeBounds25Frame;

static inline void SpiderNativeBounds25_Reset(SpiderNativeBounds25Frame *f)
{
    if(!f)return;
    memset(f,0,sizeof(*f));
    f->min_x=1.0e30f;
    f->max_x=-1.0e30f;
}

/* The genuine native iris consists of both the hooked polygon mesh and
 * 4 large axis-aligned opaque mattes at its outside. Remove ONLY the
 * large rectangular mattes by GEOMETRY, never by call order.
 * A small genuine tooth rectangle is retained.
 */
static inline int SpiderNativeBounds25_IsOuterMatte(
    const SpiderMenuVertex *p,unsigned width,unsigned height)
{
    float minx,maxx,miny,maxy,dx,dy;
    unsigned i,corners[2][2]={{0u,0u},{0u,0u}};
    if(!p)return 0;
    minx=maxx=p[0].x;
    miny=maxy=p[0].y;
    for(i=1u;i<4u;i++){
        if(p[i].x<minx)minx=p[i].x;
        if(p[i].x>maxx)maxx=p[i].x;
        if(p[i].y<miny)miny=p[i].y;
        if(p[i].y>maxy)maxy=p[i].y;
    }
    dx=maxx-minx;dy=maxy-miny;
    /* Big matte, not a diagonal tooth. Width/height tests are ratios
     * of the physical output size and work at all widescreen formats. */
    /* Compare X against the physical 4:3 canvas width, not the entire
     * ultrawide framebuffer: the same genuine outer matte must still
     * be rejected on 21:9 and 32:9 displays. */
    if(!(dx>=((float)height*4.0f/3.0f)*.15f &&
         dy>=(float)height*.35f))
        return 0;
    for(i=0u;i<4u;i++){
        int col=fabsf(p[i].x-minx)<.02f?0:
                (fabsf(p[i].x-maxx)<.02f?1:-1);
        int row=fabsf(p[i].y-miny)<.02f?0:
                (fabsf(p[i].y-maxy)<.02f?1:-1);
        if(col<0||row<0)return 0;
        if(++corners[col][row]>1u)return 0;
    }
    return 1;
}

/* Clip a genuine iris triangle to the physical screen Y=[0,height].
 * Sutherland–Hodgman for two axis-aligned half-planes. Only X bounds
 * are needed, and no input/source memory is ever modified. */
typedef struct SpiderNativeBounds25Point { float x,y; }
    SpiderNativeBounds25Point;

static inline unsigned SpiderNativeBounds25_ClipY(
    const SpiderNativeBounds25Point *src,unsigned n,
    SpiderNativeBounds25Point *dst,float edge,int keep_above)
{
    unsigned i,out=0u;
    if(!src || !dst || !n)return 0u;
    for(i=0u;i<n;i++){
        SpiderNativeBounds25Point a=src[i],b=src[(i+1u)%n];
        int ai=keep_above?(a.y>=edge):(a.y<=edge);
        int bi=keep_above?(b.y>=edge):(b.y<=edge);
        if(ai!=bi && b.y!=a.y){
            float t=(edge-a.y)/(b.y-a.y);
            if(out<8u){
                dst[out].x=a.x+t*(b.x-a.x);
                dst[out].y=edge;
                ++out;
            }
        }
        if(bi && out<8u)dst[out++]=b;
    }
    return out;
}

static inline void SpiderNativeBounds25_Observe(
    SpiderNativeBounds25Frame *f,const SpiderMenuVertex *mapped,
    unsigned width,unsigned height)
{
    unsigned tri;
    if(!f || !mapped || width<640u || height<480u ||
       width>16384u || height>16384u)return;
    ++f->observed;
    for(tri=0u;tri<4u;tri++)
        if(!isfinite(mapped[tri].x) || !isfinite(mapped[tri].y))
            return;  /* never trust NaN/Inf */
    if(SpiderNativeBounds25_IsOuterMatte(mapped,width,height)){
        ++f->excluded_matte;
        return;
    }
    for(tri=0u;tri<2u;tri++){
        unsigned i,n;
        SpiderNativeBounds25Point original[3],temp[8],clipped[8];
        unsigned idx[3]={0u,tri?2u:1u,tri?3u:2u};
        float area;
        for(i=0u;i<3u;i++){
            original[i].x=mapped[idx[i]].x;
            original[i].y=mapped[idx[i]].y;
        }
        area=(original[1].x-original[0].x)*
             (original[2].y-original[0].y)-
             (original[1].y-original[0].y)*
             (original[2].x-original[0].x);
        if(fabsf(area)<.00001f)continue;
        n=SpiderNativeBounds25_ClipY(original,3u,temp,0.0f,1);
        n=SpiderNativeBounds25_ClipY(temp,n,clipped,
                                    (float)height,0);
        if(n<3u)continue;
        ++f->eligible;
        for(i=0u;i<n;i++){
            if(clipped[i].x<f->min_x)f->min_x=clipped[i].x;
            if(clipped[i].x>f->max_x)f->max_x=clipped[i].x;
        }
    }
    if(f->eligible==0u)++f->offscreen;
}

/* Expected 4:3 physical content width is 4H/3. Each curtain is confined
 * to its physical SIDE margin, except 1px overlap that kills the
 * D3D7 raster seam at the 4:3 canvas boundary.
 * After a tooth physically reaches x=0 / x=W, width is ZERO.
 *
 * result: 1 => valid geometry (width may be zero),
 *         0 => malformed input or unsupported/narrow aspect.
 * For missing tooth observations use V24 as an explicit fallback;
 * never invent bounds or fill the actual spider body.
 */
static inline int SpiderNativeBounds25_Widths(
    const SpiderNativeBounds25Frame *f,unsigned width,unsigned height,
    float *left,float *right)
{
    float gutter,overlap=SPIDER_NATIVE_BOUNDS25_OVERLAP_PX;
    if(left)*left=0.0f;
    if(right)*right=0.0f;
    if(!f || !left || !right ||
       width<640u || height<480u ||
       width>16384u || height>16384u ||
       (uint64_t)width*3u<=(uint64_t)height*4u ||
       f->eligible==0u ||
       !isfinite(f->min_x) || !isfinite(f->max_x))
        return 0;
    gutter=((float)width-(float)height*4.0f/3.0f)*.5f;
    if(!(gutter>0.0f && gutter<(float)width*.5f))return 0;
    *left=fminf(gutter+overlap,fmaxf(0.0f,f->min_x+overlap));
    *right=fminf(gutter+overlap,
                 fmaxf(0.0f,(float)width-f->max_x+overlap));
    if(*left<.5f)*left=0.0f;
    if(*right<.5f)*right=0.0f;
    return 1;
}

/* Prepare two possibly ASYMMETRIC original-material 16:9 side fans.
 * These are never drawn if their width is zero.
 */
static inline int SpiderNativeBounds25_Build(
    const SpiderMenuVertex *original,unsigned width,unsigned height,
    float left,float right,SpiderMenuVertex out[2][4])
{
    float gutter;
    unsigned i,j;
    if(!original || !out ||
       !isfinite(left) || !isfinite(right) ||
       left<0.0f || right<0.0f ||
       width<640u || height<480u ||
       width>16384u || height>16384u ||
       (uint64_t)width*3u<=(uint64_t)height*4u)return 0;
    gutter=((float)width-(float)height*4.0f/3.0f)*.5f;
    if(!(gutter>0.0f &&
         left<=gutter+SPIDER_NATIVE_BOUNDS25_OVERLAP_PX+.01f &&
         right<=gutter+SPIDER_NATIVE_BOUNDS25_OVERLAP_PX+.01f))
        return 0;
    if(!SpiderNativeHold_Build(original,width,height,out))
        return 0;
    for(i=0u;i<2u;i++){
        for(j=0u;j<4u;j++){
            if(i==0u){
                if(out[i][j].x>0.0f)out[i][j].x=left;
            }else{
                if(out[i][j].x<(float)width)
                    out[i][j].x=(float)width-right;
            }
        }
    }
    return 1;
}
#endif
