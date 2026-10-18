#ifndef SPIDERMANFIX_COMIC_NATIVE_H
#define SPIDERMANFIX_COMIC_NATIVE_H
#include "menu_scale.h"
int SpiderComicNative_IsCandidate(const SpiderMenuVertex *v,
    unsigned count, unsigned primitive, unsigned long fvf,
    unsigned long caller_rva, unsigned texture_w, unsigned texture_h,
    unsigned screen_w, unsigned screen_h);
int SpiderComicNative_Transform(const SpiderMenuVertex *src,
    SpiderMenuVertex *dst,unsigned count,unsigned screen_w,unsigned screen_h);
#endif