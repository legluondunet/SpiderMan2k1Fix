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
#endif
