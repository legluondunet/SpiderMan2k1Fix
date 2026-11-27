#ifndef SPIDER_SHADOW_DRAW_PATHS_H
#define SPIDER_SHADOW_DRAW_PATHS_H
#include <stdint.h>
/* Both native flush sites use the same queue record vertex pointer, already
 * established in menubg provenance. Its 240 bytes hold a 16-byte header
 * and at most eight 28-byte vertices. No size-only texture classification. */
enum SpiderShadowDrawPath {
  SPIDER_SHADOW_PATH_OTHER=0,SPIDER_SHADOW_PATH_PRIMARY=1,
  SPIDER_SHADOW_PATH_COMPANION=2,SPIDER_SHADOW_PATH_INDEXED=3
};
static inline unsigned SpiderShadowDraw_Path(uint32_t rva,int indexed){
  if(indexed)return SPIDER_SHADOW_PATH_INDEXED;
  if(rva==0x00102c72u)return SPIDER_SHADOW_PATH_PRIMARY;
  if(rva==0x00103389u)return SPIDER_SHADOW_PATH_COMPANION;
  return SPIDER_SHADOW_PATH_OTHER;
}
static inline int SpiderShadowDraw_Eligible(unsigned path,unsigned type,uint32_t fvf,
    unsigned count,unsigned record_count,uint32_t material,uint32_t named){
  return (path==SPIDER_SHADOW_PATH_PRIMARY || path==SPIDER_SHADOW_PATH_COMPANION) &&
    type==6u && fvf==0x144u && count>=3u && count<=8u &&
    count==record_count && material!=0u && material==named;
}
#endif
