#ifndef SPIDERMAN2K1FIX_UI_COMPOSITOR_POLICY_H
#define SPIDERMAN2K1FIX_UI_COMPOSITOR_POLICY_H

/* Renderer-owned semantic layers, not names, textures or level IDs.
 * An ESC primitive has an owner only after its original native renderer
 * draw path has been verified. Everything else continues as the game drew
 * it. First trial records menu primitives and replays them at EndScene.
 */
enum SpiderUICompLayer {
    SPIDER_UI_COMP_WORLD = 0,
    SPIDER_UI_COMP_HUD = 1,
    SPIDER_UI_COMP_SCREEN_TINT = 2,
    SPIDER_UI_COMP_PAUSE_DECOR = 3,
    SPIDER_UI_COMP_PAUSE_MENU = 4
};

/* Explicit native Pause painter order. The physical outer gutters
 * have ALREADY been filled by ui_comp_draw_native_exterior_gutters()
 * before these passes. Both original backdrop and inner spider tiles
 * MUST remain together in their original capture order: separating or
 * skipping source colours corrupted the centre's alpha blending in
 * hardware comparisons. No level-specific palette or RHW matching.
 */
enum SpiderUICompReplayPass {
    SPIDER_UI_COMP_REPLAY_HUD = 0,
    SPIDER_UI_COMP_REPLAY_MOTIF = 1,
    SPIDER_UI_COMP_REPLAY_MENU = 2,
    SPIDER_UI_COMP_REPLAY_SKIP = -1
};

static inline int SpiderUIComp_ReplayPassForDraw(int layer)
{
    if(layer==SPIDER_UI_COMP_PAUSE_MENU)
        return SPIDER_UI_COMP_REPLAY_MENU;
    if(layer==SPIDER_UI_COMP_HUD)
        return SPIDER_UI_COMP_REPLAY_HUD;
    if(layer==SPIDER_UI_COMP_PAUSE_DECOR)
        return SPIDER_UI_COMP_REPLAY_MOTIF;
    return SPIDER_UI_COMP_REPLAY_SKIP;
}

/* Full native Pause: up to 229 menu/decor primitives plus verified
 * HUD sprites, captured only while the native ribbon is live.
 * Keep a hard bound so a malformed frame fails closed. */
#define SPIDER_UI_COMP_MAX_COMMANDS 384u

static inline int SpiderUIComp_ShouldDefer(int trial_enabled,
        int scene_open, int scene_bailout, int native_layer,
        unsigned vertex_count, unsigned fvf)
{
    return trial_enabled && scene_open && !scene_bailout &&
           (native_layer == SPIDER_UI_COMP_PAUSE_DECOR ||
            native_layer == SPIDER_UI_COMP_PAUSE_MENU ||
            native_layer == SPIDER_UI_COMP_HUD) &&
           vertex_count == 4u && fvf == 0x144u;
}

/* Fail-closed proof that a set of game-owned native exterior
 * rectangles covers a physical screen edge all the way from top to
 * bottom. This is screen-space geometry, independent of boss/level,
 * source RGB, screen aspect, and the native spider's shape.
 *
 * The intervals are in the original physical display coordinates,
 * NEVER the corrected 4:3 canvas coordinates. A colour/texture mismatch
 * must be rejected by the caller before testing these spans.
 */
typedef struct SpiderUICompYSpan {
    float top,bottom;
} SpiderUICompYSpan;

static inline int SpiderUIComp_CoversFullHeight(
    const SpiderUICompYSpan *spans,unsigned count,float height)
{
    unsigned pass,i;
    float covered=0.0f;
    if(!spans || count==0u || count>SPIDER_UI_COMP_MAX_COMMANDS ||
       height<480.0f || height>16384.0f)return 0;
    for(pass=0u;pass<count;pass++) {
        float next=covered;
        for(i=0u;i<count;i++) {
            if(!(spans[i].top>=-height*.05f &&
                 spans[i].bottom<=height*1.05f &&
                 spans[i].top<spans[i].bottom))
                return 0;
            if(spans[i].top<=covered+0.5f &&
               spans[i].bottom>next)
                next=spans[i].bottom;
        }
        if(next<=covered+0.01f)return 0;
        covered=next;
        if(covered>=height-0.5f)return 1;
    }
    return 0;
}

/* Identify a source-backed, uniform native backdrop independently of
 * the inner spider, without any hardcoded colour, camera depth or level.
 *
 * Source evidence 2026-11-06: the ESC motif has two flat diffuse
 * colour families. The background one touches all four source edges,
 * and the second is strictly interior. This proof authorizes ONLY
 * two physical OUTER gutter quads. All 160 original source triangles
 * must remain untouched; replacing the source backdrop made the
 * centre's translucent colours incorrect (hardware A/B confirmed).
 *
 * AABB alone is not an authoritative proof of unique screen coverage:
 * the caller also verifies untextured, same-vertex-colour D3D7 triangles
 * and the saved original render-state owner on each source command.
 * Reject >2 colours, missing edges, zero opacity or invalid geometry.
 */
typedef struct SpiderUICompBackdropSample {
    float left,right,top,bottom;
    unsigned color;
} SpiderUICompBackdropSample;

static inline int SpiderUIComp_IdentifyNativeBackdrop(
    const SpiderUICompBackdropSample *samples,unsigned count,
    float width,float height,unsigned *background)
{
    SpiderUICompYSpan left[SPIDER_UI_COMP_MAX_COMMANDS];
    SpiderUICompYSpan right[SPIDER_UI_COMP_MAX_COMMANDS];
    SpiderUICompYSpan top[SPIDER_UI_COMP_MAX_COMMANDS];
    SpiderUICompYSpan bottom[SPIDER_UI_COMP_MAX_COMMANDS];
    unsigned nl=0u,nr=0u,nt=0u,nb=0u;
    unsigned candidate=0u,inner=0u,i;
    int have_candidate=0,have_inner=0;
    if(background)*background=0u;
    if(!samples || !background || count<4u ||
       count>SPIDER_UI_COMP_MAX_COMMANDS ||
       width<640.0f || height<480.0f ||
       width>16384.0f || height>16384.0f ||
       width*3.0f<=height*4.0f)return 0;
    for(i=0u;i<count;i++) {
        const SpiderUICompBackdropSample *q=&samples[i];
        int on_left,on_right,on_top,on_bottom,on_edge;
        if(!(q->left>=-width*.05f && q->right<=width*1.05f &&
             q->top>=-height*.05f && q->bottom<=height*1.05f &&
             q->right>q->left+0.5f &&
             q->bottom>q->top+0.5f && (q->color>>24)!=0u))
            return 0;
        on_left=q->left<=width*.005f;
        on_right=q->right>=width*.995f;
        on_top=q->top<=height*.005f;
        on_bottom=q->bottom>=height*.995f;
        on_edge=on_left || on_right || on_top || on_bottom;
        if(on_edge) {
            if(!have_candidate){candidate=q->color;have_candidate=1;}
            else if(candidate!=q->color)return 0;
            if(on_left){
                left[nl].top=q->top;left[nl++].bottom=q->bottom;
            }
            if(on_right){
                right[nr].top=q->top;right[nr++].bottom=q->bottom;
            }
            if(on_top){
                top[nt].top=q->left;top[nt++].bottom=q->right;
            }
            if(on_bottom){
                bottom[nb].top=q->left;bottom[nb++].bottom=q->right;
            }
        }
    }
    if(!have_candidate || !nl || !nr || !nt || !nb ||
       !SpiderUIComp_CoversFullHeight(left,nl,height) ||
       !SpiderUIComp_CoversFullHeight(right,nr,height) ||
       !SpiderUIComp_CoversFullHeight(top,nt,width) ||
       !SpiderUIComp_CoversFullHeight(bottom,nb,width))
        return 0;
    for(i=0u;i<count;i++){
        unsigned color=samples[i].color;
        if(color!=candidate){
            if(!have_inner){inner=color;have_inner=1;}
            else if(color!=inner)return 0;
        }
    }
    if(!have_inner)return 0; /* one flat fill is not a spider */
    *background=candidate;
    return 1;
}


#endif
