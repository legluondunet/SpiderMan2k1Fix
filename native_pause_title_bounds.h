#ifndef SPIDERMAN2K1FIX_NATIVE_PAUSE_TITLE_BOUNDS_H
#define SPIDERMAN2K1FIX_NATIVE_PAUSE_TITLE_BOUNDS_H

/* Original ESC/confirmation title labels use the same 16/32/64x32 font
 * quads as menu choices, but titles such as "SAVE GAME PROGRESS ?" are
 * wider than the central 50% X-zone initially calibrated for PAUSED and
 * REALLY QUIT?. The original transform mixed corrected interior letters
 * with uncorrected first/last letters, creating visible gaps.
 *
 * This helper ONLY refines horizontal eligibility AFTER the native game's
 * exact original D3D7 caller, 4-vertex/FVF, same-frame purple ribbon,
 * font texture, Z/RHW, and global stage guards have all matched.
 *
 * Keep menu choices in their previously validated middle 50%. Widen only
 * the title row at 16%-34% of the viewport height to the middle 64%.
 * No vertex changes are performed here; only coordinates are classified.
 */
static inline int SpiderNativePause43_TextInZone(
    float left, float right, float top, float bottom,
    float screen_width, float screen_height)
{
    if (!(screen_width >= 640.0f && screen_height >= 480.0f &&
          right > left && bottom > top))
        return 0;

    /* Preserve all previously approved central dialog choices. */
    if (left > screen_width * 0.25f &&
        right < screen_width * 0.75f)
        return 1;

    /* Restrict the widened X-zone to dialogue headings, not gameplay HUD. */
    return top >= screen_height * 0.16f &&
           bottom <= screen_height * 0.34f &&
           left > screen_width * 0.18f &&
           right < screen_width * 0.82f;
}

#endif
