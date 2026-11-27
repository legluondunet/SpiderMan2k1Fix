#ifndef SPIDER_SHADOW_RESOLVE_SCOPE_H
#define SPIDER_SHADOW_RESOLVE_SCOPE_H
#include <stdint.h>
/* Index and returned surface are different namespaces. A submit must
 * resolve at least once, and every observed conversion must agree. */
typedef struct SpiderShadowResolveScope {
  uint32_t index,surface;
  unsigned invalid;
} SpiderShadowResolveScope;
static inline void SpiderShadowResolve_Begin(SpiderShadowResolveScope *s,uint32_t index){
  s->index=index;s->surface=0;s->invalid=0;
}
static inline void SpiderShadowResolve_Observe(SpiderShadowResolveScope *s,uint32_t index,uint32_t surface){
  if(!s->index || index!=s->index || !surface || (s->surface && s->surface!=surface)){
    s->invalid=1;return;
  }
  s->surface=surface;
}
static inline int SpiderShadowResolve_Match(const SpiderShadowResolveScope *s,uint32_t material){
  return s->index && !s->invalid && s->surface && s->surface==material;
}
#endif
