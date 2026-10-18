#include "comic_native.h"
#include <string.h>
static float delta(float a,float b) { return a>b?a-b:b-a; }
int SpiderComicNative_IsCandidate(const SpiderMenuVertex *v,
    unsigned count, unsigned primitive, unsigned long fvf,
    unsigned long caller_rva, unsigned texture_w, unsigned texture_h,
    unsigned screen_w, unsigned screen_h) {
  unsigned i;
  float xmin=1.e7f,ymin=1.e7f,xmax=-1.e7f,ymax=-1.e7f;
  float umin=1.e7f,vmin=1.e7f,umax=-1.e7f,vmax=-1.e7f;
  /* Distinct from correct MENU/title callsite RVA 0x00103389. */
  if (!v || count!=4 || primitive!=6u || fvf!=0x144UL ||
      caller_rva!=0x00102C72UL || texture_w!=512u || texture_h!=256u ||
      screen_w<640u || screen_h<480u ||
      (unsigned long long)screen_w*3u<=(unsigned long long)screen_h*4u)
    return 0;
  for(i=0;i<count;i++) {
    const SpiderMenuVertex *p=&v[i];
    if (!(p->x>-4.f && p->x<(float)screen_w+4.f &&
          p->y>-4.f && p->y<(float)screen_h+4.f &&
          p->u>-.02f && p->u<1.02f && p->v>-.02f && p->v<.958f)) return 0;
    if(p->x<xmin) xmin=p->x;
    if(p->x>xmax) xmax=p->x;
    if(p->y<ymin) ymin=p->y;
    if(p->y>ymax) ymax=p->y;
    if(p->u<umin) umin=p->u;
    if(p->u>umax) umax=p->u;
    if(p->v<vmin) vmin=p->v;
    if(p->v>vmax) vmax=p->v;
  }
  return delta(xmin,0.f)<2.1f && delta(ymin,0.f)<2.1f &&
         delta(xmax,(float)screen_w)<2.1f &&
         delta(ymax,(float)screen_h)<2.1f &&
         delta(umin,0.f)<.015f && delta(umax,1.f)<.015f &&
         delta(vmin,0.f)<.015f && delta(vmax,240.f/256.f)<.015f;
}
int SpiderComicNative_Transform(const SpiderMenuVertex *src,
    SpiderMenuVertex *dst,unsigned count,unsigned screen_w,unsigned screen_h) {
  unsigned i;
  float target_w,left,factor;
  if(!src || !dst || src==dst || count!=4 || !screen_w || !screen_h ||
     (unsigned long long)screen_w*3u<=(unsigned long long)screen_h*4u)
    return 0;
  target_w=(float)screen_h*(4.f/3.f);
  if(target_w>=(float)screen_w) return 0;
  left=((float)screen_w-target_w)*.5f;
  factor=target_w/(float)screen_w;
  memcpy(dst,src,count*sizeof(*dst));
  for(i=0;i<count;i++) dst[i].x=left+src[i].x*factor;
  return 1;
}