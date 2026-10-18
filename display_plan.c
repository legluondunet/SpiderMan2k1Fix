#include "display_plan.h"
#include <stdint.h>

int SpiderDisplay_Fit(int source_w, int source_h,
                      int screen_w, int screen_h,
                      SpiderDisplayRect *out)
{
    int width, height;
    if (!out || source_w < 1 || source_h < 1 ||
        screen_w < 1 || screen_h < 1 ||
        screen_w > 32768 || screen_h > 32768) return 0;
    if ((int64_t)screen_w * source_h <= (int64_t)screen_h * source_w) {
        width = screen_w;
        height = (int)((int64_t)screen_w * source_h / source_w);
    } else {
        height = screen_h;
        width = (int)((int64_t)screen_h * source_w / source_h);
    }
    if (width < 1 || height < 1) return 0;
    out->left = (screen_w - width) / 2;
    out->top = (screen_h - height) / 2;
    out->right = out->left + width;
    out->bottom = out->top + height;
    return 1;
}

int SpiderDisplay_FitIntroVideo(int source_w, int source_h,
                                int screen_w, int screen_h,
                                int preserve_320x192_aspect,
                                SpiderDisplayRect *out)
{
    if (preserve_320x192_aspect && source_w == 320 && source_h == 192)
        return SpiderDisplay_Fit(320, 192, screen_w, screen_h, out);
    return SpiderDisplay_Fit(640, 480, screen_w, screen_h, out);
}

double SpiderDisplay_NativeHorizontalFactor(int base_w,int base_h,
                                             int screen_w,int screen_h)
{
    double source, target;
    if (base_w < 1 || base_h < 1 || screen_w < 1 || screen_h < 1) return 1.0;
    source = (double)base_w / (double)base_h;
    target = (double)screen_w / (double)screen_h;
    return target > source ? source / target : 1.0;
}