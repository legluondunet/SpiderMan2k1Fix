#ifndef SPIDERMAN2K1FIX_HUD_BOSS_NATIVE_H
#define SPIDERMAN2K1FIX_HUD_BOSS_NATIVE_H

/* Source-verified second boss-HUD render variant. This is intentionally
 * NOT a level-name switch; original D3D7 depth, texture, size, tint and
 * source quad gates must identify the group wherever it is drawn.
 *
 * Original lossless F9 scrn0042.png + matching 2026-11-05 Log=1:
 *   fill:     x=1383.8..1691.2, y=130.5..166.5,
 *             z=.00098, RHW=8.0113, alpha=FF, NO texture;
 *   frame:    x=1590..1687.5, y=108..180,
 *             z=.00024, RHW=17.1292, color=FFFFFFFF, tex=32x32;
 *   portrait: x=1680..1848.8, y=72..211.5,
 *             z=.00024, RHW=17.1292, color=FFFFFFFF, tex=64x64.
 * All are triangle fans, FVF=0x144, caller RVA=0x00102C72.
 *
 * Existing prior boss HUD has different layers (RHW ~12..22) and
 * remains under the old right-edge anchored adaptive classifier.
 * Dreamcast shows the entire Rhino-style group uses a 4:3
 * SCREEN-CENTER anchor, unlike red/green reticles whose own center
 * must remain fixed. No Y, Z, RHW, tint, UV or texture is changed.
 *
 * A separate dark HP marker is not source-verified: fail closed for
 * unknown layers, so damage-mask handling may need additional trace.
 */
#include "hud_boss_probe.h"
#include <stdint.h>

static inline int SpiderHudBossNative_Near(float v,float center,float tolerance)
{
    return v>=center-tolerance && v<=center+tolerance;
}

static inline int SpiderHudBossNative_MightMatch(const SpiderMenuVertex *v)
{
    if(!v || (v[0].diffuse>>24)!=0xFFu) return 0;
    if(SpiderHudBossNative_Near(v[0].z,0.00098f,0.000035f) &&
       SpiderHudBossNative_Near(v[0].rhw,8.0113f,0.065f))
        return 1;
    return (v[0].diffuse==0xFFFFFFFFu &&
            SpiderHudBossNative_Near(v[0].z,0.00024f,0.000035f) &&
            SpiderHudBossNative_Near(v[0].rhw,17.1292f,0.065f));
}

static inline int SpiderHudBossNative_Transform(
    const SpiderMenuVertex *src,SpiderMenuVertex *dst,
    unsigned count,unsigned primitive_type,uint32_t fvf,
    uint32_t caller_rva,unsigned tex_w,unsigned tex_h,
    unsigned screen_w,unsigned screen_h)
{
    int kind;
    unsigned i;
    float factor,mid;
    if(!src || !dst || src==dst ||
       !SpiderHudBossNative_MightMatch(src))
        return SPIDER_HUD_BOSS_PROBE_NONE;
    kind=SpiderHudBossProbe_Classify(src,count,primitive_type,fvf,
              caller_rva,tex_w,tex_h,screen_w,screen_h);
    if(kind==SPIDER_HUD_BOSS_PROBE_NONE)return kind;
    if(kind==SPIDER_HUD_BOSS_PROBE_FILL){
        if(tex_w!=0u || tex_h!=0u ||
           !SpiderHudBossNative_Near(src[0].z,0.00098f,0.000035f) ||
           !SpiderHudBossNative_Near(src[0].rhw,8.0113f,0.065f))
            return SPIDER_HUD_BOSS_PROBE_NONE;
    }else{
        if(!((kind==SPIDER_HUD_BOSS_PROBE_FRAME &&
              tex_w==32u && tex_h==32u) ||
             (kind==SPIDER_HUD_BOSS_PROBE_PORTRAIT &&
              tex_w==64u && tex_h==64u)) ||
           src[0].diffuse!=0xFFFFFFFFu ||
           !SpiderHudBossNative_Near(src[0].z,0.00024f,0.000035f) ||
           !SpiderHudBossNative_Near(src[0].rhw,17.1292f,0.065f))
            return SPIDER_HUD_BOSS_PROBE_NONE;
    }
    for(i=0;i<4u;i++){
        if(!SpiderHudBossNative_Near(src[i].z,src[0].z,0.000005f) ||
           !SpiderHudBossNative_Near(src[i].rhw,src[0].rhw,0.006f) ||
           (src[i].diffuse>>24)!=0xFFu ||
           (kind!=SPIDER_HUD_BOSS_PROBE_FILL &&
            src[i].diffuse!=0xFFFFFFFFu))
            return SPIDER_HUD_BOSS_PROBE_NONE;
    }
    factor=(4.0f*(float)screen_h)/(3.0f*(float)screen_w);
    mid=0.5f*(float)screen_w;
    for(i=0;i<4u;i++){
        dst[i]=src[i];
        dst[i].x=mid+(src[i].x-mid)*factor;
    }
    return kind;
}
#endif
