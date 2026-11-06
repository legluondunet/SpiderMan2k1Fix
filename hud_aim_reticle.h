#ifndef SPIDERMAN2K1FIX_HUD_AIM_RETICLE_H
#define SPIDERMAN2K1FIX_HUD_AIM_RETICLE_H

/* Source-verified red/green D3D7 targeting ring classifier.
 * GE-Proton11-12 F9 + Log=1 identify the same ring family at several
 * different perspective RHW depths: 9.9878, 7.8952, 5.9130, and now
 * 9.4993 (scrn0090 / 2026-11-06 17:34:01). A fixed-depth whitelist
 * missed the last ring and cannot cover arbitrary levels.
 *
 * Identity is instead the combination of the original 00102C72 caller,
 * triangle-fan FVF 0x144, 64x64 bound texture, Z=.00146, uniform
 * red=B0FF4040 or green=B040FF40 tint, valid perspective RHW, and
 * 4:3-to-widescreen-stretched circular geometry. No individual
 * level, fixed screen coordinate, or discrete RHW value is required.
 *
 * Transform X around the ring's OWN center. Preserve its position,
 * Y/UV/color/Z/RHW and do not touch arbitrary world or menu sprites.
 * The host D3D7 wrapper also gates active gameplay and SHA-pinned HUD.
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

/* Cheap prefilter: the retail renderer varies RHW continuously with
 * camera/target depth. Require a valid perspective divisor, not a
 * per-scene whitelist; the caller, texture, tint, Z, and stretched
 * circular quad are checked separately before any vertex is touched.
 * The upper bound rejects invalid or unrelated extreme perspective
 * layers. NaN fails the ordered comparisons.
 */
static inline int SpiderHudAimReticle_PlausibleLayer(float z,float rhw)
{
    return SpiderHudAim_Near(z,0.00146f,0.000035f) &&
           rhw>0.0f && rhw<=128.0f;
}

static inline int SpiderHudAimReticle_MightMatch(
    const SpiderMenuVertex *src)
{
    return src &&
      (src[0].diffuse==0xB0FF4040u ||
       src[0].diffuse==0xB040FF40u) &&
      SpiderHudAimReticle_PlausibleLayer(src[0].z,src[0].rhw) &&
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
