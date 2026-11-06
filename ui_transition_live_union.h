#ifndef SPIDERMAN_UI_TRANSITION_LIVE_UNION_H
#define SPIDERMAN_UI_TRANSITION_LIVE_UNION_H
/* V15 experimental LIVE source iris reconstruction.
 *
 * The native game submits its 0x00000000 D3D7 quads EVERY frame.
 * V10-V14 sample a stable set only once and animate a reference shape
 * by a scalar measured from the next frame. This module uses CURRENT
 * quads to reconstruct CURRENT coverage, with no frozen template/zoom.
 *
 * Exact sweep and 4:3 aspect code are reused from V14, which remains
 * unchanged in its separate opt-in. Fail closed for missing draws,
 * changing source families or unsupported native geometry. Source
 * coverage checks are independent of game timers and scene IDs.
 */
#include "ui_transition_exact_union.h"

typedef struct SpiderLiveUnionReport {
    unsigned reason;           /* 0=ready, 1=invalid, 2=incomplete, 3=union rejected */
    unsigned seen,expected,unique,traps;
    unsigned union_reason,union_event,union_object;
    float union_a,union_b;
    float max_vertex_deviation,rms_vertex_deviation;
} SpiderLiveUnionReport;

/* Diagnostic ONLY. Compare native quad i with the nearest reference
 * polygon at the estimated global scale, accommodating reversals and
 * reorderings. Differences measure whether native animation is truly
 * a one-scalar zoom; the result NEVER changes current geometry.
 */
static inline void SpiderLiveUnion_MeasureDeviation(
    const SpiderAtomicMaskQuad *current,unsigned count,
    const SpiderAtomicMaskQuad *reference,unsigned reference_count,
    unsigned w,unsigned h,float estimated_scale,
    SpiderLiveUnionReport *report)
{
    unsigned i,j,k,l;
    double sum_sq=0.0,max_dev=0.0;
    float cx=(float)w*.5f,cy=(float)h*.5f;
    if(!current || !reference || !report || !count ||
       !reference_count || reference_count>SPIDER_ATOMIC_MASK_MAX ||
       !(estimated_scale>0.0f))return;
    for(i=0u;i<count;i++){
        float best=1.0e30f;
        for(j=0u;j<reference_count;j++){
            /* All corners must match, not just the nearest single vertex;
             * the original engine may submit both windings or reorder fans. */
            float err=0.0f;
            for(k=0u;k<4u;k++){
                float dmin=1.0e30f;
                for(l=0u;l<4u;l++){
                    float dx=current[i].x[k]-
                             (cx+(reference[j].x[l]-cx)*estimated_scale);
                    float dy=current[i].y[k]-
                             (cy+(reference[j].y[l]-cy)*estimated_scale);
                    float d=dx*dx+dy*dy;
                    if(d<dmin)dmin=d;
                }
                err+=dmin;
            }
            if(err<best)best=err;
        }
        if(best<1.0e30f){
            double dev=sqrt((double)best*.25);
            sum_sq+=best;
            if(dev>max_dev)max_dev=dev;
        }
    }
    report->rms_vertex_deviation=(float)sqrt(sum_sq/(4.0*count));
    report->max_vertex_deviation=(float)max_dev;
}

static inline int SpiderLiveUnion_Prepare(
    const SpiderAtomicMaskQuad *live,unsigned live_count,
    unsigned expected_count,unsigned w,unsigned h,
    const SpiderAtomicMaskQuad *reference,unsigned reference_count,
    float estimated_scale,SpiderExactTemplate *out,
    SpiderLiveUnionReport *report)
{
    SpiderExactDiagnostic diagnostic;
    if(report)memset(report,0,sizeof(*report));
    if(report){report->seen=live_count;report->expected=expected_count;}
    if(!live || !out || !report || !reference || !reference_count ||
       reference_count>SPIDER_ATOMIC_MASK_MAX ||
       expected_count<32u || expected_count>SPIDER_ATOMIC_MASK_MAX ||
       w<640u || h<480u || !(estimated_scale>=.015f &&
                              estimated_scale<=24.0f)){
        if(report)report->reason=1u;
        return 0;
    }
    if(live_count!=expected_count || reference_count!=expected_count){
        report->reason=2u;
        return 0;
    }
    SpiderLiveUnion_MeasureDeviation(live,live_count,
         reference,reference_count,w,h,estimated_scale,report);
    if(!SpiderExact_Prepare(live,live_count,w,h,out,&diagnostic)){
        report->reason=3u;
        report->union_reason=diagnostic.reason;
        report->union_event=diagnostic.event;
        report->union_object=diagnostic.object;
        report->union_a=diagnostic.a;
        report->union_b=diagnostic.b;
        return 0;
    }
    report->reason=0u;
    report->unique=out->unique_quads;
    report->traps=out->traps;
    return 1;
}
#endif
