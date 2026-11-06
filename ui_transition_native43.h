#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_H
/*
 * V18 Dreamcast-comparison experiment: retain ORIGINAL PC D3D7 iris
 * DrawPrimitive call order, topology, vertex motion and render state.
 *
 * The 3D world remains widescreen. Only black-mask X coordinates are
 * presented within the CENTERED 4:3 game canvas (the actual Dreamcast
 * Flycast wide-hack behavior seen in the user reference film).
 *
 * No synthetic outer matte, no left/right gutter replay, no mask
 * texture, no union/trapezoid/sprited silhouette, no double zoom,
 * no topological modification, no new DRAW, no future-frame buffer.
 * Preserve original FVF, X/Y/Z/RHW, diffuse/specular/UV except X.
 *
 * Visual comparison requires Wine testing; 4:3 is an observed display
 * behavior, NOT proof that Dreamcast and Win32 renderers share code.
 */
#include "menu_scale.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

/* SHA-verified original SpideyPC.exe call site that draws the iris. */
#define SPIDER_TRANSITION_CALLER_RVA 0x00102C72u

/* V19 experimental: native quads may legitimately extend FAR beyond
 * the viewport as the game animates the iris. D3D7 clips those vertices.
 * V18 accidentally mapped only the +/-10% quads and forwarded the rest
 * unchanged, mixing coordinate spaces and visibly detaching the hooks.
 *
 * overhang is a symmetric multiple of physical width/height, bounded
 * for numeric safety. 0.1 preserves EXACT V18 behavior for A/B.
 * 32.0 allows fully offscreen original triangles without inventing art.
 */
static inline int SpiderNative43_MapBounded(
    const SpiderMenuVertex *source,SpiderMenuVertex *destination,
    unsigned count,unsigned width,unsigned height,float overhang)
{
    unsigned i;
    float cx,k;
    if(!source || !destination || count!=4u ||
       width<640u || height<480u ||
       width>16384u || height>16384u ||
       !(overhang>=.1f && overhang<=32.0f) ||
       (uint64_t)width*3u<=(uint64_t)height*4u)
        return 0;
    cx=(float)width*.5f;
    k=(float)height*4.0f/((float)width*3.0f);
    if(!(k>0.0f && k<1.0f))return 0;
    for(i=0u;i<count;i++){
        const SpiderMenuVertex *v=&source[i];
        if(!isfinite(v->x) || !isfinite(v->y) ||
           !(v->x>=-(float)width*overhang &&
             v->x<=(float)width*(1.0f+overhang) &&
             v->y>=-(float)height*overhang &&
             v->y<=(float)height*(1.0f+overhang)))
            return 0;
    }
    /* Build all four vertices together: no per-tooth or edge mapping. */
    memcpy(destination,source,count*sizeof(*source));
    for(i=0u;i<count;i++)
        destination[i].x=cx+(source[i].x-cx)*k;
    return 1;
}
/* V18 strict public API unchanged; V19 must be explicitly enabled. */
static inline int SpiderNative43_Map(
    const SpiderMenuVertex *source,SpiderMenuVertex *destination,
    unsigned count,unsigned width,unsigned height)
{
    return SpiderNative43_MapBounded(
        source,destination,count,width,height,.1f);
}
#endif
