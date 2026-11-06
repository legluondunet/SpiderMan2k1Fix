#ifndef SPIDERMAN_UI_TRANSITION_LIVE_DIRECT_H
#define SPIDERMAN_UI_TRANSITION_LIVE_DIRECT_H
/* V17 experimental direct CURRENT-FRAME native black-quad renderer.
 *
 * The original game may retire black iris quads as the opening expands
 * beyond the viewport. The V16 assumption "black must cover every edge"
 * is not true at the end of the wipe. Draw only the native black surfaces
 * that the hook ACTUALLY intercepted, with one common 4:3 X correction.
 *
 * The side gutters are black ONLY at Y intervals where original black
 * material crosses the source's physical X=0 or X=W boundary. As the
 * game opens the iris across an edge, that gutter naturally becomes
 * transparent at that Y -- no frozen reference teeth, no flat matte,
 * no guessed timing, no second zoom, and no need to merge 92 quads.
 *
 * Native opaque black source is validated by the caller's D3D7
 * material/caller gate. This header handles only deterministic geometry.
 */
#include "ui_transition_atomic_vector.h"
#include <math.h>
#define SPIDER_LIVE_DIRECT_MAX_VERTS (SPIDER_ATOMIC_MASK_MAX * 30u)

static inline int SpiderLiveDirect_EdgeSpan(
    const SpiderAtomicMaskQuad *q,unsigned a,unsigned b,unsigned c,
    float xedge,float height,float *ymin,float *ymax)
{
    unsigned index[3]={a,b,c},i,n=0u;
    float lo=0.0f,hi=0.0f,area;
    float ax=q->x[a],ay=q->y[a],bx=q->x[b],by=q->y[b];
    float cx=q->x[c],cy=q->y[c];
    area=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax);
    if(fabsf(area)<.00001f)return 0;
    for(i=0u;i<3u;i++){
        unsigned ia=index[i],ib=index[(i+1u)%3u];
        float x0=q->x[ia],y0=q->y[ia];
        float x1=q->x[ib],y1=q->y[ib];
        float d0=x0-xedge,d1=x1-xedge,y;
        if(fabsf(d0)<.0001f){
            y=y0;
            if(!n)lo=hi=y;else {if(y<lo)lo=y;if(y>hi)hi=y;}
            ++n;
        }
        if((d0<0.0f && d1>0.0f)||(d0>0.0f && d1<0.0f)){
            float t=(xedge-x0)/(x1-x0);
            y=y0+t*(y1-y0);
            if(!n)lo=hi=y;else {if(y<lo)lo=y;if(y>hi)hi=y;}
            ++n;
        }
    }
    if(n<2u || hi-lo<.0001f || hi<=0.0f || lo>=height)return 0;
    *ymin=fmaxf(lo,0.0f);*ymax=fminf(hi,height);
    return *ymax-*ymin>.0001f;
}
static inline int SpiderLiveDirect_Build(
    const SpiderAtomicMaskQuad *native,unsigned count,unsigned w,unsigned h,
    SpiderAtomicVectorPoint *output,unsigned capacity,unsigned *used,
    unsigned *left_gutter,unsigned *right_gutter)
{
    unsigned i,k,n=0u,left=0u,right=0u;
    float cx,aspect,gutter;
    if(used)*used=0u;
    if(left_gutter)*left_gutter=0u;
    if(right_gutter)*right_gutter=0u;
    if(!native || !output || !used || count<1u ||
       count>SPIDER_ATOMIC_MASK_MAX ||
       capacity<SPIDER_LIVE_DIRECT_MAX_VERTS ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u<(uint64_t)h*4u)
        return 0;
    cx=(float)w*.5f;
    aspect=(float)h*4.0f/((float)w*3.0f);
    if(!(aspect>0.0f && aspect<=1.000001f))return 0;
    gutter=cx*(1.0f-aspect);
    for(i=0u;i<count;i++){
        SpiderAtomicVectorPoint p[4];
        for(k=0u;k<4u;k++){
            float x=native[i].x[k],y=native[i].y[k];
            if(!(x>=-(float)w*.10f-.01f &&
                 x<=(float)w*1.10f+.01f &&
                 y>=-(float)h*.10f-.01f &&
                 y<=(float)h*1.10f+.01f))
                return 0;
            p[k].x=cx+(x-cx)*aspect;
            p[k].y=y;
        }
        if(!SpiderAtomicVector_AppendTriangle(
                output,capacity,&n,p[0],p[1],p[2]) ||
           !SpiderAtomicVector_AppendTriangle(
                output,capacity,&n,p[0],p[2],p[3]))
            return 0;
        /* Only two original fan triangles are eligible for gutter
         * extension. A side rectangle is emitted iff THAT triangle
         * physically intersects the corresponding source edge.
         * No subtraction, no min/max of disjoint legs, no invented
         * black outer region when the real iris has opened. */
        if(gutter>0.0001f){
            unsigned tri;
            for(tri=0u;tri<2u;tri++){
                unsigned a=0u,b=tri==0u?1u:2u,c=tri==0u?2u:3u;
                float miny,maxy;
                if(SpiderLiveDirect_EdgeSpan(&native[i],a,b,c,
                          0.0f,(float)h,&miny,&maxy)){
                    if(!SpiderAtomicVector_AppendRectangle(output,capacity,
                            &n,0.0f,miny,gutter,maxy))return 0;
                    ++left;
                }
                if(SpiderLiveDirect_EdgeSpan(&native[i],a,b,c,
                          (float)w,(float)h,&miny,&maxy)){
                    if(!SpiderAtomicVector_AppendRectangle(output,capacity,
                            &n,(float)w-gutter,miny,(float)w,maxy))return 0;
                    ++right;
                }
            }
        }
    }
    if(!n)return 0;
    *used=n;
    if(left_gutter)*left_gutter=left;
    if(right_gutter)*right_gutter=right;
    return 1;
}
#endif
