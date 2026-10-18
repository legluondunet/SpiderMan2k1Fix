#include "display_policy.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static int is_auto(const char *s) {
  if (!s || !s[0]) return 1;
  if(strlen(s)!=4u) return 0;
  return tolower((unsigned char)s[0])=='a' &&
         tolower((unsigned char)s[1])=='u' &&
         tolower((unsigned char)s[2])=='t' &&
         tolower((unsigned char)s[3])=='o';
}
int SpiderDisplayPolicy_Resolve(const char *setting,
    unsigned native_width, unsigned native_height,
    unsigned *width, unsigned *height) {
  char *end;
  unsigned long w,h;
  if(!width || !height) return 0;
  if(is_auto(setting)) {
    if(native_width<640 || native_height<480 ||
       native_width>16384 || native_height>16384) return 0;
    *width=native_width;*height=native_height;return 1;
  }
  if(!setting || !isdigit((unsigned char)*setting)) return 0;
  w=strtoul(setting,&end,10);
  if((*end!='x' && *end!='X') || !isdigit((unsigned char)end[1])) return 0;
  h=strtoul(end+1,&end,10);
  if(*end || w<640 || h<480 || w>16384 || h>16384) return 0;
  *width=(unsigned)w;*height=(unsigned)h;return 1;
}
int SpiderDisplayPolicy_ResolveAspect(const char *resolution,const char *aspect,
    unsigned native_width,unsigned native_height,
    unsigned *width,unsigned *height) {
  unsigned w,h;
  if(!width || !height ||
     !SpiderDisplayPolicy_Resolve(resolution,native_width,native_height,&w,&h))return 0;
  if(!aspect || is_auto(aspect)) { *width=w;*height=h;return 1; }
  if(strcmp(aspect,"4:3")!=0)return 0;
  if((unsigned long long)w*3ull>(unsigned long long)h*4ull) {
    h=(h/3u)*3u;
    w=(h/3u)*4u;
  } else if((unsigned long long)w*3ull<(unsigned long long)h*4ull) {
    w=(w/4u)*4u;
    h=(w/4u)*3u;
  }
  if(w<640u || h<480u)return 0;
  *width=w;*height=h;return 1;
}
int SpiderDisplayPolicy_SelectAvailable43(unsigned desired_width,unsigned desired_height,
    unsigned native_width,unsigned native_height,
    const SpiderDisplayMode *modes,size_t count,unsigned *width,unsigned *height) {
  size_t i;
  unsigned best_w=0,best_h=0;
  unsigned long long best_area=0;
  if(!width || !height || (!modes && count))return 0;
  for(i=0;i<count;i++) {
    unsigned w=modes[i].width,h=modes[i].height;
    unsigned long long area=(unsigned long long)w*h;
    if(modes[i].depth!=32 || w<800 || h<600 ||
       w>desired_width || h>desired_height ||
       w>native_width || h>native_height ||
       (unsigned long long)w*3ull!=(unsigned long long)h*4ull)continue;
    if(area>best_area) {best_area=area;best_w=w;best_h=h;}
  }
  if(!best_area)return 0;
  *width=best_w;*height=best_h;return 1;
}

int SpiderDisplayPolicy_ResolveRuntime(const char *resolution,const char *aspect,
    unsigned native_width,unsigned native_height,
    unsigned *width,unsigned *height) {
  /* Logical framebuffer size only. In 4:3 the physical display always uses
   * the unmodified [Display] Resolution. No display-mode enumeration here:
   * 1440x1080 is a valid internal canvas on a 1920x1080 monitor, regardless
   * of whether a 1440x1080 physical video mode is advertised by Wine. */
  return SpiderDisplayPolicy_ResolveAspect(resolution,aspect,native_width,native_height,width,height);
}

static void put_u32le(unsigned char *out,unsigned val) {
  out[0]=(unsigned char)val;out[1]=(unsigned char)(val>>8);
  out[2]=(unsigned char)(val>>16);out[3]=(unsigned char)(val>>24);
}
int SpiderDisplayPolicy_Enforce(unsigned char *data,size_t length,
    unsigned width,unsigned height) {
  if(!data || length<12 || width<640 || height<480 ||
     width>16384 || height>16384) return 0;
  put_u32le(data+0,width);put_u32le(data+4,height);
  put_u32le(data+8,32u);
  return 1;
}