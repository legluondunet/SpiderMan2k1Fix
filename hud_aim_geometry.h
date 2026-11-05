#ifndef SPIDERMAN2K1FIX_HUD_AIM_GEOMETRY_H
#define SPIDERMAN2K1FIX_HUD_AIM_GEOMETRY_H

/* Geometry-only model for a game-owned aiming reticle known to originate
 * from 4:3 coordinates and then receive widescreen horizontal stretching.
 *
 * The user supplied 1920x1080 screenshots (2026-11-04):
 * Dreamcast green circular outline ~163x162;
 * PC green outline ~94x70; PC red outline ~153x113.
 * Width/height ratios 1.01 vs ~1.34/1.35 support the inverse of
 * the 4:3 -> 16:9 stretch, i.e. 0.75 horizontal scaling.
 *
 * IMPORTANT: this is NOT a renderer hook or a screen-space heuristic.
 * Do not apply to generic DrawPrimitive geometry! The actual game
 * reticle still needs exact D3D7 caller/depth/texture identification
 * from Log=1 before the operation can safely be enabled at runtime.
 * It leaves vertical position, UV, color, depth and image scaling alone.
 */
#include <stdint.h>

static inline float SpiderHudAim_AspectFactor(
    unsigned output_w, unsigned output_h)
{
    if (output_w < 640u || output_h < 480u ||
        output_w > 16384u || output_h > 16384u ||
        (uint64_t)output_w * 3u <= (uint64_t)output_h * 4u)
        return 1.0f;
    return (4.0f * (float)output_h) / (3.0f * (float)output_w);
}

static inline float SpiderHudAim_MapX(
    float original_x, float object_center_x,
    unsigned output_w, unsigned output_h)
{
    return object_center_x +
        (original_x - object_center_x) *
         SpiderHudAim_AspectFactor(output_w, output_h);
}
#endif
