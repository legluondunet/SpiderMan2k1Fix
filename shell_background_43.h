#ifndef SPIDERMAN2K1FIX_SHELL_BACKGROUND_43_H
#define SPIDERMAN2K1FIX_SHELL_BACKGROUND_43_H

/* Experimental 4:3 aspect trial for the ORIGINAL game's shell background
 * rendering family. The retail game has two separate D3D7 DrawPrimitive
 * return sites. UI-CANVAS already maps 0x00102C72 (panel/text), but does
 * not map this immediate 0x00103389 path. 
 *
 * This policy is intentionally NOT a universal fullscreen modifier: it
 * matches only a large, flat, screen-space quad emitted by that original
 * immediate primitive site while a source-verified native shell-like
 * context is active. The host must enforce exact retail EXE, valid shell
 * state and explicit opt-in. Failing any check preserves ORIGINAL coords.
 *
 * Screenshot and binary provenance are not proof the candidate is menubg:
 * human A/B Wine screenshots are REQUIRED before this can ship by default.
 */
#include "menu_scale.h"
#include <stdint.h>
#include <string.h>
#define SPIDER_SHELL_BG43_CALLER_RVA 0x00103389u

static inline int SpiderShellBackground43_Transform(
    const SpiderMenuVertex *src, SpiderMenuVertex *dst,
    unsigned primitive_type, uint32_t fvf, uint32_t caller_rva,
    unsigned output_w, unsigned output_h, int native_shell_active)
{
    float l,r,t,b,w,h,scale,z0,rhw0;
    unsigned i;
    if(!native_shell_active || !src || !dst || src==dst ||
       primitive_type!=6u || fvf!=0x144u ||
       caller_rva!=SPIDER_SHELL_BG43_CALLER_RVA ||
       output_w<640u || output_h<480u ||
       output_w>16384u || output_h>16384u ||
       (uint64_t)output_w*3u<=(uint64_t)output_h*4u)
      return 0;
    w=(float)output_w;h=(float)output_h;
    l=r=src[0].x;t=b=src[0].y;
    z0=src[0].z;rhw0=src[0].rhw;
    for(i=0u;i<4u;i++) {
        /* Finite screen coordinates, very near flat depth, no projected
         * world geometry. Unknown primitives are not transformed. */
        if(!(src[i].x>=-w*0.05f && src[i].x<=w*1.05f &&
             src[i].y>=-h*0.05f && src[i].y<=h*1.05f &&
             src[i].z>=0.0f && src[i].z<=0.020f &&
             src[i].rhw>=2.0f && src[i].rhw<=64.0f &&
             src[i].z-z0>=-0.00015f && src[i].z-z0<=0.00015f &&
             src[i].rhw-rhw0>=-0.15f && src[i].rhw-rhw0<=0.15f))
          return 0;
        if(src[i].x<l)l=src[i].x;
        if(src[i].x>r)r=src[i].x;
        if(src[i].y<t)t=src[i].y;
        if(src[i].y>b)b=src[i].y;
    }
    /* A whole-screen decorative material is large. Restrict against
     * smaller gameplay objects even during a transient shell candidate.
     * This is NOT based on the name of the SAVE GAME menu or its texture. */
    if(!(r-l>=w*0.60f && b-t>=h*0.40f))
      return 0;
    scale=(4.0f*h)/(3.0f*w);
    memcpy(dst,src,4u*sizeof(*dst));
    for(i=0u;i<4u;i++)
      dst[i].x=0.5f*w+(src[i].x-0.5f*w)*scale;
    return 1;
}

#endif
