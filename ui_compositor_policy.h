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

/* Direct3D7 EndScene replay order: scene/HUD has already drawn.
 * Keep the native translucent backdrop below its inner spider artwork,
 * and the entire Pause menu ABOVE that artwork. Within each semantic
 * pass, preserve native capture order and original materials.
 */
enum SpiderUICompReplayPass {
    SPIDER_UI_COMP_REPLAY_BACKDROP = 0,
    SPIDER_UI_COMP_REPLAY_HUD = 1,
    SPIDER_UI_COMP_REPLAY_MOTIF = 2,
    SPIDER_UI_COMP_REPLAY_MENU = 3,
    SPIDER_UI_COMP_REPLAY_SKIP = -1
};

/* Native material identity comes from four-edge coverage and the
 * detected inner silhouette, never a fixed RGB value or level number.
 * If that source classification is unproven, retain exact original
 * decoration order in the motif pass (fail closed).
 */
static inline int SpiderUIComp_ReplayPassForDraw(
    int layer,unsigned source_color,int backdrop_verified,
    int backdrop_drawn,unsigned backdrop_color)
{
    if(layer==SPIDER_UI_COMP_PAUSE_MENU)
        return SPIDER_UI_COMP_REPLAY_MENU;
    if(layer==SPIDER_UI_COMP_HUD)
        return SPIDER_UI_COMP_REPLAY_HUD;
    if(layer!=SPIDER_UI_COMP_PAUSE_DECOR)
        return SPIDER_UI_COMP_REPLAY_SKIP;
    if(!backdrop_verified)
        return SPIDER_UI_COMP_REPLAY_MOTIF;
    if(source_color==backdrop_color)
        return backdrop_drawn ? SPIDER_UI_COMP_REPLAY_SKIP :
                                SPIDER_UI_COMP_REPLAY_BACKDROP;
    return SPIDER_UI_COMP_REPLAY_MOTIF;
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

/* Entire overlay is the screen; decoration remains in 4:3. Keep the
 * source RGB and limit alpha during a reversible first hardware trial.
 * This is NOT an exact reproduction of the game's native pause tint.
 */
static inline unsigned SpiderUIComp_Tint(unsigned source_argb)
{
    unsigned alpha=(source_argb >> 24) & 0xffu;
    if (alpha>0x50u) alpha=0x50u;
    return (alpha<<24)|(source_argb & 0xffffffu);
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

/* On a physical widescreen, the 4:3 gameplay UI is centered. The
 * exterior strips are simply the two complements of that canvas.
 * This never scales, reprojects or extends the spider decoration.
 */
static inline float SpiderUIComp_GutterWidth(
    unsigned screen_w,unsigned screen_h)
{
    float outer;
    if(screen_w<640u || screen_h<480u ||
       screen_w>16384u || screen_h>16384u ||
       (unsigned long long)screen_w*3u <=
           (unsigned long long)screen_h*4u)return 0.0f;
    outer=((float)screen_w-(float)screen_h*4.0f/3.0f)*0.5f;
    return outer>0.0f ? outer : 0.0f;
}

/* A source quad attached to a physical edge is allowed to establish
 * exterior colour only if it covers the ENTIRE native 4:3 gutter width.
 * Touching the edge alone is insufficient: narrow tiles could otherwise
 * be erroneously expanded to the whole strip. This is a generic
 * source-geometry condition for every widescreen aspect ratio.
 *
 * right_side=0: [0,gutter], right_side=1: [width-gutter,width].
 * 0.5px tolerance avoids rejecting exact raster edge coordinates.
 */
static inline int SpiderUIComp_EdgeTileCoversGutter(
    float min_x,float max_x,float width,float gutter,int right_side)
{
    if(!(width>=640.0f && width<=16384.0f &&
         gutter>0.0f && gutter<width*0.5f &&
         min_x>=-width*0.05f && max_x<=width*1.05f &&
         max_x>min_x))
        return 0;
    if(right_side)
        return max_x>=width*0.995f &&
               min_x<=width-gutter+0.5f;
    return min_x<=width*0.005f &&
           max_x>=gutter-0.5f;
}

/* Reconstruct one original motif TRIANGLE's exact colour at the left or
 * right monitor edge. Each game-owned 4-vertex triangle fan has triangles
 * (0,1,2) and (0,2,3). Only a NONDEGENERATE triangle intersecting the
 * physical edge over a positive Y interval can contribute an exterior
 * strip. A point-only intersection must not paint an entire band.
 *
 * The replay projects that SOURCE EDGE vertically across the outside
 * gutter. It does not stretch the motif geometry or invent one uniform
 * ARGB for the whole height. Diffuse and native blending come from the
 * source command and its original state block.
 */
static inline int SpiderUIComp_TriangleEdgeSpan(
    float x0,float y0,float x1,float y1,float x2,float y2,
    float edge,float height,SpiderUICompYSpan *span)
{
    const float xs[3]={x0,x1,x2};
    const float ys[3]={y0,y1,y2};
    float minimum=height+1.0f,maximum=-1.0f;
    float area=(x1-x0)*(y2-y0)-(x2-x0)*(y1-y0);
    unsigned i;
    if(!span || !(height>=480.0f && height<=16384.0f) ||
       !(edge>=0.0f && edge<=16384.0f) ||
       !(area>0.1f || area < -0.1f))
        return 0;
    for(i=0u;i<3u;i++) {
        unsigned j=(i+1u)%3u;
        float xa=xs[i]-edge,xb=xs[j]-edge;
        /* Vertex exactly on the physical edge. */
        if(xa>=-0.25f && xa<=0.25f) {
            if(ys[i]<minimum)minimum=ys[i];
            if(ys[i]>maximum)maximum=ys[i];
        }
        /* An original triangle crossing the physical edge can have
         * neither of its endpoints lying precisely on the border. */
        if((xa < -0.25f && xb > 0.25f) ||
           (xa > 0.25f && xb < -0.25f)) {
            float y=ys[i]+(ys[j]-ys[i])*(-xa)/(xb-xa);
            if(y<minimum)minimum=y;
            if(y>maximum)maximum=y;
        }
    }
    if(!(maximum>minimum+0.5f) ||
       maximum<=0.0f || minimum>=height ||
       minimum < -height*.05f || maximum > height*1.05f)
        return 0;
    span->top=minimum<0.0f?0.0f:minimum;
    span->bottom=maximum>height?height:maximum;
    return span->bottom>span->top+0.5f;
}

/* Identify a source-backed, uniform native backdrop independently of
 * the inner spider, without any hardcoded colour, camera depth or level.
 *
 * Source evidence 2026-11-06: the ESC motif has two flat diffuse colour
 * families. The background one reaches every physical screen edge,
 * continuously, while the other is strictly interior. A fullscreen
 * overlay may replace ONLY that proven background family; the interior
 * artwork remains native and is mapped as one 4:3 object.
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

/* Piecewise continuous native-background projection:
 * the INTERIOR spider keeps the normal centered 4:3 map exactly,
 * whereas only the original backdrop outside its measured source
 * bounding box is extended to the physical left/right edges.
 *
 * Mapping shared source coordinates identically avoids gaps between
 * tessellated backdrop triangles. It NEVER adds a second translucent
 * pass beneath the spider (which would double-blend its alpha).
 */
static inline float SpiderUIComp_BackdropMapX(
    float x,float width,float height,float inner_left,float inner_right)
{
    float scale,center,mapped_left,mapped_right;
    if(width<640.0f || height<480.0f || width>16384.0f ||
       height>16384.0f || width*3.0f<=height*4.0f ||
       !(inner_left>1.0f && inner_left<inner_right &&
         inner_right<width-1.0f))
        return x;
    scale=(height*4.0f)/(width*3.0f);
    center=width*.5f;
    mapped_left=center+(inner_left-center)*scale;
    mapped_right=center+(inner_right-center)*scale;
    if(x<inner_left)
        return x*mapped_left/inner_left;
    if(x>inner_right)
        return mapped_right+(x-inner_right)*
              (width-mapped_right)/(width-inner_right);
    return center+(x-center)*scale;
}

#endif
