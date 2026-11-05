#ifndef SPIDERMAN2K1FIX_HUD_BOSS_PROBE_H
#define SPIDERMAN2K1FIX_HUD_BOSS_PROBE_H

/* Bounded READ-ONLY diagnostic classifier for original D3D7 boss
 * bar/portrait pieces. No transform is performed here.
 *
 * Rhino evidence (2026-11-05 01:01:26):
 *   source 32x32 boss border piece at x=1590..1687.5,
 *   y=117..189, Z=0.00024, RHW=17.1292, color=FFFFFFFF;
 * earlier Venom/Scorpion cases were ~RHW 21..22, so their
 * existing strict adaptive HUD gate excluded this boss context.
 *
 * The 64x64 portrait and untextured gradient fill were NOT included
 * in the pre-existing broad HUD-VISUAL logs; capture each layer,
 * its real depth and quad before altering the renderer. */
#include "menu_scale.h"
#include <stdint.h>

enum SpiderHudBossProbeKind {
    SPIDER_HUD_BOSS_PROBE_NONE=0,
    SPIDER_HUD_BOSS_PROBE_FILL=1,
    SPIDER_HUD_BOSS_PROBE_FRAME=2,
    SPIDER_HUD_BOSS_PROBE_PORTRAIT=3
};

static inline int SpiderHudBossProbe_Classify(
    const SpiderMenuVertex *v,
    unsigned count,unsigned primitive_type,uint32_t fvf,
    uint32_t caller_rva,unsigned texture_w,unsigned texture_h,
    unsigned screen_w,unsigned screen_h)
{
    float l,r,t,b,width,height;
    unsigned i;
    if(!v || count!=4u || primitive_type!=6u || fvf!=0x144u ||
       caller_rva!=0x00102C72u || screen_w<640u || screen_h<480u ||
       screen_w>16384u || screen_h>16384u ||
       (uint64_t)screen_w*3u <= (uint64_t)screen_h*4u)
        return SPIDER_HUD_BOSS_PROBE_NONE;
    l=r=v[0].x;t=b=v[0].y;
    for(i=0;i<4u;i++){
        if(v[i].x<l)l=v[i].x;
        if(v[i].x>r)r=v[i].x;
        if(v[i].y<t)t=v[i].y;
        if(v[i].y>b)b=v[i].y;
    }
    width=r-l;height=b-t;
    if(!(l>=(float)screen_w*0.46f &&
         r<=(float)screen_w+64.0f &&
         t>=(float)screen_h*0.045f &&
         b<=(float)screen_h*0.36f &&
         width>16.0f && width<(float)screen_w*0.60f &&
         height>=(float)screen_h*0.020f &&
         height<=(float)screen_h*0.20f &&
         (v[0].diffuse>>24)==0xFFu &&
         v[0].z>=0.0f && v[0].z<=0.002f &&
         v[0].rhw>3.0f && v[0].rhw<32.0f))
        return SPIDER_HUD_BOSS_PROBE_NONE;

    if(texture_w==0u && texture_h==0u &&
       t>=(float)screen_h*0.085f &&
       b<=(float)screen_h*0.32f &&
       height>=(float)screen_h*0.022f &&
       height<=(float)screen_h*0.065f &&
       v[0].z>=0.00045f && v[0].z<0.00145f)
        return SPIDER_HUD_BOSS_PROBE_FILL;

    if(texture_w==32u && texture_h==32u &&
       width>=(float)screen_h*0.040f &&
       width<=(float)screen_h*0.12f &&
       height>=(float)screen_h*0.045f &&
       height<=(float)screen_h*0.115f &&
       v[0].z>=0.00012f && v[0].z<0.00039f)
        return SPIDER_HUD_BOSS_PROBE_FRAME;

    if(texture_w==64u && texture_h==64u &&
       width>=(float)screen_h*0.105f &&
       width<=(float)screen_h*0.22f &&
       height>=(float)screen_h*0.105f &&
       height<=(float)screen_h*0.20f &&
       v[0].z>=0.00012f && v[0].z<0.00039f)
        return SPIDER_HUD_BOSS_PROBE_PORTRAIT;
    return SPIDER_HUD_BOSS_PROBE_NONE;
}
#endif
