#include "menu_scale.h"
#include "display_plan.h"
#include <string.h>
#include <stddef.h>

_Static_assert(sizeof(SpiderMenuVertex) == 28, "Unexpected Direct3D 7 FVF 0x144 vertex layout");

int SpiderMenu_MakeMapping(int logical_w, int logical_h,
                           int output_w, int output_h,
                           SpiderMenuMapping *map)
{
    SpiderDisplayRect rect;
    if (!map || !SpiderDisplay_Fit(logical_w, logical_h, output_w, output_h, &rect))
        return 0;
    memset(map, 0, sizeof(*map));
    map->output_w = output_w;
    map->output_h = output_h;
    map->left = rect.left;
    map->top = rect.top;
    map->right = rect.right;
    map->bottom = rect.bottom;
    map->scale_x = (float)(rect.right - rect.left) / (float)logical_w;
    map->scale_y = (float)(rect.bottom - rect.top) / (float)logical_h;
    return 1;
}

int SpiderMenu_TransformVertices(const SpiderMenuMapping *map,
                                  const SpiderMenuVertex *src,
                                  SpiderMenuVertex *dst,
                                  unsigned count)
{
    unsigned i;
    if (!map || !src || !dst || !map->output_w || !map->output_h || count == 0)
        return 0;
    if (count > 65536u) return 0;
    /* Caller provides distinct src/dst to preserve the game's original vertices. */
    if (src == dst) return 0;
    memcpy(dst, src, (size_t)count * sizeof(*dst));
    for (i = 0; i < count; ++i) {
        dst[i].x = (float)map->left + src[i].x * map->scale_x;
        dst[i].y = (float)map->top  + src[i].y * map->scale_y;
    }
    return 1;
}