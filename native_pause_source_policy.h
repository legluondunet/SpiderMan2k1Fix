#ifndef SPIDERMAN2K1FIX_NATIVE_PAUSE_SOURCE_POLICY_H
#define SPIDERMAN2K1FIX_NATIVE_PAUSE_SOURCE_POLICY_H

/* Pure geometry and material policy for the ORIGINAL 3D ESC/Pause overlay.
 *
 * The original game can report a 4:3 logical gameplay mode while presenting
 * to a wider physical DirectDraw backbuffer. Ordinary 3D UI is authored in
 * logical coordinates, but native shell/Pause art may instead use physical
 * screen coordinates. Detect the source space from the original fullscreen
 * Pause ribbon, never from a level name, a specific RHW value, a controller
 * model or one chosen screen resolution.
 *
 * These helpers do NOT modify game state. Callers MUST additionally verify
 * the SHA-pinned original executable, D3D7 triangle fan + FVF 0x144, the
 * original DrawPrimitive owner 0x00102C72, an untextured ribbon and a
 * fresh same-Flip observation before remapping any vertices. A source-space
 * mismatch fails closed to the preexisting renderer.
 */
#include <stdint.h>

enum {
  SPIDER_PAUSE_SOURCE_NONE=0,
  SPIDER_PAUSE_SOURCE_LOGICAL=1,
  SPIDER_PAUSE_SOURCE_PHYSICAL=2
};

static inline float SpiderPauseSource_Abs(float n) {
  return n<0.0f?-n:n;
}

static inline int SpiderPauseSource_RibbonBounds(
    float left,float right,float top,float bottom,
    unsigned width,unsigned height) {
  if(width<640u || height<480u ||
     width>16384u || height>16384u ||
     !((right-left)>(float)width*0.96f))
    return 0;
  return left>=-(float)width*0.016f &&
         left<=(float)width*0.011f &&
         right>=(float)width*0.987f &&
         right<=(float)width*1.02f &&
         top>=(float)height*0.30f &&
         top<=(float)height*0.52f &&
         bottom>=(float)height*0.55f &&
         bottom<(float)height*0.72f &&
         bottom-top>(float)height*0.11f;
}

/* Geometric source-space choice, not a guessed level/depth signature.
 * This remains valid for 16:9, 16:10, fractional-size letterboxes and
 * native 4:3 physical monitors. On ambiguity or unsupported mode, abstain. */
static inline unsigned SpiderPauseSource_Space(
    float left,float right,float top,float bottom,
    unsigned logical_width,unsigned logical_height,
    unsigned physical_width,unsigned physical_height) {
  int logical,physical;
  if(!logical_width || !logical_height || !physical_width ||
     !physical_height || logical_height!=physical_height ||
     SpiderPauseSource_Abs((float)logical_width*3.0f-
                           (float)logical_height*4.0f)>3.0f ||
     logical_width>physical_width)
    return SPIDER_PAUSE_SOURCE_NONE;
  logical=SpiderPauseSource_RibbonBounds(
    left,right,top,bottom,logical_width,logical_height);
  physical=SpiderPauseSource_RibbonBounds(
    left,right,top,bottom,physical_width,physical_height);
  if(logical==physical)return SPIDER_PAUSE_SOURCE_NONE;
  return logical?SPIDER_PAUSE_SOURCE_LOGICAL:
         SPIDER_PAUSE_SOURCE_PHYSICAL;
}

/* The full-width native ribbon uses the original .00122 depth plane.
 * Level-dependent perspective RHW values are deliberately NOT enumerated.
 * Colour can establish a source-owned purple band without requiring
 * hardcoded per-level RHW pairs; the host also checks original geometry
 * and the live untextured D3D7 material before trusting it. */
static inline int SpiderPauseSource_RibbonMaterial(
    float z,float rhw,uint32_t argb) {
  unsigned a=(argb>>24)&255u,red=(argb>>16)&255u;
  unsigned green=(argb>>8)&255u,blue=argb&255u;
  return SpiderPauseSource_Abs(z-0.00122f)<0.000050f &&
         rhw>0.5f && rhw<=128.0f &&
         a>=32u && a<=208u &&
         blue>red+20u && blue>green+20u;
}

/* Only known ORIGINAL ESC planes and original-source UI materials may be
 * remapped. No full-screen transforms, scene-specific data or arbitrary
 * HUD/3D elements. The actual native caller/texture verification is in C. */
static inline int SpiderPauseSource_Material(
    float z,float rhw,uint32_t argb,
    unsigned tex_width,unsigned tex_height,int texture_bound) {
  if(rhw<=0.5f || rhw>128.0f)return 0;
  if(!texture_bound) {
    if(SpiderPauseSource_RibbonMaterial(z,rhw,argb))return 1;
    return SpiderPauseSource_Abs(z-0.00146f)<0.000045f &&
           ((argb>>24)&255u)==128u;
  }
  if(SpiderPauseSource_Abs(z-0.00122f)>=0.00010f &&
     SpiderPauseSource_Abs(z-0.00098f)>=0.00010f &&
     SpiderPauseSource_Abs(z-0.00146f)>=0.000045f)
    return 0;
  return (tex_width==8u && tex_height==8u) ||
         (tex_height==32u &&
          (tex_width==16u || tex_width==32u || tex_width==64u)) ||
         (tex_width==64u && tex_height==64u);
}

static inline float SpiderPauseSource_MapPhysicalX(
    float x,unsigned physical_width,unsigned physical_height) {
  const float c=(float)physical_width*0.5f;
  return c+(x-c)*(4.0f*(float)physical_height/
                  (3.0f*(float)physical_width));
}

#endif
