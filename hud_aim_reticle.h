#ifndef SPIDERMAN2K1FIX_HUD_AIM_RETICLE_H
#define SPIDERMAN2K1FIX_HUD_AIM_RETICLE_H

/* Strict, source-verified red/green 3D aiming ring classifier.
 * GE-Proton11-12 F9 + matching Log=1 confirm TWO discrete render
 * layers, not one range spanning the levels:
 *
 *  level 1: scrn0028 red and scrn0029 green, RHW=9.9878;
 *  level 2: scrn0034 red, RHW=7.8952; green uses same layer.
 *
 * Both use original caller=00102C72, triangle-fan FVF 0x144,
 * 64x64 texture, Z=.00146, and identical color codes:
 * red=B0FF4040; green=B040FF40. Level-2 red F9 at 00:56:55
 * correlates with log sample=39 flip=4687 and unchanged color/Z/RHW.
 *
 * Each circle was horizontally stretched by 4:3->16:9. Transform
 * X around the *object's own center* only; never move the target,
 * modify Y/texture/color/Z/RHW, or touch generic world/2D draws.
 * The host D3D7 wrapper additionally gates native gameplay state,
 * installed SHA-pinned HUD fix and the actual bound texture.
 */
#include "hud_aim_geometry.h"
#include "menu_scale.h"
#include <stdint.h>

enum SpiderHudAimReticleKind {
    SPIDER_HUD_AIM_NONE=0,
    SPIDER_HUD_AIM_RED=1,
    SPIDER_HUD_AIM_GREEN=2
};

static inline int SpiderHudAim_Near(float v,float reference,float tolerance)
{
    return v>=reference-tolerance && v<=reference+tolerance;
}

/* Cheap fail-closed prefilter before a D3D7 texture COM query. */
/* Level-specific RHW must match one of the two captured depths.
 * Never interpolate (e.g. 8.9), because the engine draws unrelated
 * game and UI geometry at other layers through the same caller. */
static inline int SpiderHudAimReticle_ObservedDepth(float z,float rhw)
{
    return SpiderHudAim_Near(z,0.00146f,0.000035f) &&
           (SpiderHudAim_Near(rhw,9.9878f,0.060f) ||
            SpiderHudAim_Near(rhw,7.8952f,0.060f));
}

static inline int SpiderHudAimReticle_MightMatch(
    const SpiderMenuVertex *src)
{
    return src &&
      (src[0].diffuse==0xB0FF4040u ||
       src[0].diffuse==0xB040FF40u) &&
      SpiderHudAimReticle_ObservedDepth(src[0].z,src[0].rhw) &&
      SpiderHudAim_Near(src[0].u,0.0f,0.050f) &&
      SpiderHudAim_Near(src[0].v,0.0f,0.050f);
}

static inline int SpiderHudAimReticle_Transform(
    const SpiderMenuVertex *src,SpiderMenuVertex *dst,
    unsigned count,unsigned primitive_type,uint32_t fvf,
    uint32_t caller_rva,unsigned texture_w,unsigned texture_h,
    unsigned screen_w,unsigned screen_h)
{
    float l,r,t,b,centerx,centery,width,height,expected_ratio,ratio;
    unsigned i;
    if(!src || !dst || src==dst ||
       count!=4u || primitive_type!=6u || fvf!=0x144u ||
       caller_rva!=0x00102C72u || texture_w!=64u || texture_h!=64u ||
       SpiderHudAim_AspectFactor(screen_w,screen_h)>=1.0f ||
       !SpiderHudAimReticle_MightMatch(src))
        return SPIDER_HUD_AIM_NONE;

    l=r=src[0].x;t=b=src[0].y;
    for(i=0;i<4u;i++) {
        /* This translucent HUD sprite is uniformly tinted; reject
         * multicolored quads or 3D vertex gradients. */
        if(src[i].diffuse!=src[0].diffuse ||
           !SpiderHudAim_Near(src[i].z,src[0].z,0.000005f) ||
           !SpiderHudAim_Near(src[i].rhw,src[0].rhw,0.005f))
            return SPIDER_HUD_AIM_NONE;
        if(src[i].x<l)l=src[i].x;
        if(src[i].x>r)r=src[i].x;
        if(src[i].y<t)t=src[i].y;
        if(src[i].y>b)b=src[i].y;
    }
    width=r-l;height=b-t;
    if(!(width>0.0f && height>0.0f))
        return SPIDER_HUD_AIM_NONE;
    centerx=(l+r)*0.5f;centery=(t+b)*0.5f;
    expected_ratio=1.0f/SpiderHudAim_AspectFactor(screen_w,screen_h);
    ratio=width/height;
    if(!(l>=0.0f && r<=(float)screen_w &&
         t>=0.0f && b<=(float)screen_h &&
         height>=(float)screen_h*0.045f &&
         height<=(float)screen_h*0.30f &&
         width>=(float)screen_h*0.055f &&
         width<=(float)screen_h*0.42f &&
         centerx>=(float)screen_w*0.10f &&
         centerx<=(float)screen_w*0.90f &&
         centery>=(float)screen_h*0.10f &&
         centery<=(float)screen_h*0.90f &&
         ratio>=expected_ratio-0.085f &&
         ratio<=expected_ratio+0.085f))
        return SPIDER_HUD_AIM_NONE;

    for(i=0;i<4u;i++) {
        dst[i]=src[i];
        dst[i].x=SpiderHudAim_MapX(src[i].x,centerx,
                                   screen_w,screen_h);
    }
    return src[0].diffuse==0xB0FF4040u?
           SPIDER_HUD_AIM_RED:SPIDER_HUD_AIM_GREEN;
}
#endif
