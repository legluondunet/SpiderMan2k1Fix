#ifndef SPIDER_WORLD_QUAD_PROJECTION_H
#define SPIDER_WORLD_QUAD_PROJECTION_H
/* The native fixed-point world-quad path submits pixel X, Y, depth,
 * UV and color. The X projection lacks the successful gameplay FOV
 * scalar. Only X is mapped, about the native viewport midpoint.
 * Validate the entire quad before changing any output. Offscreen
 * vertices are intentionally retained for original clipping. */
static inline int SpiderWorldQuad_MapX(const float source[4],unsigned width,
                                       float scalar,float out[4]) {
  float center, mapped[4];
  unsigned i;
  if(!source || !out || width<320u || width>32768u ||
     !(scalar>=0.3f && scalar<1.0f))return 0;
  center=(float)width*0.5f;
  for(i=0;i<4;i++){
    if(!(source[i]>=-128.f*(float)width &&
         source[i]<=128.f*(float)width))return 0;
    mapped[i]=center+(source[i]-center)*scalar;
  }
  for(i=0;i<4;i++)out[i]=mapped[i];
  return 1;
}
#endif
