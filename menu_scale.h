#ifndef SPIDERMAN2K1FIX_MENU_SCALE_H
#define SPIDERMAN2K1FIX_MENU_SCALE_H
#include <stdint.h>

/* The observed menu uses XYZRHW|DIFFUSE|TEX1 (FVF 0x144), 28 bytes/vertex.
 * Keep its depth, color and UV data unchanged, and never write into game memory.
 * All calculations are independent of DirectDraw and testable on Linux.
 */
typedef struct SpiderMenuVertex {
    float x, y, z, rhw;
    uint32_t diffuse;
    float u, v;
} SpiderMenuVertex;

typedef struct SpiderMenuMapping {
    int output_w, output_h;
    int left, top, right, bottom;
    float scale_x, scale_y;
} SpiderMenuMapping;

int SpiderMenu_MakeMapping(int logical_w, int logical_h,
                           int output_w, int output_h,
                           SpiderMenuMapping *map);
int SpiderMenu_TransformVertices(const SpiderMenuMapping *map,
                                  const SpiderMenuVertex *src,
                                  SpiderMenuVertex *dst,
                                  unsigned count);
#endif
