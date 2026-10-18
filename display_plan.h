#ifndef SPIDERMAN2K1FIX_DISPLAY_PLAN_H
#define SPIDERMAN2K1FIX_DISPLAY_PLAN_H

/* Pure geometry math, independent of Windows APIs and the game.
 * Fits source geometry into destination without stretching and centers it.
 */
typedef struct SpiderDisplayRect {
    int left, top, right, bottom;
} SpiderDisplayRect;

int SpiderDisplay_Fit(int source_w, int source_h,
                      int screen_w, int screen_h,
                      SpiderDisplayRect *out);

/* Intro FMV output: 640x480 and 320x240 keep the game's 4:3 stage.
 * A 320x192 clip can optionally use its 5:3 square-pixel source ratio.
 * Source dimensions are not monitor dimensions; the fit is dynamic.
 */
int SpiderDisplay_FitIntroVideo(int source_w, int source_h,
                                int screen_w, int screen_h,
                                int preserve_320x192_aspect,
                                SpiderDisplayRect *out);

/* Horizontal projection/HUD factor for native widescreen planning only.
 * Does not alter the game camera or renderer.
 */
double SpiderDisplay_NativeHorizontalFactor(int base_w,int base_h,
                                             int screen_w,int screen_h);

#endif
