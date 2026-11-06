#ifndef SPIDERMAN2K1FIX_UI_TRANSITION_GUTTERS_H
#define SPIDERMAN2K1FIX_UI_TRANSITION_GUTTERS_H

/* Experimental physical-gutter compositor for native 2D scene masks.
 *
 * A wipe/iris is not a HUD. Its source material is a screen-space 2D
 * polygon: the game already supplies black masks for the physical frame.
 * The 4:3 user-interface transform is useful for the INNER opening, but
 * contracts its outer masks into the centered safe area, revealing the
 * world in two unwanted screen gutters.
 *
 * Instead of stretching the iris or duplicating the whole scene:
 *  - keep the centered 4:3 mapping for the original inner silhouette;
 *  - clip the ORIGINAL native mask polygons to the two *physical* gutters;
 *  - submit ONLY those clipped polygon fragments, in the native material
 *    state, during the same game-owned Direct3D7 render pass.
 *
 * Clipping is generic Sutherland-Hodgman and preserves each polygon's
 * sloping contours. There are no scene/level IDs and no pre-baked shapes.
 * A strict observed render-family check controls the initial trial;
 * this is not a proven general-purpose UI/3D ownership classifier.
 * Disabled by default until explicit Wine and Windows visual validation.
 */
#include "menu_scale.h"
#include <stdint.h>
#include <string.h>

#define SPIDER_TRANSITION_MAX_VERTS 8u
#define SPIDER_TRANSITION_CALLER_RVA 0x00102C72u

static inline float SpiderTransition_Abs(float x)
{ return x<0.0f ? -x : x; }

/* Family observed during native expanding spider transition in the
 * original game: RGB black (actual vertex ARGB=0x00000000),
 * screen-projected, z≈0, RHW≈27.6.  The earlier unverified
 * expectation 0xFF000000 rejected EVERY real spider mask!
 *
 * The classifier accepts only uniform RGBA black, with alpha 0 or
 * 255. Alpha 0 is safe to draw as a solid black mask ONLY when the
 * native Direct3D7 alpha blending and alpha tests are disabled.
 * The live D3D render-state validation belongs to the caller.
 * This identifies a render family, never a level or a particular
 * spider silhouette.
 */
static inline int SpiderTransition_IsNativeBlackMask(
    const SpiderMenuVertex *v,unsigned count,unsigned primitive,
    uint32_t fvf,uint32_t caller_rva,unsigned w,unsigned h)
{
    unsigned i;
    uint32_t color;
    float minx,maxx,miny,maxy;
    if(!v || count!=4u || primitive!=6u || fvf!=0x144u ||
       caller_rva!=SPIDER_TRANSITION_CALLER_RVA ||
       w<640u || h<480u || w>16384u || h>16384u ||
       (uint64_t)w*3u <= (uint64_t)h*4u)return 0;
    color=v[0].diffuse;
    /* Two known, strictly uniform vertex encodings for black.
     * Do not treat translucent black/zero-alpha gradients or
     * dark sprites as native scene masks. */
    if(color!=0x00000000u && color!=0xFF000000u)return 0;
    minx=maxx=v[0].x;miny=maxy=v[0].y;
    for(i=0u;i<4u;i++){
        if(v[i].diffuse!=color ||
           SpiderTransition_Abs(v[i].z)>0.00004f ||
           SpiderTransition_Abs(v[i].rhw-27.6f)>0.08f ||
           !(v[i].x>=-(float)w*.05f && v[i].x<=(float)w*1.05f &&
             v[i].y>=-(float)h*.05f && v[i].y<=(float)h*1.05f))
            return 0;
        if(v[i].x<minx)minx=v[i].x;
        if(v[i].x>maxx)maxx=v[i].x;
        if(v[i].y<miny)miny=v[i].y;
        if(v[i].y>maxy)maxy=v[i].y;
    }
    return maxx-minx>0.5f && maxy-miny>0.5f;
}

static inline SpiderMenuVertex SpiderTransition_Interpolate(
    const SpiderMenuVertex *a,const SpiderMenuVertex *b,float t)
{
    SpiderMenuVertex out=*a;
    out.x=a->x+t*(b->x-a->x);
    out.y=a->y+t*(b->y-a->y);
    out.z=a->z+t*(b->z-a->z);
    out.rhw=a->rhw+t*(b->rhw-a->rhw);
    out.u=a->u+t*(b->u-a->u);
    out.v=a->v+t*(b->v-a->v);
    /* The gated material has uniform RGB black at every vertex.
     * Its original alpha is kept; no colour interpolation is needed. */
    return out;
}

/* One vertical half-plane, optionally retaining x >= edge. */
static inline unsigned SpiderTransition_ClipHalfPlane(
    const SpiderMenuVertex *input,unsigned n,
    SpiderMenuVertex *output,float edge,int greater_equal)
{
    unsigned i,k=0u;
    if(!input || !output || n<3u || n>SPIDER_TRANSITION_MAX_VERTS)
        return 0u;
    for(i=0u;i<n;i++){
        const SpiderMenuVertex *a=&input[(i+n-1u)%n],*b=&input[i];
        int ain=greater_equal ? a->x>=edge : a->x<=edge;
        int bin=greater_equal ? b->x>=edge : b->x<=edge;
        if(ain!=bin){
            float dx=b->x-a->x;
            float t;
            if(dx==0.0f || k>=SPIDER_TRANSITION_MAX_VERTS)return 0u;
            t=(edge-a->x)/dx;
            if(!(t>=0.0f && t<=1.0f))return 0u;
            output[k++]=SpiderTransition_Interpolate(a,b,t);
            output[k-1u].x=edge;
        }
        if(bin){
            if(k>=SPIDER_TRANSITION_MAX_VERTS)return 0u;
            output[k++]=*b;
        }
    }
    return k;
}

/* Clip the original quad's surface to an actual exterior strip.
 * side=0 -> [0,gutter], side=1 -> [screen_w-gutter,screen_w].
 * Callers submit the returned XYZRHW fan unaltered using native D3D7
 * state. The central canvas is deliberately excluded, so the spider's
 * already-correct 4:3 shape is never duplicated or horizontally scaled.
 */
static inline unsigned SpiderTransition_ClipToGutter(
    const SpiderMenuVertex *source,SpiderMenuVertex *out,
    unsigned side,unsigned screen_w,unsigned screen_h)
{
    SpiderMenuVertex first[SPIDER_TRANSITION_MAX_VERTS];
    float w=(float)screen_w,h=(float)screen_h,gutter,left,right;
    unsigned n;
    if(!source || !out || side>1u || screen_w<640u ||
       screen_h<480u || screen_w>16384u || screen_h>16384u ||
       (uint64_t)screen_w*3u <= (uint64_t)screen_h*4u)
        return 0u;
    gutter=(w-h*4.0f/3.0f)*.5f;
    if(!(gutter>0.0f && gutter<w*.5f))return 0u;
    left=side ? w-gutter : 0.0f;
    right=side ? w : gutter;
    n=SpiderTransition_ClipHalfPlane(source,4u,first,left,1);
    if(n<3u)return 0u;
    n=SpiderTransition_ClipHalfPlane(first,n,out,right,0);
    return n>=3u ? n : 0u;
}

/* Physical-gutter fill derived ONLY from the black mask's
 * intersection with the ORIGINAL screen edge (source x=0 or x=W).
 *
 * The center viewport maps original x=0 to x=gutter (and original
 * x=W to x=W-gutter). Replaying original source polygons clipped to
 * physical x=[0,gutter] is inconsistent with this mapping: near the
 * END of the expanding iris, source fragments can enter x=[0,gutter]
 * without touching original x=0, leaving isolated black triangles in
 * the physical margin even while the center's edge is already open.
 *
 * Instead, extend exactly the black vertical boundary intervals at
 * original x=0 or x=W across their corresponding outside strips.
 * A polygon not touching that screen edge returns 0 (not a dangling
 * native-source fragment). No spider silhouette edge is stretched:
 * only an existing screen-EDGE black band extends horizontally.
 *
 * The game-owned black polygons are convex TRIANGLEFAN quads. Reject
 * non-convex geometry to avoid filling disjoint vertical segments.
 * No level IDs, fixed transition duration or guessed fade parameters.
 */
static inline unsigned SpiderTransition_ExtendBlackEdge(
    const SpiderMenuVertex *source,SpiderMenuVertex *out,
    unsigned side,unsigned screen_w,unsigned screen_h)
{
    float xedge,w,h,gutter,miny=0.0f,maxy=0.0f;
    float previous=0.0f;
    unsigned i,count=0u;
    if(!source || !out || side>1u || screen_w<640u ||
       screen_h<480u || screen_w>16384u || screen_h>16384u ||
       (uint64_t)screen_w*3u <= (uint64_t)screen_h*4u)
        return 0u;
    w=(float)screen_w;h=(float)screen_h;
    gutter=(w-h*4.0f/3.0f)*0.5f;
    if(!(gutter>0.0f && gutter<w*0.5f))return 0u;
    xedge=side ? w : 0.0f;
    /* Reject concave or bow-tie quads before taking a min/max span.
     * The native mask consists of convex screen-space fans. */
    for(i=0u;i<4u;i++){
        const SpiderMenuVertex *a=&source[i],*b=&source[(i+1u)%4u],
                               *c=&source[(i+2u)%4u];
        float cross=(b->x-a->x)*(c->y-b->y) -
                    (b->y-a->y)*(c->x-b->x);
        if(cross>0.01f){
            if(previous<0.0f)return 0u;
            previous=1.0f;
        }else if(cross<-.01f){
            if(previous>0.0f)return 0u;
            previous=-1.0f;
        }
    }
    /* Accumulate exact edge vertices and edge-crossing intersections.
     * Any accepted candidate must intersect the physical screen EDGE;
     * isolated pixels in the source gutter are never replicated. */
    for(i=0u;i<4u;i++){
        const SpiderMenuVertex *a=&source[i],*b=&source[(i+1u)%4u];
        float da=a->x-xedge,db=b->x-xedge,y;
        int onedge=SpiderTransition_Abs(da)<=0.25f;
        int crossing=(da<-.25f && db>.25f) ||
                     (da>.25f && db<-.25f);
        if(!onedge && !crossing)continue;
        if(crossing){
            float dx=b->x-a->x,t;
            if(dx==0.0f)return 0u;
            t=(xedge-a->x)/dx;
            if(!(t>=0.0f && t<=1.0f))return 0u;
            y=a->y+t*(b->y-a->y);
        } else y=a->y;
        if(count==0u)miny=maxy=y;
        else {
            if(y<miny)miny=y;
            if(y>maxy)maxy=y;
        }
        ++count;
    }
    if(count<2u || maxy-miny<=0.5f || miny>=h || maxy<=0.0f)
        return 0u;
    if(miny<0.0f)miny=0.0f;
    if(maxy>h)maxy=h;
    if(maxy-miny<=0.5f)return 0u;
    /* Leave source material untouched (FVF, RGB/alpha, depth, RHW,
     * texcoords); only screen-space X/Y of this NEW gutter rectangle
     * differ. Shared boundary uses exactly the canvas x=gutter.
     * No widening into the center; no painted triangle disconnected
     * from the center mask.
     */
    for(i=0u;i<4u;i++)out[i]=source[0];
    out[0].x=side ? w-gutter : 0.0f;
    out[1].x=side ? w : gutter;
    out[2].x=out[1].x;
    out[3].x=out[0].x;
    out[0].y=out[1].y=miny;
    out[2].y=out[3].y=maxy;
    return 4u;
}

/* Fullscreen coherent iris layer (additional explicit experiment).
 *
 * In native physical coordinates, all known opaque/zero-alpha black
 * mask quads collectively describe the complete spider-shaped wipe.
 * The existing 4:3 correction maps (x,y) to:
 *    ((x-W/2)*k+W/2, y), k=4H/(3W)
 * which preserves its original aspect but leaves the physical gutters.
 *
 * A SINGLE similarity transform of that corrected shape can "cover"
 * the physical screen: zoom BOTH corrected axes by 1/k about the
 * center.  This simplifies exactly to:
 *    X = source.x
 *    Y = H/2 + (source.y-H/2) / k
 *
 * Crucially EVERY source mask vertex follows the SAME continuous map.
 * The original polygon adjacency/connectivity is retained; unlike
 * independent edge rectangles, no new detached black "spider legs"
 * are constructed. On 1920x1080, k=0.75: X is unchanged and Y zooms
 * 4/3 about the center. The top/bottom 180px are simply off-screen.
 *
 * This deliberately changes the visible field of the original 4:3
 * iris relative to the "contain" mode. It is an alternative visual
 * strategy, NOT yet Dreamcast-validated. HUD, native ESC and gameplay
 * keep their existing mapping; only the verified scene-iris quads are
 * eligible, after independent native D3D texture/alpha checks.
 */
static inline int SpiderTransition_MapFullscreenCover43(
    const SpiderMenuVertex *source,SpiderMenuVertex *mapped,
    unsigned screen_w,unsigned screen_h)
{
    float w=(float)screen_w,h=(float)screen_h;
    float zoom;
    unsigned i;
    if(!source || !mapped || screen_w<640u || screen_h<480u ||
       screen_w>16384u || screen_h>16384u ||
       (uint64_t)screen_w*3u <= (uint64_t)screen_h*4u)
        return 0;
    zoom=(3.0f*w)/(4.0f*h);
    if(!(zoom>1.0f && zoom<8.0f))return 0;
    memcpy(mapped,source,4u*sizeof(*mapped));
    for(i=0u;i<4u;i++){
        /* Preserve exact native source X: the width-filling result
         * of composing center-4:3 with uniform screen-cover zoom. */
        mapped[i].x=source[i].x;
        mapped[i].y=h*.5f+(source[i].y-h*.5f)*zoom;
    }
    return 1;
}

/* V8 optional topology repair for the COHERENT V7 fullscreen iris.
 *
 * In the 1920x1080 V7 video, the black "teeth" occasionally float
 * about 60-100 physical pixels away from the nearest exterior black
 * area near the END of the opening. The V7 similarity transform is
 * mathematically consistent, but cannot repair disconnected source
 * geometry. Preserve all existing tooth vertices and seal ONLY the
 * small gap from the outermost V7 tooth edge to the corresponding
 * physical screen border.
 *
 * This is not a whole-mask stretch. It creates a new black connector
 * quad, derived solely from existing flat vertical edges of verified
 * black geometry already inside the computed 4:3 exterior gutter.
 * It deliberately REFUSES an isolated triangle tip: with only one
 * extremal vertex the safe vertical connector interval is unknown.
 *
 * side=0: physical left strip from x=0 to the polygon's minimum x.
 * side=1: physical right strip from max x to the physical screen W.
 * Only a flat polygon edge within the corresponding physical gutter
 * may be extended. No original vertices or render states are changed.
 * Paint with the same game-owned native material/alpha guard as V7.
 * No level, scene, animation time, sprite, or boss identifiers.
 * Experimental and OFF unless separately opted in.
 */
static inline unsigned SpiderTransition_SealOuterToothEdge(
    const SpiderMenuVertex *mapped,SpiderMenuVertex *connector,
    unsigned side,unsigned screen_w,unsigned screen_h)
{
    float w=(float)screen_w,h=(float)screen_h,gutter,edge;
    float top=0.0f,bottom=0.0f;
    float minx,maxx;
    unsigned i,matching=0u;
    if(!mapped || !connector || side>1u ||
       screen_w<640u || screen_h<480u ||
       screen_w>16384u || screen_h>16384u ||
       (uint64_t)screen_w*3u <= (uint64_t)screen_h*4u)
        return 0u;
    gutter=(w-h*4.0f/3.0f)*.5f;
    if(!(gutter>1.0f && gutter<w*.5f))
        return 0u;
    minx=maxx=mapped[0].x;
    for(i=0u;i<4u;i++){
        if(!(mapped[i].x>=-w*.10f && mapped[i].x<=w*1.10f &&
             mapped[i].y>=-h*.60f && mapped[i].y<=h*1.60f &&
             SpiderTransition_Abs(mapped[i].z)<=0.00004f &&
             SpiderTransition_Abs(mapped[i].rhw-27.6f)<=0.08f) ||
           mapped[i].diffuse!=0x00000000u)
            return 0u;
        if(mapped[i].x<minx)minx=mapped[i].x;
        if(mapped[i].x>maxx)maxx=mapped[i].x;
    }
    if(maxx-minx<=1.0f)return 0u;
    edge=side ? maxx : minx;
    /* An exact 4:3 exterior is NOT an arbitrary distance threshold:
     * its geometry comes from the actual physical display dimensions.
     * Avoid creating connectors from mid-screen mask pieces. */
    if(side){
        if(!(edge<w-1.0f && edge>=w-gutter*.92f))
            return 0u;
    }else{
        if(!(edge>1.0f && edge<=gutter*.92f))
            return 0u;
    }
    /* Only connect a real native VERTICAL border edge (two vertices
     * sharing this X). Never synthesize broad rectangular fills from
     * a pointed triangular tooth; that would change its silhouette. */
    for(i=0u;i<4u;i++){
        if(SpiderTransition_Abs(mapped[i].x-edge)>.6f)continue;
        if(matching==0u)top=bottom=mapped[i].y;
        else{
            if(mapped[i].y<top)top=mapped[i].y;
            if(mapped[i].y>bottom)bottom=mapped[i].y;
        }
        matching++;
    }
    if(matching<2u || bottom-top<=1.0f || bottom<=0.0f || top>=h)
        return 0u;
    if(top<0.0f)top=0.0f;
    if(bottom>h)bottom=h;
    if(bottom-top<=1.0f)return 0u;
    /* A separate connector is drawn only in the physical OUTER
     * margin. Native black tooth positions, depth, RHW, UV, and
     * alpha remain intact. */
    for(i=0u;i<4u;i++)connector[i]=mapped[0];
    connector[0].x=side ? edge : 0.0f;
    connector[1].x=side ? w : edge;
    connector[2].x=connector[1].x;
    connector[3].x=connector[0].x;
    connector[0].y=connector[1].y=top;
    connector[2].y=connector[3].y=bottom;
    return 4u;
}

/* The previously approved 4:3 gameplay UI mapping is unmodified.
 * During the new explicit transition-only test, cancel only the old
 * physical-edge pinning of verified mask polygons. The actual shape
 * still receives the same centered 4:3 X mapping as before. */
static inline void SpiderTransition_MapInner43(
    const SpiderMenuVertex *src,SpiderMenuVertex *mapped,
    unsigned screen_w,unsigned screen_h)
{
    unsigned i;
    float center=(float)screen_w*.5f;
    float scale=(float)screen_h*4.0f/(3.0f*(float)screen_w);
    /* Complete out-of-place copy: preserve Y, Z, RHW, diffuse and UV
     * even when the output buffer was not initialized by a prior pass. */
    memcpy(mapped,src,4u*sizeof(*mapped));
    for(i=0u;i<4u;i++)
        mapped[i].x=center+(src[i].x-center)*scale;
}

#endif
