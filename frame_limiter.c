/* Non-invasive DirectDraw Flip pacing for the 2001 Spider-Man PC game.
 * The interval uses a ceil division so the limiter does not intentionally
 * exceed the configured FPS (apart from timer precision and concurrency).
 * Does not call timeBeginPeriod(), patch any game function, or use GDB.
 */
#include "frame_limiter.h"

BOOL SpiderFrameLimiter_Init(SpiderFrameLimiter *limiter, unsigned int fps)
{
    LARGE_INTEGER freq;
    if (!limiter) return FALSE;
    *limiter = (SpiderFrameLimiter){0};
    if (fps == 0) return TRUE; /* explicitly disabled */
    if (fps < 1 || fps > 240 || !QueryPerformanceFrequency(&freq) ||
        freq.QuadPart < (LONGLONG)fps) return FALSE;
    limiter->frequency = freq;
    limiter->frame_interval_ticks = (freq.QuadPart + fps - 1) / fps;
    limiter->max_fps = fps;
    InitializeCriticalSection(&limiter->lock);
    limiter->lock_initialized = TRUE;
    limiter->active = TRUE;
    return TRUE;
}

void SpiderFrameLimiter_Wait(SpiderFrameLimiter *limiter)
{
    LARGE_INTEGER clock_now;
    LONGLONG target, remaining;
    DWORD sleep_ms;
    if (!limiter || !limiter->active) return;
    EnterCriticalSection(&limiter->lock);
    if (!QueryPerformanceCounter(&clock_now)) {
        limiter->last_flip_tick = 0;
        LeaveCriticalSection(&limiter->lock);
        return;
    }

    /* The first frame is never delayed. Reset schedule after pause, lag,
     * unexpected clock movement, or an application stall > 1 second.
     */
    if (limiter->last_flip_tick > 0 &&
        clock_now.QuadPart >= limiter->last_flip_tick &&
        clock_now.QuadPart - limiter->last_flip_tick < limiter->frequency.QuadPart) {
        target = limiter->last_flip_tick + limiter->frame_interval_ticks;
        while (clock_now.QuadPart < target) {
            remaining = target - clock_now.QuadPart;
            /* Let Sleep cover the long part; yield for the final 1–2 ms.
             * Sleep timing depends on Wine/Windows scheduler precision.
             */
            sleep_ms = (DWORD)((remaining * 1000) / limiter->frequency.QuadPart);
            if (sleep_ms > 2) Sleep(sleep_ms - 2);
            else SwitchToThread();
            if (!QueryPerformanceCounter(&clock_now)) {
                limiter->last_flip_tick = 0;
                LeaveCriticalSection(&limiter->lock);
                return;
            }
        }
    }
    limiter->last_flip_tick = clock_now.QuadPart;
    LeaveCriticalSection(&limiter->lock);
}

void SpiderFrameLimiter_Destroy(SpiderFrameLimiter *limiter)
{
    /* Call only after all rendering threads and Flip hooks have stopped. */
    if (!limiter) return;
    if (limiter->lock_initialized) {
        DeleteCriticalSection(&limiter->lock);
        limiter->lock_initialized = FALSE;
    }
    limiter->active = FALSE;
}