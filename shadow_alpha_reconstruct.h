#ifndef SPIDER_SHADOW_ALPHA_RECONSTRUCT_H
#define SPIDER_SHADOW_ALPHA_RECONSTRUCT_H
#include <stdint.h>
/* Pure offline-tested reconstruction. Not connected to the renderer yet.
 * Map original A4 alpha to A8, use monotone cubic interpolation, sample
 * texel centers at 4x resolution and never invent an alpha overshoot.
 * Original RGB and UV handling will be decided at the authenticated
 * conversion site; this routine intentionally handles alpha only. */
#define SPIDER_SHADOW_SRC_SIZE 64u
#define SPIDER_SHADOW_DST_SIZE 256u
static inline float SpiderShadow_Slope(float a,float b) {
  return a*b>0.0f ? 2.0f*a*b/(a+b) : 0.0f;
}
static inline float SpiderShadow_Cubic(float a,float b,float c,float d,float t) {
  float s=SpiderShadow_Slope(b-a,c-b);
  float q=SpiderShadow_Slope(c-b,d-c);
  float t2=t*t,t3=t2*t;
  float v=(2*t3-3*t2+1)*b+(t3-2*t2+t)*s+(-2*t3+3*t2)*c+(t3-t2)*q;
  float lo=b<c?b:c,hi=b<c?c:b;
  return v<lo?lo:(v>hi?hi:v);
}
static inline unsigned SpiderShadow_ClampIndex(int x) {
  return x<0?0u:(x>=64?63u:(unsigned)x);
}
static inline int SpiderShadow_Alpha4To8(const uint8_t *src,uint8_t *dst) {
  unsigned x,y;
  if(!src || !dst || src==dst)return 0;
  for(y=0;y<64u*64u;y++)if(src[y]>15u)return 0;
  for(y=0;y<256u;y++){
    /* (pixel + 0.5)/4 - 0.5 = (2*pixel - 3)/8. */
    int py=(int)y*2-3,iy=py>=0?py/8:(py-7)/8;
    float fy=(float)(py-iy*8)/8.0f;
    for(x=0;x<256u;x++){
      int px=(int)x*2-3,ix=px>=0?px/8:(px-7)/8;
      float fx=(float)(px-ix*8)/8.0f,rows[4],v;
      int k;
      for(k=0;k<4;k++){
        unsigned sy=SpiderShadow_ClampIndex(iy+k-1)*64u;
        rows[k]=SpiderShadow_Cubic(
          (float)src[sy+SpiderShadow_ClampIndex(ix-1)],
          (float)src[sy+SpiderShadow_ClampIndex(ix)],
          (float)src[sy+SpiderShadow_ClampIndex(ix+1)],
          (float)src[sy+SpiderShadow_ClampIndex(ix+2)],fx);
      }
      v=SpiderShadow_Cubic(rows[0],rows[1],rows[2],rows[3],fy)*17.0f;
      dst[y*256u+x]=(uint8_t)(v+0.5f);
    }
  }
  return 1;
}
#endif
