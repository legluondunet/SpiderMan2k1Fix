#ifndef SPIDERMAN_UI_TRANSITION_EXACT_UNION_H
#define SPIDERMAN_UI_TRANSITION_EXACT_UNION_H
/* V14 experimental polygon-union compositor.
 * The real 92 source quads make one BLACK region with one CENTRAL opening.
 * The opening contains undercuts (V13 ray #1117 jumped by 37px); radial
 * reconstruction would erase the game's eight hooked limbs.
 *
 * This deterministic native-geometry vertical sweep merges all original
 * opaque polygons into ONE non-overlapping set of black trapezoids.
 * Every edge stays the exact original straight segment. The entire set is
 * transformed by ONE 4:3 X correction and ONE game-owned uniform zoom.
 * No texture rasterization, antialiasing claim, independent tooth patch,
 * per-level rule, or synthetic animation. Strict opt-in and V12 fallback.
 */
#include "ui_transition_atomic_vector.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define SPIDER_EXACT_MAX_QUADS SPIDER_ATOMIC_MASK_MAX
#define SPIDER_EXACT_MAX_EDGES (SPIDER_EXACT_MAX_QUADS*4u)
#define SPIDER_EXACT_MAX_EVENTS 2048u
#define SPIDER_EXACT_MAX_TRAPS 4096u

typedef struct SpiderExactEdge {float ax,ay,bx,by;} SpiderExactEdge;
typedef struct SpiderExactInterval {
    unsigned top,bottom;float lo,hi;
} SpiderExactInterval;
typedef struct SpiderExactTrap {
    float xl,xr,top_l,top_r,bottom_l,bottom_r;
} SpiderExactTrap;
typedef struct SpiderExactTemplate {
    unsigned traps,unique_quads,events,largest_intervals;
    float native_width,native_height;
    SpiderExactTrap trap[SPIDER_EXACT_MAX_TRAPS];
} SpiderExactTemplate;
typedef struct SpiderExactDiagnostic {
    unsigned reason,event,object;float a,b;
} SpiderExactDiagnostic;
/* reason: 1=invalid 2=event capacity 3=non-convex/invalid source
 * 4=source exterior incomplete 5=center opaque 6=mesh capacity. */
static inline int SpiderExact_Fail(SpiderExactDiagnostic *d,
    unsigned reason,unsigned event,unsigned object,float a,float b)
{
    if(d){d->reason=reason;d->event=event;d->object=object;d->a=a;d->b=b;}
    return 0;
}
static inline float SpiderExact_Cross(float ax,float ay,float bx,float by)
{
    return ax*by-ay*bx;
}
static inline int SpiderExact_EqualPoint(float ax,float ay,float bx,float by)
{
    return fabsf(ax-bx)<.005f && fabsf(ay-by)<.005f;
}
static inline int SpiderExact_EqualQuad(
    const SpiderAtomicMaskQuad *a,const SpiderAtomicMaskQuad *b)
{
    unsigned i,j;
    for(i=0u;i<4u;i++){
        int found=0;
        for(j=0u;j<4u;j++)if(SpiderExact_EqualPoint(
             a->x[i],a->y[i],b->x[j],b->y[j])){found=1;break;}
        if(!found)return 0;
    }
    return 1;
}
static inline int SpiderExact_AddEvent(float *events,unsigned *count,
                                       float x,float w)
{
    unsigned i;
    if(!(x>=0.0f && x<=w))return 1;
    for(i=0u;i<*count;i++)if(fabsf(events[i]-x)<.001f)return 1;
    if(*count>=SPIDER_EXACT_MAX_EVENTS)return 0;
    events[(*count)++]=x;
    return 1;
}
static int SpiderExact_CompareFloat(const void *a,const void *b)
{
    float x=*(const float*)a,y=*(const float*)b;
    return x<y?-1:(x>y?1:0);
}
static int SpiderExact_CompareInterval(const void *a,const void *b)
{
    const SpiderExactInterval *x=(const SpiderExactInterval*)a;
    const SpiderExactInterval *y=(const SpiderExactInterval*)b;
    return x->lo<y->lo?-1:(x->lo>y->lo?1:0);
}
static inline float SpiderExact_EdgeY(const SpiderExactEdge *e,float x)
{
    return e->ay+(e->by-e->ay)*(x-e->ax)/(e->bx-e->ax);
}
static inline int SpiderExact_PointInTriangle(float px,float py,
    float ax,float ay,float bx,float by,float cx,float cy)
{
    float a,b,c;
    if(fabsf(SpiderExact_Cross(bx-ax,by-ay,cx-ax,cy-ay))<.00001f)
        return 0;
    a=SpiderExact_Cross(bx-ax,by-ay,px-ax,py-ay);
    b=SpiderExact_Cross(cx-bx,cy-by,px-bx,py-by);
    c=SpiderExact_Cross(ax-cx,ay-cy,px-cx,py-cy);
    return (a>=-.0001f && b>=-.0001f && c>=-.0001f) ||
           (a<=.0001f && b<=.0001f && c<=.0001f);
}
static inline int SpiderExact_PointInQuad(const SpiderAtomicMaskQuad *q,
                                          float x,float y)
{
    return SpiderExact_PointInTriangle(x,y,q->x[0],q->y[0],
                 q->x[1],q->y[1],q->x[2],q->y[2]) ||
           SpiderExact_PointInTriangle(x,y,q->x[0],q->y[0],
                 q->x[2],q->y[2],q->x[3],q->y[3]);
}
static inline int SpiderExact_AppendTrap(SpiderExactTemplate *m,
    float x0,float x1,float t0,float t1,float b0,float b1)
{
    SpiderExactTrap *q;
    if(m->traps>=SPIDER_EXACT_MAX_TRAPS)return 0;
    if(x1-x0<.0001f)return 1;
    q=&m->trap[m->traps++];
    q->xl=x0;q->xr=x1;q->top_l=t0;q->top_r=t1;
    q->bottom_l=b0;q->bottom_r=b1;
    return 1;
}
/* For each interval between original vertex and edge-intersection Xs,
 * black polygon intersection with a vertical line has constant topology.
 * Merge its Y intervals and reconstruct their exact endpoint trapezoids.
 * Split at ALL segment intersections: never approximate hooked geometry. */
static inline int SpiderExact_Prepare(
    const SpiderAtomicMaskQuad *source,unsigned count,unsigned w,unsigned h,
    SpiderExactTemplate *out,SpiderExactDiagnostic *diag)
{
    SpiderAtomicMaskQuad *unique=NULL;
    SpiderExactEdge *edges=NULL;
    SpiderExactInterval *intervals=NULL;
    float *events=NULL;
    unsigned uq=0u,ne=0u,nx=0u,i,j,slab;
    int ok=0;
    SpiderExactTemplate tmp;
    if(diag)memset(diag,0,sizeof(*diag));
    if(!source || !out || count<8u || count>SPIDER_EXACT_MAX_QUADS ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u<(uint64_t)h*4u)
        return SpiderExact_Fail(diag,1u,0u,count,(float)w,(float)h);
    unique=(SpiderAtomicMaskQuad*)malloc(count*sizeof(*unique));
    edges=(SpiderExactEdge*)malloc(SPIDER_EXACT_MAX_EDGES*sizeof(*edges));
    intervals=(SpiderExactInterval*)malloc(count*sizeof(*intervals));
    events=(float*)malloc(SPIDER_EXACT_MAX_EVENTS*sizeof(*events));
    if(!unique || !edges || !intervals || !events){
        SpiderExact_Fail(diag,1u,0u,count,0.0f,0.0f);goto done;
    }
    memset(&tmp,0,sizeof(tmp));tmp.native_width=(float)w;
    tmp.native_height=(float)h;
    for(i=0u;i<count;i++){
        int duplicate=0;
        for(j=0u;j<uq;j++)if(SpiderExact_EqualQuad(source+i,unique+j)){
            duplicate=1;break;
        }
        if(duplicate)continue;
        unique[uq++]=source[i];
        for(j=0u;j<4u;j++){
            unsigned next=(j+1u)%4u;
            SpiderExactEdge *e;
            float ax=source[i].x[j],ay=source[i].y[j];
            float bx=source[i].x[next],by=source[i].y[next];
            if(!(ax>=-.01f && ax<=(float)w+.01f &&
                 bx>=-.01f && bx<=(float)w+.01f &&
                 ay>=-.01f && ay<=(float)h+.01f &&
                 by>=-.01f && by<=(float)h+.01f)){
                SpiderExact_Fail(diag,1u,0u,i,ax,ay);goto done;
            }
            if(!SpiderExact_AddEvent(events,&nx,ax,(float)w)){
                SpiderExact_Fail(diag,2u,0u,i,ax,0.0f);goto done;
            }
            if(SpiderExact_EqualPoint(ax,ay,bx,by))continue;
            if(ne>=SPIDER_EXACT_MAX_EDGES){
                SpiderExact_Fail(diag,1u,0u,i,(float)ne,0.0f);goto done;
            }
            e=&edges[ne++];e->ax=ax;e->ay=ay;e->bx=bx;e->by=by;
        }
    }
    if(!SpiderExact_AddEvent(events,&nx,0.0f,(float)w) ||
       !SpiderExact_AddEvent(events,&nx,(float)w,(float)w)){
        SpiderExact_Fail(diag,2u,0u,0u,0.0f,0.0f);goto done;
    }
    for(i=0u;i<ne;i++)for(j=i+1u;j<ne;j++){
        const SpiderExactEdge *a=edges+i,*b=edges+j;
        float adx=a->bx-a->ax,ady=a->by-a->ay;
        float bdx=b->bx-b->ax,bdy=b->by-b->ay;
        float den=SpiderExact_Cross(adx,ady,bdx,bdy),ta,tb,px;
        if(fabsf(den)<.00001f)continue;
        ta=SpiderExact_Cross(b->ax-a->ax,b->ay-a->ay,bdx,bdy)/den;
        tb=SpiderExact_Cross(b->ax-a->ax,b->ay-a->ay,adx,ady)/den;
        if(!(ta>.00001f && ta<.99999f && tb>.00001f && tb<.99999f))
            continue;
        px=a->ax+ta*adx;
        if(!SpiderExact_AddEvent(events,&nx,px,(float)w)){
            SpiderExact_Fail(diag,2u,0u,0u,px,0.0f);goto done;
        }
    }
    qsort(events,nx,sizeof(*events),SpiderExact_CompareFloat);
    if(uq<8u || ne<20u || nx<3u){
        SpiderExact_Fail(diag,1u,0u,uq,(float)ne,(float)nx);goto done;
    }
    for(i=0u;i<uq;i++)if(SpiderExact_PointInQuad(unique+i,
                      (float)w*.5f,(float)h*.5f)){
        SpiderExact_Fail(diag,5u,0u,i,(float)w*.5f,(float)h*.5f);
        goto done;
    }
    for(slab=0u;slab+1u<nx;slab++){
        float xl=events[slab],xr=events[slab+1u],xm=(xl+xr)*.5f;
        unsigned ni=0u,merged=0u;
        SpiderExactInterval current;
        if(xr-xl<.0001f)continue;
        for(i=0u;i<uq;i++){
            unsigned hits[4],nh=0u;
            float ys[4];
            const SpiderAtomicMaskQuad *q=unique+i;
            for(j=0u;j<4u;j++){
                unsigned next=(j+1u)%4u;
                float ax=q->x[j],bx=q->x[next];
                if(!((ax<=xm && bx>xm)||(bx<=xm && ax>xm)))continue;
                ys[nh]=q->y[j]+(q->y[next]-q->y[j])*(xm-ax)/(bx-ax);
                hits[nh]=j;nh++;
            }
            if(!nh)continue;
            if(nh!=2u){
                SpiderExact_Fail(diag,3u,slab,i,(float)nh,xm);goto done;
            }
            intervals[ni].lo=fminf(ys[0],ys[1]);
            intervals[ni].hi=fmaxf(ys[0],ys[1]);
            intervals[ni].top=(ys[0]<ys[1]?hits[0]:hits[1])+4u*i;
            intervals[ni].bottom=(ys[0]<ys[1]?hits[1]:hits[0])+4u*i;
            ni++;
        }
        if(ni==0u){
            SpiderExact_Fail(diag,4u,slab,0u,xm,0.0f);goto done;
        }
        qsort(intervals,ni,sizeof(*intervals),SpiderExact_CompareInterval);
        if(ni>tmp.largest_intervals)tmp.largest_intervals=ni;
        current=intervals[0];
        for(i=1u;i<=ni;i++){
            if(i<ni && intervals[i].lo<=current.hi+.002f){
                if(intervals[i].lo<current.lo){
                    current.top=intervals[i].top;current.lo=intervals[i].lo;
                }
                if(intervals[i].hi>current.hi){
                    current.bottom=intervals[i].bottom;current.hi=intervals[i].hi;
                }
            }else{
                SpiderExactEdge top,bottom;
                unsigned qtop=current.top/4u,ktop=current.top%4u;
                unsigned qbot=current.bottom/4u,kbot=current.bottom%4u;
                unsigned ntop=(ktop+1u)%4u,nbot=(kbot+1u)%4u;
                if(qtop>=uq || qbot>=uq){
                    SpiderExact_Fail(diag,3u,slab,i,(float)qtop,(float)qbot);
                    goto done;
                }
                top.ax=unique[qtop].x[ktop];top.ay=unique[qtop].y[ktop];
                top.bx=unique[qtop].x[ntop];top.by=unique[qtop].y[ntop];
                bottom.ax=unique[qbot].x[kbot];bottom.ay=unique[qbot].y[kbot];
                bottom.bx=unique[qbot].x[nbot];bottom.by=unique[qbot].y[nbot];
                if(fabsf(top.ax-top.bx)<.00001f ||
                   fabsf(bottom.ax-bottom.bx)<.00001f){
                    SpiderExact_Fail(diag,3u,slab,i,top.ax,bottom.ax);
                    goto done;
                }
                if(!SpiderExact_AppendTrap(&tmp,xl,xr,
                         SpiderExact_EdgeY(&top,xl),SpiderExact_EdgeY(&top,xr),
                         SpiderExact_EdgeY(&bottom,xl),SpiderExact_EdgeY(&bottom,xr))){
                    SpiderExact_Fail(diag,6u,slab,i,(float)tmp.traps,0.0f);
                    goto done;
                }
                merged++;
                if(i<ni)current=intervals[i];
            }
        }
        /* Source black must reach BOTH original screen bounds in each
         * slab. Incomplete or unrelated source families are rejected. */
        if(!merged || tmp.trap[tmp.traps-merged].top_l>.03f ||
           tmp.trap[tmp.traps-merged].top_r>.03f ||
           tmp.trap[tmp.traps-1u].bottom_l<(float)h-.03f ||
           tmp.trap[tmp.traps-1u].bottom_r<(float)h-.03f){
            SpiderExact_Fail(diag,4u,slab,merged,xm,(float)h);goto done;
        }
    }
    tmp.unique_quads=uq;tmp.events=nx;
    if(tmp.traps<4u){
        SpiderExact_Fail(diag,1u,0u,uq,(float)tmp.traps,0.0f);goto done;
    }
    *out=tmp;ok=1;
done:
    free(events);free(intervals);free(edges);free(unique);
    return ok;
}
/* The original matte is black outside its finite source frame. Its
 * four mathematical exterior strips share the same affine map, rather
 * than extending any tooth separately. Draw exactly ONE triangle list. */
static inline int SpiderExact_Build(const SpiderExactTemplate *m,
    unsigned w,unsigned h,float scale,
    SpiderAtomicVectorPoint *points,unsigned capacity,unsigned *count)
{
    unsigned n=0u,i;
    float cx,cy,aspect,l,r,t,b;
    if(count)*count=0u;
    if(!m || !points || !count || !m->traps ||
       capacity<(m->traps+4u)*6u ||
       !(scale>=.015f && scale<=24.0f) ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u<(uint64_t)h*4u ||
       m->native_width!=(float)w || m->native_height!=(float)h)
        return 0;
    cx=(float)w*.5f;cy=(float)h*.5f;
    aspect=(float)h*4.0f/((float)w*3.0f);
    l=cx-cx*aspect*scale;r=cx+cx*aspect*scale;
    t=cy-cy*scale;b=cy+cy*scale;
    if(l>0.0f && !SpiderAtomicVector_AppendRectangle(points,capacity,&n,
          0.0f,0.0f,fminf(l,(float)w),(float)h))return 0;
    if(r<(float)w && !SpiderAtomicVector_AppendRectangle(points,capacity,&n,
          fmaxf(r,0.0f),0.0f,(float)w,(float)h))return 0;
    if(t>0.0f && !SpiderAtomicVector_AppendRectangle(points,capacity,&n,
          fmaxf(l,0.0f),0.0f,fminf(r,(float)w),fminf(t,(float)h)))return 0;
    if(b<(float)h && !SpiderAtomicVector_AppendRectangle(points,capacity,&n,
          fmaxf(l,0.0f),fmaxf(b,0.0f),fminf(r,(float)w),(float)h))return 0;
    for(i=0u;i<m->traps;i++){
        SpiderExactTrap q=m->trap[i];
        SpiderAtomicVectorPoint a,p1,p2,d;
        a.x=cx+(q.xl-cx)*aspect*scale;
        a.y=cy+(q.top_l-cy)*scale;
        p1.x=cx+(q.xr-cx)*aspect*scale;
        p1.y=cy+(q.top_r-cy)*scale;
        p2.x=p1.x;p2.y=cy+(q.bottom_r-cy)*scale;
        d.x=a.x;d.y=cy+(q.bottom_l-cy)*scale;
        if(!SpiderAtomicVector_AppendTriangle(points,capacity,&n,a,p1,p2) ||
           !SpiderAtomicVector_AppendTriangle(points,capacity,&n,a,p2,d))
            return 0;
    }
    *count=n;
    return n>0u;
}
#endif
