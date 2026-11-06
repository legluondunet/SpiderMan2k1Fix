#ifndef SPIDERMAN_UI_TRANSITION_ATOMIC_CONTOUR_H
#define SPIDERMAN_UI_TRANSITION_ATOMIC_CONTOUR_H
/* V13 experiment: identify the CENTER-CONNECTED iris opening from the
 * native untextured, black 2D polygons. Every direction must meet black;
 * the opening must be clear at its center. Rendering its ONE radial
 * boundary (black everywhere outside) eliminates detached transparent
 * islands and exposed widescreen side gutters.
 *
 * Strict trial, not a replacement for the original game data. The native
 * source is immutable, and the original game's scale drives EVERY frame.
 * Detect unsupported/non-star-shaped sources and retain V12 when invalid.
 */
#include "ui_transition_atomic_mask.h"
#include "ui_transition_atomic_vector.h"
#include <math.h>
#define SPIDER_ATOMIC_CONTOUR_SAMPLES 2048u
#define SPIDER_ATOMIC_CONTOUR_VERTICES (SPIDER_ATOMIC_CONTOUR_SAMPLES*6u)
#define SPIDER_ATOMIC_CONTOUR_PI 3.14159265358979323846f

typedef struct SpiderAtomicContour {
    float radius[SPIDER_ATOMIC_CONTOUR_SAMPLES];
    float minimum,maximum;
} SpiderAtomicContour;

static inline float SpiderAtomicContour_Cross(float ax,float ay,float bx,float by)
{
    return ax*by-ay*bx;
}
/* Source points are already aspect-corrected about the physical center.
 * A ray from the center meets the closest opaque polygon at t>0.
 * The min intersection is the boundary of the CENTER-CONNECTED opening,
 * even if separate transparent islands exist farther away.
 */
static inline int SpiderAtomicContour_RaySegment(float dx,float dy,
    float ax,float ay,float bx,float by,float *out)
{
    float ex=bx-ax,ey=by-ay;
    float denominator=SpiderAtomicContour_Cross(dx,dy,ex,ey);
    float distance,portion;
    if(fabsf(denominator)<.000001f)return 0;
    distance=SpiderAtomicContour_Cross(ax,ay,ex,ey)/denominator;
    portion=SpiderAtomicContour_Cross(ax,ay,dx,dy)/denominator;
    if(!(distance>0.0001f && portion>=-.000001f && portion<=1.000001f))
        return 0;
    *out=distance;
    return 1;
}
static inline int SpiderAtomicContour_ContainsCenter(
    float ax,float ay,float bx,float by,float cx,float cy)
{
    float a=SpiderAtomicContour_Cross(bx-ax,by-ay,-ax,-ay);
    float b=SpiderAtomicContour_Cross(cx-bx,cy-by,-bx,-by);
    float c=SpiderAtomicContour_Cross(ax-cx,ay-cy,-cx,-cy);
    return (a>=-.0001f && b>=-.0001f && c>=-.0001f) ||
           (a<=.0001f && b<=.0001f && c<=.0001f);
}
/* Fail closed when source isn't a central open star-shaped aperture.
 * Quads are original game-owned sources; no texture pixels, no timer,
 * no per-boss or per-level artwork. */
static inline int SpiderAtomicContour_Prepare(
    const SpiderAtomicMaskQuad *quads,unsigned count,
    unsigned w,unsigned h,SpiderAtomicContour *contour)
{
    unsigned i,j,k,tri;
    float cx,cy,aspect,minr=1.0e30f,maxr=0.0f;
    if(!quads || !contour || !count || count>SPIDER_ATOMIC_MASK_MAX ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u<(uint64_t)h*4u)return 0;
    cx=(float)w*.5f;cy=(float)h*.5f;
    aspect=((float)h*4.0f)/((float)w*3.0f);
    /* Never overwrite a live contour until the complete original frame
     * passes all rays. A failed probe will retain the V12 draw path. */
    for(j=0u;j<count;j++)for(tri=0u;tri<2u;tri++){
        unsigned ids[3]={0u,tri==0u?1u:2u,tri==0u?2u:3u};
        float x[3],y[3];
        for(k=0u;k<3u;k++){
            x[k]=(quads[j].x[ids[k]]-cx)*aspect;
            y[k]=quads[j].y[ids[k]]-cy;
        }
        /* The native renderer can submit degenerate fan triangles.
         * They cover no pixels and must not masquerade as center fill. */
        if(fabsf(SpiderAtomicContour_Cross(
             x[1]-x[0],y[1]-y[0],x[2]-x[0],y[2]-y[0]))<.00001f)
            continue;
        if(SpiderAtomicContour_ContainsCenter(x[0],y[0],x[1],y[1],x[2],y[2]))
            return 0;
    }
    for(i=0u;i<SPIDER_ATOMIC_CONTOUR_SAMPLES;i++){
        float angle=(2.0f*SPIDER_ATOMIC_CONTOUR_PI*(float)i)/
                    (float)SPIDER_ATOMIC_CONTOUR_SAMPLES;
        float dx=cosf(angle),dy=sinf(angle),nearest=1.0e30f;
        for(j=0u;j<count;j++)for(tri=0u;tri<2u;tri++){
            unsigned ids[3]={0u,tri==0u?1u:2u,tri==0u?2u:3u};
            float x[3],y[3],t;
            for(k=0u;k<3u;k++){
                x[k]=(quads[j].x[ids[k]]-cx)*aspect;
                y[k]=quads[j].y[ids[k]]-cy;
            }
            for(k=0u;k<3u;k++){
                unsigned next=(k+1u)%3u;
                if(SpiderAtomicContour_RaySegment(dx,dy,
                    x[k],y[k],x[next],y[next],&t) && t<nearest)
                    nearest=t;
            }
        }
        /* Missing ray means original material did NOT supply a closed
         * aperture. Never invent a silhouette from unsupported sources. */
        if(!(nearest>1.0f && nearest<(float)(w+h)*4.0f))return 0;
        contour->radius[i]=nearest;
        if(nearest<minr)minr=nearest;
        if(nearest>maxr)maxr=nearest;
    }
    if(!(maxr<minr*32.0f))return 0;
    /* Sudden large jumps indicate islands or a non-star-shaped source,
     * where first-ray-hit would not preserve the native contour. */
    for(i=0u;i<SPIDER_ATOMIC_CONTOUR_SAMPLES;i++){
        float a=contour->radius[i];
        float b=contour->radius[(i+1u)%SPIDER_ATOMIC_CONTOUR_SAMPLES];
        if(fabsf(a-b)>fmaxf(8.0f,fmaxf(a,b)*.22f))return 0;
    }
    contour->minimum=minr;contour->maximum=maxr;
    return 1;
}
/* One connected black exterior, ONE D3D7 TRIANGLELIST batch. The
 * outer ring is analytically infinite black matte clipped by viewport;
 * it is not a separate, independently animated spider limb. Only the
 * inner contour is game artwork and uses the exact native zoom.
 */
static inline int SpiderAtomicContour_Build(
    const SpiderAtomicContour *contour,unsigned w,unsigned h,float zoom,
    SpiderAtomicVectorPoint *out,unsigned capacity,unsigned *used)
{
    unsigned i,n=0u;
    float cx,cy,outer_radius;
    if(used)*used=0u;
    if(!contour || !out || !used ||
       w<640u || h<480u || !(zoom>=.015f && zoom<=24.0f) ||
       capacity<SPIDER_ATOMIC_CONTOUR_VERTICES ||
       !(contour->minimum>0.0f && contour->maximum>=contour->minimum))
        return 0;
    cx=(float)w*.5f;cy=(float)h*.5f;
    outer_radius=fmaxf((float)(w+h)*4.0f,contour->maximum*zoom*4.0f);
    for(i=0u;i<SPIDER_ATOMIC_CONTOUR_SAMPLES;i++){
        unsigned next=(i+1u)%SPIDER_ATOMIC_CONTOUR_SAMPLES;
        float a0=(2.0f*SPIDER_ATOMIC_CONTOUR_PI*(float)i)/
                 (float)SPIDER_ATOMIC_CONTOUR_SAMPLES;
        float a1=(2.0f*SPIDER_ATOMIC_CONTOUR_PI*(float)(i+1u))/
                 (float)SPIDER_ATOMIC_CONTOUR_SAMPLES;
        float x0=cosf(a0),y0=sinf(a0),x1=cosf(a1),y1=sinf(a1);
        SpiderAtomicVectorPoint in0,in1,out0,out1;
        in0.x=cx+x0*contour->radius[i]*zoom;
        in0.y=cy+y0*contour->radius[i]*zoom;
        in1.x=cx+x1*contour->radius[next]*zoom;
        in1.y=cy+y1*contour->radius[next]*zoom;
        out0.x=cx+x0*outer_radius;out0.y=cy+y0*outer_radius;
        out1.x=cx+x1*outer_radius;out1.y=cy+y1*outer_radius;
        if(!SpiderAtomicVector_AppendTriangle(out,capacity,&n,in0,out0,out1) ||
           !SpiderAtomicVector_AppendTriangle(out,capacity,&n,in0,out1,in1))
            return 0;
    }
    *used=n;
    return 1;
}
#endif
