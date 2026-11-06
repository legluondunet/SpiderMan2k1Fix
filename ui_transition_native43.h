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

static inline int SpiderNative43_Map(
    const SpiderMenuVertex *source,SpiderMenuVertex *destination,
    unsigned count,unsigned width,unsigned height)
{
    unsigned i;
    float cx,k;
    if(!source || !destination || count!=4u ||
       width<640u || height<480u ||
       width>16384u || height>16384u ||
       (uint64_t)width*3u<=(uint64_t)height*4u)
        return 0;
    cx=(float)width*.5f;
    k=(float)height*4.0f/((float)width*3.0f);
    if(!(k>0.0f && k<1.0f))return 0;
    for(i=0u;i<count;i++){
        const SpiderMenuVertex *v=&source[i];
        if(!isfinite(v->x) || !isfinite(v->y) ||
           !(v->x>=-(float)width*.10f &&
             v->x<=(float)width*1.10f &&
             v->y>=-(float)height*.10f &&
             v->y<=(float)height*1.10f))
            return 0;
    }
    /* Build all four vertices together: no per-tooth or edge mapping. */
    memcpy(destination,source,count*sizeof(*source));
    for(i=0u;i<count;i++)
        destination[i].x=cx+(source[i].x-cx)*k;
    return 1;
}
#endif
