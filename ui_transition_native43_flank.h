#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_FLANK_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_FLANK_H
/* V21 hypothesis test: extend only the game's LARGE, FULL-HEIGHT black
 * outer-matte rectangles. NEVER a hooked spider limb.
 *
 * V20 extended every native black triangle touching source x=0/W into
 * the physical side gutters, turning a hook into a 240px-wide rectangle.
 * The opening itself is already correct in native V19. No new artwork,
 * masks, topology, timed zoom, or tooth interpolation may change it.
 *
 * A source fan is eligible here only if it is an axis-aligned rectangle
 * with two X/Y levels, covering the WHOLE original viewport height and
 * tied to the OUTER original screen edge. Limb polygons, partial-height
 * triangles, out-of-frame tooth hooks and slanted shapes cannot pass.
 *
 * On the actual captured small opening, two 0..746.25 / 1173.75..1920
 * full-height native flank rectangles (two windings) are eligible. Once
 * those original outer rectangles are absent, no synthetic teeth or
 * full-screen side extensions are ever generated. This may expose a
 * POP when the original engine retires its outer matte. That potential
 * animation limitation is deliberately surfaced for the hardware A/B
 * test; do not hide it with guessed timing or per-leg rectangles.
 */
#include "menu_scale.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static inline float SpiderNativeFlank_Cross(
    const SpiderMenuVertex *v,unsigned a,unsigned b,unsigned c)
{
    return (v[b].x-v[a].x)*(v[c].y-v[a].y)-
           (v[b].y-v[a].y)*(v[c].x-v[a].x);
}

/* Returns 1 if the source is valid; *count is zero for an ineligible
 * source (most mask polygons). Only an original full-height flank can
 * generate exactly ONE full-height 16:9 gutter black rectangle. */
static inline int SpiderNativeFlank_Build(
    const SpiderMenuVertex *source,unsigned width,unsigned height,
    SpiderMenuVertex result[4],unsigned *side)
{
    unsigned i,k,grid[2][2]={{0,0},{0,0}};
    float minx,maxx,miny,maxy,eps=.5f,cx,gutter,area;
    int left=0,right=0;
    if(side)*side=0u;
    if(!source || !result || !side || width<640u || height<480u ||
       width>16384u || height>16384u ||
       (uint64_t)width*3u<=(uint64_t)height*4u)
        return 0;
    minx=maxx=source[0].x;
    miny=maxy=source[0].y;
    for(i=0u;i<4u;i++){
        float x=source[i].x,y=source[i].y;
        if(!isfinite(x)||!isfinite(y)||
           !(x>=-(float)width*32.0f && x<=(float)width*33.0f &&
             y>=-(float)height*32.0f && y<=(float)height*33.0f))
            return 0;
        if(x<minx)minx=x;
        if(x>maxx)maxx=x;
        if(y<miny)miny=y;
        if(y>maxy)maxy=y;
    }
    /* A physical outer matte must cover both original top/bottom edges.
     * The quads that constitute individual legs never do. */
    if(!(miny<=eps && maxy>=(float)height-eps &&
         maxx-minx>=(float)width*.05f &&
         maxy-miny>=(float)height*.95f))
        return 1;
    cx=(float)width*.5f;
    left=minx<=eps && maxx<cx-eps && maxx>=(float)width*.05f;
    right=maxx>=(float)width-eps &&
          minx>cx+eps && minx<=(float)width*.95f;
    if(!left && !right)return 1;
    /* Verify the source is an actual rectangle, not a diagonal hooked
     * limb that happens to span the screen. Four distinct corners and
     * two X/Y coordinate levels are mandatory, in either fan winding. */
    for(i=0u;i<4u;i++){
        int xi=fabsf(source[i].x-minx)<=eps?0:
               (fabsf(source[i].x-maxx)<=eps?1:-1);
        int yi=fabsf(source[i].y-miny)<=eps?0:
               (fabsf(source[i].y-maxy)<=eps?1:-1);
        if(xi<0 || yi<0)return 1;
        if(++grid[xi][yi]>1u)return 1;
    }
    area=SpiderNativeFlank_Cross(source,0u,1u,2u);
    if(fabsf(area)<.001f)return 1;
    gutter=((float)width-(float)height*4.0f/3.0f)*.5f;
    if(!(gutter>0.0f && gutter<cx))return 0;
    for(i=0u;i<4u;i++)result[i]=source[0];
    /* Retain the original triangle winding and direct D3D7 material
     * state rather than changing culling or creating a separate layer. */
    result[0].x=left?0.0f:(float)width-gutter;
    result[0].y=0.0f;
    if(area>0.0f){
        result[1].x=left?gutter:(float)width;
        result[1].y=0.0f;
        result[2].x=left?gutter:(float)width;
        result[2].y=(float)height;
        result[3].x=result[0].x;
        result[3].y=(float)height;
    }else{
        result[1].x=result[0].x;
        result[1].y=(float)height;
        result[2].x=left?gutter:(float)width;
        result[2].y=(float)height;
        result[3].x=result[2].x;
        result[3].y=0.0f;
    }
    *side=left?1u:2u;
    return 1;
}
#endif
