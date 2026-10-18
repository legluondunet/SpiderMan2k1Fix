#ifndef SPIDERMAN2K1FIX_FRAME_LIMITER_H
#define SPIDERMAN2K1FIX_FRAME_LIMITER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Only initialize once, BEFORE installing the DirectDraw Flip hook.
 * A zero FPS limit disables waiting. A positive number limits presentation
 * (Flip calls) without modifying original game-timing code.
 */
typedef struct SpiderFrameLimiter {
    CRITICAL_SECTION lock;
    LARGE_INTEGER frequency;
    LONGLONG last_flip_tick;
    LONGLONG frame_interval_ticks;
    unsigned int max_fps;
    BOOL active;
    BOOL lock_initialized;
} SpiderFrameLimiter;

BOOL SpiderFrameLimiter_Init(SpiderFrameLimiter *limiter, unsigned int fps);
void SpiderFrameLimiter_Wait(SpiderFrameLimiter *limiter);
void SpiderFrameLimiter_Destroy(SpiderFrameLimiter *limiter);

#endif
