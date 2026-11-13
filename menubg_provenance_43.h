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

/* Two original D3D7 queue paths were observed drawing the same SAVE/NAME
 * artwork: the source-wrapped record and an untagged companion at original
 * DrawPrimitive caller RVA 0x103389. Never match an arbitrary 512x512
 * texture: require the EXACT original queue material handle previously
 * authenticated by the named resource wrapper, a recent actual source draw,
 * live native save shell and the observed fullscreen D3D7 vertex geometry.
 * Material identity and native shell are checked by the caller as well. */
#define SPIDER_MENUBG43_COMPANION_CALLER 0x00103389u
static inline int SpiderMenubg43_IsCompanionBackground(
    const SpiderMenuVertex *v,unsigned count,unsigned type,uint32_t fvf,
    uint32_t caller,unsigned w,unsigned h,
    uint32_t material,uint32_t verified_material,
    uint32_t frame,uint32_t verified_frame)
{
    unsigned i;
    float l,r,t,b;
    if(!v || count!=4u || type!=6u || fvf!=0x144u ||
       caller!=SPIDER_MENUBG43_COMPANION_CALLER ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u<=(uint64_t)h*4u ||
       !material || material!=verified_material ||
       frame<verified_frame || frame-verified_frame>2u)
        return 0;
    l=r=v[0].x;t=b=v[0].y;
    for(i=0u;i<4u;i++){
        if(!(v[i].x>=-2.0f && v[i].x<=(float)w+2.0f &&
             v[i].y>=-2.0f && v[i].y<=(float)h+2.0f &&
             v[i].z>=0.995f && v[i].z<=1.0f &&
             v[i].rhw>0.010f && v[i].rhw<0.050f))
            return 0;
        if(v[i].x<l)l=v[i].x;
        if(v[i].x>r)r=v[i].x;
        if(v[i].y<t)t=v[i].y;
        if(v[i].y>b)b=v[i].y;
    }
    return l<=2.0f && t<=2.0f &&
           r>=(float)w-2.0f && b>=(float)h-2.0f;
}

/* Unlike the large background, title glyphs are small original queued
 * sprites. The log identified native title quads at caller 0x102C72,
 * Y=99..171, Z=0.99902/0.99927, RHW=0.0229, 32/64px glyph textures.
 * This applies only inside the separately authenticated save shell and
 * only alongside a recently mapped named menubg background.
 * Preserve original Y,Z,RHW,colour and UV; no global text shift. */
#define SPIDER_MENUBG43_TITLE_CALLER 0x00102C72u
static inline int SpiderMenubg43_TitleGlyphTransform(
    const SpiderMenuVertex *v,SpiderMenuVertex *out,
    unsigned count,unsigned type,uint32_t fvf,uint32_t caller,
    unsigned w,unsigned h,unsigned texw,unsigned texh)
{
    float l,r,t,b,scale;
    unsigned i;
    if(!v || !out || v==out || count!=4u || type!=6u ||
       fvf!=0x144u || caller!=SPIDER_MENUBG43_TITLE_CALLER ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u<=(uint64_t)h*4u ||
       texw<8u || texw>128u || texh<8u || texh>128u)
        return 0;
    l=r=v[0].x;t=b=v[0].y;
    for(i=0u;i<4u;i++){
       if(!(v[i].x>=-(float)w*0.06f && v[i].x<(float)w*0.75f &&
            v[i].y>=(float)h*0.05f && v[i].y<=(float)h*0.23f &&
            v[i].z>=0.998f && v[i].z<=1.0f &&
            v[i].rhw>0.020f && v[i].rhw<0.027f))
            return 0;
       if(v[i].x<l)l=v[i].x;
       if(v[i].x>r)r=v[i].x;
       if(v[i].y<t)t=v[i].y;
       if(v[i].y>b)b=v[i].y;
    }
    if(!(r-l>=10.0f && r-l<=150.0f &&
         b-t>=15.0f && b-t<=(float)h*0.11f))
        return 0;
    scale=4.0f*(float)h/(3.0f*(float)w);
    memcpy(out,v,4u*sizeof(*out));
    for(i=0u;i<4u;i++)
       out[i].x=(float)w*0.5f+(v[i].x-(float)w*0.5f)*scale;
    return 1;
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
