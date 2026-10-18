#include "fov_aspect.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

SpiderFOVSetting SpiderFOV_Parse(const char *value,double *degrees) {
  const char *p;
  char *end;
  double number;
  if (!value) return SPIDER_FOV_INVALID;
  p=value;
  while (*p==' ' || *p=='\t') ++p;
  if (strlen(p)>=4 && (p[0]=='A'||p[0]=='a') &&
      (p[1]=='U'||p[1]=='u') &&
      (p[2]=='T'||p[2]=='t') &&
      (p[3]=='O'||p[3]=='o')) {
    p+=4;
    while(*p==' '||*p=='\t')++p;
    return *p==0 ? SPIDER_FOV_AUTO : SPIDER_FOV_INVALID;
  }
  if(!isdigit((unsigned char)*p)) return SPIDER_FOV_INVALID;
  number=strtod(p,&end);
  if(end==p || !(number>=10.0 && number<=170.0))
    return SPIDER_FOV_INVALID;
  while(*end==' ' || *end=='\t') ++end;
  if(*end) return SPIDER_FOV_INVALID;
  if(degrees) *degrees=number;
  return SPIDER_FOV_DEGREES;
}

int SpiderFOV_AspectScalar(unsigned width,unsigned height,float *out) {
  double aspect,scale;
  if(!out || width<320 || height<240 || width>16384 || height>16384)
    return 0;
  aspect=(double)width/(double)height;
  if(aspect<4.0/3.0) return 0; /* No verified narrower-screen formula. */
  scale=(4.0/3.0)/aspect;
  if(scale<0.3 || scale>1.001) return 0;
  *out=(float)scale;
  return 1;
}

/* XYZ/RHW values are preprojected into output pixels by the game's CPU.
 * Only classify depth-bearing polys. Pure 2D HUD/BD/menus must not move.
 * Additional D3D ZENABLE gate lives in the Windows wrapper.
 */
int SpiderFOV_IsWorldQuad(const SpiderMenuVertex *v, unsigned n) {
  unsigned i;
  int all_overlay=1,all_zero_depth=1;
  if(!v || n<3 || n>64) return 0;
  for(i=0;i<n;i++) {
    if(!(v[i].x>-100000.f && v[i].x<100000.f &&
         v[i].y>-100000.f && v[i].y<100000.f &&
         v[i].z>=0.f && v[i].z<=1.001f &&
         v[i].rhw>0.f && v[i].rhw<100000.f)) return 0;
    if(!(v[i].z>=0.99999f && v[i].rhw>0.99999f && v[i].rhw<1.00001f))
      all_overlay=0;
    if(v[i].z>0.000001f) all_zero_depth=0;
  }
  return !all_overlay && !all_zero_depth;
}

int SpiderFOV_RestoreVertical(const SpiderMenuVertex *source,
                              SpiderMenuVertex *dest, unsigned n,
                              float scalar, float screen_mid_y) {
  unsigned i;
  float inverse;
  if(!source || !dest || source==dest ||
     !(scalar>=0.3f && scalar<=1.0f) ||
     !(screen_mid_y>=120.f && screen_mid_y<=8192.f) ||
     !SpiderFOV_IsWorldQuad(source,n)) return 0;
  inverse=1.f/scalar;
  for(i=0;i<n;i++) {
    dest[i]=source[i];
    dest[i].y=screen_mid_y+(source[i].y-screen_mid_y)*inverse;
  }
  return 1;
}