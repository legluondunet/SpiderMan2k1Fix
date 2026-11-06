#ifndef SPIDERMAN_UI_TRANSITION_ATOMIC_VECTOR_H
#define SPIDERMAN_UI_TRANSITION_ATOMIC_VECTOR_H
/* V12 experiment: render the source iris polygons at the final display
 * resolution, without first rasterizing a magnified bitmap. All quads share
 * ONE 4:3 X correction and ONE isotropic game-owned animation scale.
 *
 * Optional outer-matte rectangles represent the opaque exterior of the
 * reference image (equivalent to sampling outside the V11 texture with
 * CLAMP); they use the SAME transformed canvas boundary. They are not
 * independently animated tooth extensions. All triangles form one draw.
 * Native opaque cinematic bars are NOT part of this source family.
 */
#include "ui_transition_atomic_mask.h"
#include <math.h>

#define SPIDER_ATOMIC_VECTOR_MAX ((SPIDER_ATOMIC_MASK_MAX + 4u) * 6u)
typedef struct SpiderAtomicVectorPoint { float x,y; } SpiderAtomicVectorPoint;

static inline int SpiderAtomicVector_AppendTriangle(
    SpiderAtomicVectorPoint *out,unsigned capacity,unsigned *used,
    SpiderAtomicVectorPoint a,SpiderAtomicVectorPoint b,
    SpiderAtomicVectorPoint c)
{
    float cross=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    if(cross<.000001f && cross>-.000001f)return 1;
    if(*used>capacity || capacity-*used<3u)return 0;
    out[(*used)++]=a;out[(*used)++]=b;out[(*used)++]=c;
    return 1;
}
static inline int SpiderAtomicVector_AppendRectangle(
    SpiderAtomicVectorPoint *out,unsigned capacity,unsigned *used,
    float l,float t,float r,float b)
{
    SpiderAtomicVectorPoint a,c,d,e;
    if(!(r>l && b>t))return 1;
    a.x=l;a.y=t;c.x=r;c.y=t;d.x=r;d.y=b;e.x=l;e.y=b;
    return SpiderAtomicVector_AppendTriangle(out,capacity,used,a,c,d) &&
           SpiderAtomicVector_AppendTriangle(out,capacity,used,a,d,e);
}
/* A single affine mapping of every original native vertex:
 *   x' = W/2 + (x-W/2)*(4H/3W)*scale
 *   y' = H/2 + (y-H/2)*scale
 * This matches V11's texture re-aspect + uniform UV zoom but removes
 * intermediate pixel quantization. Includes the transformed exterior.
 * Caller draws the triangle list as untextured opaque black, CULL_NONE,
 * with alpha blending and depth testing disabled.
 */
static inline int SpiderAtomicVector_Build(
    const SpiderAtomicMaskQuad *reference,unsigned count,
    unsigned display_w,unsigned display_h,float scale,
    SpiderAtomicVectorPoint *out,unsigned capacity,unsigned *out_count)
{
    float cx,cy,aspect,l,r,t,b;
    unsigned i,k,n=0;
    if(out_count)*out_count=0u;
    if(!reference || !out || !out_count || count<1u ||
       count>SPIDER_ATOMIC_MASK_MAX ||
       display_w<640u || display_h<480u ||
       (uint64_t)display_w*3u<(uint64_t)display_h*4u ||
       display_w>16384u || display_h>16384u ||
       !(scale>=.015f && scale<=24.0f) ||
       capacity<SPIDER_ATOMIC_VECTOR_MAX)
        return 0;
    cx=(float)display_w*.5f;cy=(float)display_h*.5f;
    aspect=((float)display_h*4.0f)/((float)display_w*3.0f);
    if(!(aspect>0.0f && aspect<=1.000001f))return 0;
    l=cx-cx*aspect*scale;r=cx+cx*aspect*scale;
    t=cy-cy*scale;b=cy+cy*scale;
    /* Opaque continuation of the source image beyond its finite rectangle;
     * clip to physical viewport, but never extend individual teeth. */
    if(l>0.0f && !SpiderAtomicVector_AppendRectangle(
         out,capacity,&n,0.0f,0.0f,fminf(l,(float)display_w),(float)display_h))return 0;
    if(r<(float)display_w && !SpiderAtomicVector_AppendRectangle(
         out,capacity,&n,fmaxf(r,0.0f),0.0f,(float)display_w,(float)display_h))return 0;
    if(t>0.0f && !SpiderAtomicVector_AppendRectangle(
         out,capacity,&n,fmaxf(l,0.0f),0.0f,fminf(r,(float)display_w),fminf(t,(float)display_h)))return 0;
    if(b<(float)display_h && !SpiderAtomicVector_AppendRectangle(
         out,capacity,&n,fmaxf(l,0.0f),fmaxf(b,0.0f),fminf(r,(float)display_w),(float)display_h))return 0;
    for(i=0u;i<count;i++){
        SpiderAtomicVectorPoint v[4];
        for(k=0u;k<4u;k++){
            float x=reference[i].x[k],y=reference[i].y[k];
            if(!(x>=-(float)display_w*.10f &&
                 x<=(float)display_w*1.10f &&
                 y>=-(float)display_h*.10f &&
                 y<=(float)display_h*1.10f))return 0;
            v[k].x=cx+(x-cx)*aspect*scale;
            v[k].y=cy+(y-cy)*scale;
        }
        if(!SpiderAtomicVector_AppendTriangle(out,capacity,&n,v[0],v[1],v[2]) ||
           !SpiderAtomicVector_AppendTriangle(out,capacity,&n,v[0],v[2],v[3]))return 0;
    }
    if(n==0u)return 0;
    *out_count=n;
    return 1;
}
#endif
