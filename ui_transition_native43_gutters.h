#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_GUTTERS_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_GUTTERS_H
/*
 * V20 experimental native-iris gutters; add-on to the validated V19.
 *
 * V19 already draws all ORIGINAL D3D7 iris fans correctly, but compresses
 * their X coordinates to the centered 4:3 area. The 16:9 side gutters
 * are consequently uncovered from the very first opening frame.
 *
 * For each ORIGINAL verified black triangle, find the vertical part of
 * its intersection with the ORIGINAL source x=0 or x=width. Only that
 * part of the corresponding PHYSICAL 16:9 side strip gets a matching
 * black quad, in the original D3D7 call context. This is an extension
 * of the existing black edge, NOT a recreation of the spider/legs.
 *
 * At first opening: original matte reaches both source edges all the
 * way down, so the whole physical display is black except the opening.
 * At late opening: native coverage at an edge decreases/disappears, so
 * the matching physical side strip opens without any guessed timer.
 *
 * Never modify the input vertices, actual native iris, opaque cinematic
 * bars, 3D world, HUD, ESC, alpha state, topology, FOV or frame timing.
 *
 * All triangles are generated upfront (up to four side rectangles per
 * original fan). This code does not issue D3D7 calls or change state.
 * A caller must pass only VERIFIED native untextured black iris quads.
 */
#include "menu_scale.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define SPIDER_NATIVE43_GUTTER_MAX 4u

static inline float SpiderNative43Gutter_Cross(
    float ax,float ay,float bx,float by,float cx,float cy)
{
    return (bx-ax)*(cy-ay)-(by-ay)*(cx-ax);
}

/* Intersection of a native source triangle with original screen x=edge.
 * A single point or a degenerate triangle has no side-strip area.
 * Output is clamped to the physical screen height (same native Y).
 */
static inline int SpiderNative43Gutter_Span(
    const SpiderMenuVertex *v,unsigned a,unsigned b,unsigned c,
    float edge,float height,float *y0,float *y1)
{
    unsigned t,idx[3]={a,b,c},n=0;
    float lo=0.0f,hi=0.0f;
    if(!v || !y0 || !y1 ||
       fabsf(SpiderNative43Gutter_Cross(
           v[a].x,v[a].y,v[b].x,v[b].y,v[c].x,v[c].y))<.00001f)
        return 0;
    for(t=0u;t<3u;t++){
        unsigned i=idx[t],j=idx[(t+1u)%3u];
        float dx0=v[i].x-edge,dx1=v[j].x-edge,y;
        if(fabsf(dx0)<.0001f){
            y=v[i].y;
            if(!n)lo=hi=y;
            else {if(y<lo)lo=y;if(y>hi)hi=y;}
            ++n;
        }
        if((dx0<0.0f && dx1>0.0f)||
           (dx0>0.0f && dx1<0.0f)){
            y=v[i].y+(edge-v[i].x)*(v[j].y-v[i].y)/
                   (v[j].x-v[i].x);
            if(!n)lo=hi=y;
            else {if(y<lo)lo=y;if(y>hi)hi=y;}
            ++n;
        }
    }
    if(n<2u || hi-lo<.0001f || hi<=0.0f || lo>=height)
        return 0;
    *y0=fmaxf(0.0f,lo);
    *y1=fminf(height,hi);
    return *y1-*y0>.0001f;
}

/* Rectangle built in original fan's winding direction so that native
 * D3D7 culling state can remain unchanged. Other fields copied directly
 * from the original verified black fan (no texture or alpha blending).
 */
static inline void SpiderNative43Gutter_Rect(
    const SpiderMenuVertex *source,unsigned a,unsigned b,unsigned c,
    float xl,float xr,float yt,float yb,SpiderMenuVertex result[4])
{
    unsigned i;
    float sign=SpiderNative43Gutter_Cross(
        source[a].x,source[a].y,
        source[b].x,source[b].y,
        source[c].x,source[c].y);
    for(i=0u;i<4u;i++)result[i]=source[0];
    result[0].x=xl;result[0].y=yt;
    if(sign>=0.0f){
        result[1].x=xr;result[1].y=yt;
        result[2].x=xr;result[2].y=yb;
        result[3].x=xl;result[3].y=yb;
    }else{
        result[1].x=xl;result[1].y=yb;
        result[2].x=xr;result[2].y=yb;
        result[3].x=xr;result[3].y=yt;
    }
}

/* Return 0 for invalid inputs; 1 for a valid source (including zero
 * eligible gutters). out[i] is a 4-vertex D3DPT_TRIANGLEFAN black rect.
 * It is wholly outside the centered 4:3 canvas: never paints the iris.
 */
static inline int SpiderNative43Gutter_Build(
    const SpiderMenuVertex *source,unsigned w,unsigned h,
    SpiderMenuVertex out[SPIDER_NATIVE43_GUTTER_MAX][4],
    unsigned *out_count,unsigned *left_count,unsigned *right_count)
{
    unsigned tri,n=0u,l=0u,r=0u;
    float gutter;
    if(out_count)*out_count=0u;
    if(left_count)*left_count=0u;
    if(right_count)*right_count=0u;
    if(!source || !out || !out_count ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u<=(uint64_t)h*4u)
        return 0;
    gutter=((float)w-(float)h*4.0f/3.0f)*.5f;
    if(!(gutter>=0.0f && gutter<(float)w*.5f))
        return 0;
    for(tri=0u;tri<2u;tri++){
        unsigned a=0u,b=tri?2u:1u,c=tri?3u:2u;
        float y0,y1;
        if(SpiderNative43Gutter_Span(source,a,b,c,0.0f,
                                    (float)h,&y0,&y1)){
            SpiderNative43Gutter_Rect(source,a,b,c,
                                     0.0f,gutter,y0,y1,out[n++]);
            ++l;
        }
        if(SpiderNative43Gutter_Span(source,a,b,c,(float)w,
                                    (float)h,&y0,&y1)){
            SpiderNative43Gutter_Rect(source,a,b,c,
                                     (float)w-gutter,(float)w,
                                     y0,y1,out[n++]);
            ++r;
        }
    }
    *out_count=n;
    if(left_count)*left_count=l;
    if(right_count)*right_count=r;
    return 1;
}
#endif
