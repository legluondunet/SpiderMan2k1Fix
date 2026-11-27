#ifndef SPIDER_ESCAPE_PAUSE_EDGE_H
#define SPIDER_ESCAPE_PAUSE_EDGE_H
#include "escape_batch_trace.h"
/* One observed native down, at most one subsequent completed poll and
 * one Flip boundary. Never change the native DI latch or synthesize input. */
typedef struct SpiderEscapePauseEdge {
  unsigned pending,poll;
  long flip;
} SpiderEscapePauseEdge;
static inline void SpiderEscapePauseEdge_Batch(SpiderEscapePauseEdge *p,
    SpiderEscapeBatch edges,unsigned poll,long flip,unsigned latch) {
  if(!p)return;
  /* Any up disarms the trial, including ambiguous same-batch transitions. */
  p->pending=0;
  if(edges.down && !(edges.down&(edges.down-1u)) && !edges.up && latch==0xffu){
    p->pending=1;p->poll=poll;p->flip=flip;
  }
}
/* Native close already consumed the physical edge; never replay it on
 * the next gameplay tick. Return whether there was a pending token. */
static inline int SpiderEscapePauseEdge_AckClose(SpiderEscapePauseEdge *p) {
  if(!p || !p->pending)return 0;
  p->pending=0;
  return 1;
}
static inline int SpiderEscapePauseEdge_Take(SpiderEscapePauseEdge *p,
    unsigned poll,long flip,unsigned raw,int eligible) {
  int result;
  if(!p || !p->pending)return 0;
  result=eligible && poll-p->poll==1u && flip>=p->flip &&
         (unsigned long)(flip-p->flip)<=1u && raw==0x7fu;
  /* A normal native FF read consumes our token as well, so it cannot
   * be replayed as a second Pause request. Any other scoped read disarms. */
  p->pending=0;
  return result;
}
#endif
