#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_GUTTERS_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_GUTTERS_H

/* Experimental physical-gutter compositor for native 2D scene masks.
 *
 * A wipe/iris is not a HUD. Its source material is a screen-space 2D
 * polygon: the game already supplies black masks for the physical frame.
 * The 4:3 user-interface transform is useful for the INNER opening, but
 * contracts its outer masks into the centered safe area, revealing the
 * world in two unwanted screen gutters.
 *
 * Instead of stretching the iris or duplicating the whole scene:
 *  - keep the centered 4:3 mapping for the original inner silhouette;
 *  - clip the ORIGINAL native mask polygons to the two *physical* gutters;
 *  - submit ONLY those clipped polygon fragments, in the native material
 *    state, during the same game-owned Direct3D7 render pass.
 *
 * Clipping is generic Sutherland-Hodgman and preserves each polygon's
 * sloping contours. There are no scene/level IDs and no pre-baked shapes.
 * A strict observed render-family check controls the initial trial;
 * this is not a proven general-purpose UI/3D ownership classifier.
 * Disabled by default until explicit Wine and Windows visual validation.
 */
#include "menu_scale.h"
#include <stdint.h>
#include <string.h>

#define SPIDER_TRANSITION_MAX_VERTS 8u
#define SPIDER_TRANSITION_CALLER_RVA 0x00102C72u

static inline float SpiderTransition_Abs(float x)
{ return x<0.0f ? -x : x; }

/* Family observed during native expanding spider transition in the
 * original game: opaque black, screen-projected, z≈0, RHW≈27.6.
 * This identifies the render pass, never the spider's geometry.
 */
static inline int SpiderTransition_IsNativeBlackMask(
    const SpiderMenuVertex *v,unsigned count,unsigned primitive,
    uint32_t fvf,uint32_t caller_rva,unsigned w,unsigned h)
{
    unsigned i;
    float minx,maxx,miny,maxy;
    if(!v || count!=4u || primitive!=6u || fvf!=0x144u ||
       caller_rva!=SPIDER_TRANSITION_CALLER_RVA ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u <= (uint64_t)h*4u)return 0;
    minx=maxx=v[0].x;miny=maxy=v[0].y;
    for(i=0u;i<4u;i++){
        if(v[i].diffuse!=0xFF000000u ||
           SpiderTransition_Abs(v[i].z)>0.00004f ||
           SpiderTransition_Abs(v[i].rhw-27.6f)>0.08f ||
           !(v[i].x>=-(float)w*.05f && v[i].x<=(float)w*1.05f &&
             v[i].y>=-(float)h*.05f && v[i].y<=(float)h*1.05f))
            return 0;
        if(v[i].x<minx)minx=v[i].x;
        if(v[i].x>maxx)maxx=v[i].x;
        if(v[i].y<miny)miny=v[i].y;
        if(v[i].y>maxy)maxy=v[i].y;
    }
    return maxx-minx>0.5f && maxy-miny>0.5f;
}

static inline SpiderMenuVertex SpiderTransition_Interpolate(
    const SpiderMenuVertex *a,const SpiderMenuVertex *b,float t)
{
    SpiderMenuVertex out=*a;
    out.x=a->x+t*(b->x-a->x);
    out.y=a->y+t*(b->y-a->y);
    out.z=a->z+t*(b->z-a->z);
    out.rhw=a->rhw+t*(b->rhw-a->rhw);
    out.u=a->u+t*(b->u-a->u);
    out.v=a->v+t*(b->v-a->v);
    /* The gated material has the same opaque-black diffuse on every
     * vertex. There is no colour-gradient interpolation to approximate. */
    return out;
}

/* One vertical half-plane, optionally retaining x >= edge. */
static inline unsigned SpiderTransition_ClipHalfPlane(
    const SpiderMenuVertex *input,unsigned n,
    SpiderMenuVertex *output,float edge,int greater_equal)
{
    unsigned i,k=0u;
    if(!input || !output || n<3u || n>SPIDER_TRANSITION_MAX_VERTS)
        return 0u;
    for(i=0u;i<n;i++){
        const SpiderMenuVertex *a=&input[(i+n-1u)%n],*b=&input[i];
        int ain=greater_equal ? a->x>=edge : a->x<=edge;
        int bin=greater_equal ? b->x>=edge : b->x<=edge;
        if(ain!=bin){
            float dx=b->x-a->x;
            float t;
            if(dx==0.0f || k>=SPIDER_TRANSITION_MAX_VERTS)return 0u;
            t=(edge-a->x)/dx;
            if(!(t>=0.0f && t<=1.0f))return 0u;
            output[k++]=SpiderTransition_Interpolate(a,b,t);
            output[k-1u].x=edge;
        }
        if(bin){
            if(k>=SPIDER_TRANSITION_MAX_VERTS)return 0u;
            output[k++]=*b;
        }
    }
    return k;
}

/* Clip the original quad's surface to an actual exterior strip.
 * side=0 -> [0,gutter], side=1 -> [screen_w-gutter,screen_w].
 * Callers submit the returned XYZRHW fan unaltered using native D3D7
 * state. The central canvas is deliberately excluded, so the spider's
 * already-correct 4:3 shape is never duplicated or horizontally scaled.
 */
static inline unsigned SpiderTransition_ClipToGutter(
    const SpiderMenuVertex *source,SpiderMenuVertex *out,
    unsigned side,unsigned screen_w,unsigned screen_h)
{
    SpiderMenuVertex first[SPIDER_TRANSITION_MAX_VERTS];
    float w=(float)screen_w,h=(float)screen_h,gutter,left,right;
    unsigned n;
    if(!source || !out || side>1u || screen_w<640u ||
       screen_h<480u || screen_w>16384u || screen_h>16384u ||
       (uint64_t)screen_w*3u <= (uint64_t)screen_h*4u)
        return 0u;
    gutter=(w-h*4.0f/3.0f)*.5f;
    if(!(gutter>0.0f && gutter<w*.5f))return 0u;
    left=side ? w-gutter : 0.0f;
    right=side ? w : gutter;
    n=SpiderTransition_ClipHalfPlane(source,4u,first,left,1);
    if(n<3u)return 0u;
    n=SpiderTransition_ClipHalfPlane(first,n,out,right,0);
    return n>=3u ? n : 0u;
}

/* The previously approved 4:3 gameplay UI mapping is unmodified.
 * During the new explicit transition-only test, cancel only the old
 * physical-edge pinning of verified mask polygons. The actual shape
 * still receives the same centered 4:3 X mapping as before. */
static inline void SpiderTransition_MapInner43(
    const SpiderMenuVertex *src,SpiderMenuVertex *mapped,
    unsigned screen_w,unsigned screen_h)
{
    unsigned i;
    float center=(float)screen_w*.5f;
    float scale=(float)screen_h*4.0f/(3.0f*(float)screen_w);
    /* Complete out-of-place copy: preserve Y, Z, RHW, diffuse and UV
     * even when the output buffer was not initialized by a prior pass. */
    memcpy(mapped,src,4u*sizeof(*mapped));
    for(i=0u;i<4u;i++)
        mapped[i].x=center+(src[i].x-center)*scale;
}

#endif
