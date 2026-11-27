#ifndef SPIDER_ESCAPE_BATCH_TRACE_H
#define SPIDER_ESCAPE_BATCH_TRACE_H
#include <stdint.h>
/* DIDEVICEOBJECTDATA is five DWORDs in this exact PE32 native path.
 * Only DIK_ESCAPE (offset 1) is collected. Do not retain other key codes.
 * Bit positions are the original event indices in the 16-record batch. */
typedef struct SpiderEscapeBatch {
  uint32_t down,up;
} SpiderEscapeBatch;
static inline int SpiderEscapeBatch_Decode(const uint32_t *events,
                                            unsigned count,
                                            SpiderEscapeBatch *out) {
  unsigned i;
  SpiderEscapeBatch v={0,0};
  if(!out||!events||count>16u)return 0;
  for(i=0;i<count;i++){
    if(events[5u*i]!=1u)continue;
    if(events[5u*i+1u]&0x80u)v.down|=1u<<i;
    else v.up|=1u<<i;
  }
  *out=v;
  return (v.down|v.up)!=0u;
}
/* Highest ESC event index is the final native write. Return 1 only when
 * a down was observed and the final event in this batch was an up. */
static inline int SpiderEscapeBatch_DownOverwritten(SpiderEscapeBatch v) {
  uint32_t seen=v.down|v.up;
  if(!v.down||!v.up)return 0;
  while(seen&(seen-1u))seen&=seen-1u;
  return (seen&v.up)!=0u;
}
#endif
