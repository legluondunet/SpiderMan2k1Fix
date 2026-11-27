#ifndef SPIDER_SHADOW_ALPHA_PROCEDURAL_H
#define SPIDER_SHADOW_ALPHA_PROCEDURAL_H
#include <stdint.h>
/* Fit a smooth elliptical radial mask to the native A4 alpha support.
 * All coordinates are native texel coordinates; the source is immutable.
 * The family consists of monotonic smoothstep powers, with zero slope at
 * the center and boundary. Interpolate adjacent profiles to retain mass.
 * A non-radial, clipped or poorly fitted mask must keep the cubic path. */
typedef struct SpiderShadowFit {
  unsigned reason,peak,source_sum,output_sum,error_permille;
  unsigned left,top,right,bottom,profile,mix_permille;
} SpiderShadowFit;
static inline double SpiderShadow_Profile(double q,unsigned k){
  double t,s,p=1.0;
  unsigned i,n;
  if(q>=1.0)return 0.0;
  if(q<=0.0)return 1.0;
  t=1.0-q;s=t*t*(3.0-2.0*t);
  n=k<=4u?5u-k:k-3u;
  for(i=0;i<n;i++)p*=k<=4u?s:1.0-s;
  return k<=4u?p:1.0-p;
}
static inline double SpiderShadow_Radius2(double x,double y,double cx,double cy,double rx,double ry){
  double dx=(x-cx)/rx,dy=(y-cy)/ry;
  return dx*dx+dy*dy;
}
static inline int SpiderShadow_Procedural(const uint8_t *src,uint8_t *dst,SpiderShadowFit *f){
  unsigned x,y,k,lo=64u,top=64u,hi=0u,bottom=0u,peak=0u,sum=0u;
  unsigned asym=0u,error=0u,outsum=0u;
  double cx,cy,rx,ry,mass[9]={0},target,mix,delta;
  SpiderShadowFit init={0};
  if(!f)return 0;
  *f=init;
  if(!src || !dst || src==dst){f->reason=1;return 0;}
  for(y=0;y<64u;y++)for(x=0;x<64u;x++){
    unsigned a=src[y*64u+x];
    if(a>15u){f->reason=1;return 0;}
    sum+=a;
    if(a>peak)peak=a;
    if(a){
      if(x<lo)lo=x;
      if(x>hi)hi=x;
      if(y<top)top=y;
      if(y>bottom)bottom=y;
    }
  }
  f->peak=peak;f->source_sum=sum;
  f->left=lo;f->right=hi;f->top=top;f->bottom=bottom;
  if(!sum || peak<3u || lo==0u || top==0u || hi>=63u || bottom>=63u ||
     hi-lo<8u || bottom-top<8u){f->reason=2;return 0;}
  /* Reject asymmetric masks rather than impose an unrelated blob. */
  for(y=top;y<=bottom;y++)for(x=lo;x<=hi;x++){
    unsigned a=src[y*64u+x],b=src[y*64u+lo+hi-x],c=src[(top+bottom-y)*64u+x];
    asym+=(a>b?a-b:b-a)+(a>c?a-c:c-a);
  }
  if((uint64_t)asym*100u>(uint64_t)sum*25u){f->reason=3;return 0;}
  cx=((double)lo+hi+1.0)*0.5;cy=((double)top+bottom+1.0)*0.5;
  rx=((double)hi-lo+1.0)*0.5;ry=((double)bottom-top+1.0)*0.5;
  /* Discrete 256x256 mass, not an assumed continuous integral. */
  for(y=0;y<256u;y++)for(x=0;x<256u;x++){
    double q=SpiderShadow_Radius2((x+0.5)*0.25,(y+0.5)*0.25,cx,cy,rx,ry);
    if(q<1.0)for(k=0;k<9u;k++)mass[k]+=SpiderShadow_Profile(q,k);
  }
  target=(double)sum*16.0/(double)peak;
  if(target<mass[0] || target>mass[8]){f->reason=4;return 0;}
  for(k=0;k<7u && target>mass[k+1];k++){}
  delta=mass[k+1]-mass[k];
  if(delta<=0.0){f->reason=4;return 0;}
  mix=(target-mass[k])/delta;
  f->profile=k;f->mix_permille=(unsigned)(mix*1000.0+0.5);
  for(y=0;y<256u;y++)for(x=0;x<256u;x++){
    double q=SpiderShadow_Radius2((x+0.5)*0.25,(y+0.5)*0.25,cx,cy,rx,ry);
    double a=(1.0-mix)*SpiderShadow_Profile(q,k)+mix*SpiderShadow_Profile(q,k+1);
    unsigned value=(unsigned)(a*(double)(peak*17u)+0.5);
    dst[y*256u+x]=(uint8_t)value;outsum+=value;
  }
  f->output_sum=outsum;
  /* Preserve integrated alpha within 3%, allowing output quantization. */
  {
    unsigned expected=sum*16u*17u;
    unsigned diff=outsum>expected?outsum-expected:expected-outsum;
    if((uint64_t)diff*100u>(uint64_t)expected*3u){f->reason=5;return 0;}
  }
  /* Compare the reconstructed shape at the original pixel scale. A
   * smooth fitted mask may differ from quantized steps, not silhouette. */
  for(y=0;y<64u;y++)for(x=0;x<64u;x++){
    unsigned i,j,block=0u,original=(unsigned)src[y*64u+x]*272u;
    for(j=0;j<4u;j++)for(i=0;i<4u;i++)block+=dst[(y*4u+j)*256u+x*4u+i];
    error+=block>original?block-original:original-block;
  }
  f->error_permille=(unsigned)((uint64_t)error*1000u/((uint64_t)sum*272u));
  if(f->error_permille>300u){f->reason=6;return 0;}
  return 1;
}
#endif
