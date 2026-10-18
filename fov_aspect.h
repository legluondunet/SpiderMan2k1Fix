#ifndef SPIDERMANFIX_FOV_ASPECT_H
#define SPIDERMANFIX_FOV_ASPECT_H
#include "menu_scale.h"

/* FOV input syntax. Degrees are accepted as a future interface contract,
 * NOT converted to a camera projection until the original FOV is known. */
typedef enum SpiderFOVSetting {
  SPIDER_FOV_INVALID = 0,
  SPIDER_FOV_AUTO = 1,
  SPIDER_FOV_DEGREES = 2
} SpiderFOVSetting;

SpiderFOVSetting SpiderFOV_Parse(const char *value, double *degrees);
/* The game has a verified aspect scalar, NOT a verified field-of-view angle.
 * This computes only the documented 4:3-relative aspect correction.
 * Returns 0 on invalid screen geometry; otherwise returns 1.
 */
int SpiderFOV_AspectScalar(unsigned width,unsigned height,float *out);

/* Controlled Hor+ experiment: when the game's calibrated-on-screen zoom
 * scalar is decreased, restore only the projected Y component of world quads.
 * Caller must first verify native gameplay and z-buffer enabled. Pure math.
 */
int SpiderFOV_IsWorldQuad(const SpiderMenuVertex *v, unsigned n);
int SpiderFOV_RestoreVertical(const SpiderMenuVertex *source,
                              SpiderMenuVertex *dest, unsigned n,
                              float scalar, float screen_mid_y);

#endif