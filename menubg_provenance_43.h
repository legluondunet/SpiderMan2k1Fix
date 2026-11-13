#ifndef SPIDERMAN2K1FIX_MENUBG_PROVENANCE_43_H
#define SPIDERMAN2K1FIX_MENUBG_PROVENANCE_43_H
/* Readable, testable policy only: executable provenance and original queue
 * record identity are verified by the host hook, not inferred from z/rhw
 * and NOT from the overall dimensions of arbitrary DrawPrimitive quads.
 *
 * Exact original SpideyPC retail binary:
 *  0x49FC98 and 0x48DACC CALL 0x509D20 (menubg handle 0x006A7780).
 *  0x50768F builds fixed queue records: 0x73C7B8 + index * 240.
 *  The original D3D7 flush at 0x502C6F draws record+0x10 (FVF 0x144).
 *
 * The optical trial is OFF unless the user sets an environment variable.
 * If either call signature or queue record cannot be proved, do nothing.
 */
#include "menu_scale.h"
#include <stdint.h>
#include <string.h>

#define SPIDER_MENUBG43_RECORD_CAP 0x3c00u
#define SPIDER_MENUBG43_RECORD_STRIDE 240u
#define SPIDER_MENUBG43_RECORD_BASE 0x0073C7B8u
#define SPIDER_MENUBG43_QUEUE_COUNT 0x00AC08F4u

static inline int SpiderMenubg43_RecordIndex(
    uintptr_t source,uint32_t *record_index)
{
    uintptr_t begin=(uintptr_t)SPIDER_MENUBG43_RECORD_BASE+16u;
    uintptr_t distance;
    if(!record_index || source<begin) return 0;
    distance=source-begin;
    if(distance%SPIDER_MENUBG43_RECORD_STRIDE) return 0;
    distance/=SPIDER_MENUBG43_RECORD_STRIDE;
    if(distance>=SPIDER_MENUBG43_RECORD_CAP)return 0;
    *record_index=(uint32_t)distance;
    return 1;
}

/* The original texture, UV, brightness, alpha, Z and RHW are sacred.
 * Centre only large, finite screen-space source quads in a 4:3 canvas.
 * The record's menubg identity and live original native-shell context
 * MUST be separately verified before this helper is called. */
/* The SHA-verified source may draw a few extra frames after Windows/Wine
 * notifies the original input shell that focus has changed. Do not turn the
 * temporary native-shell mismatch into a visible aspect-ratio jump before
 * PAUSED captures the frame. This narrowly authorizes the SAME authenticated
 * background texture during at most 16 original Flips of confirmed focus loss.
 * No other menu, texture, world poly or in-focus scene transition is covered.
 */
#define SPIDER_MENUBG43_FOCUS_TAIL_FLIPS 16u
static inline int SpiderMenubg43_FocusTailAllowed(
    uint32_t frame,uint32_t last_shell_frame,
    uint32_t texture,uint32_t verified_texture,int focus_lost)
{
    return focus_lost && verified_texture!=0u &&
           texture==verified_texture &&
           frame>=last_shell_frame &&
           frame-last_shell_frame<=SPIDER_MENUBG43_FOCUS_TAIL_FLIPS;
}

static inline int SpiderMenubg43_Transform(
    const SpiderMenuVertex *source,SpiderMenuVertex *result,
    unsigned count,unsigned type,uint32_t fvf,
    unsigned screen_w,unsigned screen_h)
{
    float l,r,t,b,w,h,scalar,z0,rhw0;
    unsigned i;
    if(!source || !result || source==result ||
       count!=4u || type!=6u || fvf!=0x144u ||
       screen_w<640u || screen_h<480u ||
       screen_w>16384u || screen_h>16384u ||
       (uint64_t)screen_w*3u<=(uint64_t)screen_h*4u)
       return 0;
    w=(float)screen_w;h=(float)screen_h;
    l=r=source[0].x;t=b=source[0].y;
    z0=source[0].z;rhw0=source[0].rhw;
    for(i=0u;i<4u;i++){
      /* Comparison-style bounds also reject NaN/INF without reading
       * unknown vertex formats.  Keep flat, finite near-screen sprites. */
      if(!(source[i].x>=-w*0.25f &&
           source[i].x<=w*1.25f &&
           source[i].y>=-h*0.25f &&
           source[i].y<=h*1.25f &&
           source[i].z>=0.0f && source[i].z<=1.0f &&
           source[i].rhw>0.0f && source[i].rhw<=256.0f &&
           source[i].z-z0>=-0.001f &&
           source[i].z-z0<=0.001f &&
           source[i].rhw-rhw0>=-0.1f &&
           source[i].rhw-rhw0<=0.1f))
          return 0;
      if(source[i].x<l)l=source[i].x;
      if(source[i].x>r)r=source[i].x;
      if(source[i].y<t)t=source[i].y;
      if(source[i].y>b)b=source[i].y;
    }
    if(!(r-l>=w*0.45f && b-t>=h*0.25f))return 0;
    scalar=(4.0f*h)/(3.0f*w);
    memcpy(result,source,4u*sizeof(*result));
    for(i=0u;i<4u;i++)
      result[i].x=0.5f*w+(source[i].x-0.5f*w)*scalar;
    return 1;
}
#endif
