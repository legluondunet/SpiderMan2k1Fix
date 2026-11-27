#ifndef SPIDER_WIDE_CULLING_H
#define SPIDER_WIDE_CULLING_H
#include <stdint.h>
#include <math.h>

/* Original PC fixed-point camera-space horizontal normals are mirrored:
 * (+x,0,+z), (-x,0,+z). Inside means n dot p >= 0. For a projection
 * x multiplier s, retain the original length and change |x|/z by s.
 * No global GTE scale, distance, vertical, near or far plane change.
 * Fail closed on unexpected layout/sign/length/scale; output untouched. */
static inline int SpiderCull_WidenHorizontalPair(
    const int16_t positive[3],const int16_t negative[3],
    float scalar,int negative_side,int16_t out[3]) {
  double x,z,length2,scaled,length_factor;
  int xx,zz;
  if(!positive||!negative||!out||(negative_side!=0&&negative_side!=1)||
     !(scalar>=0.3f && scalar<1.0f))return 0;
  if(positive[0]<=0||positive[1]!=0||positive[2]<=0||
     negative[0]!=-(int)positive[0]||negative[1]!=0||
     negative[2]!=positive[2])return 0;
  x=positive[0];z=positive[2];length2=x*x+z*z;
  if(length2<3800.0*3800.0||length2>4400.0*4400.0)return 0;
  scaled=x*(double)scalar;
  length_factor=sqrt(length2/(scaled*scaled+z*z));
  xx=(int)(scaled*length_factor+0.5);
  zz=(int)(z*length_factor+0.5);
  if(xx<=0||zz<=0||xx>4400||zz>4400||
     (double)xx*z>x*(double)zz)return 0;
  out[0]=(int16_t)(negative_side?-xx:xx);
  out[1]=0;out[2]=(int16_t)zz;
  return 1;
}
#endif
