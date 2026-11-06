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

#define SPIDER_UI_COMP_MAX_COMMANDS 256u

static inline int SpiderUIComp_ShouldDefer(int trial_enabled,
        int scene_open, int scene_bailout, int native_layer,
        unsigned vertex_count, unsigned fvf)
{
    return trial_enabled && scene_open && !scene_bailout &&
           (native_layer == SPIDER_UI_COMP_PAUSE_DECOR ||
            native_layer == SPIDER_UI_COMP_PAUSE_MENU) &&
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

#endif
