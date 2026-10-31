#ifndef SPIDERMAN2K1FIX_SDL3_SETUP_FOCUS_POLICY_H
#define SPIDERMAN2K1FIX_SDL3_SETUP_FOCUS_POLICY_H

/* Pure Win32-free policy for returning from an owned SDL3 setup modal.
 *
 * HWND values must be compared in the caller WHILE the window still
 * exists (WM_CLOSE, before DestroyWindow). DestroyWindow itself can
 * synchronously activate Explorer or another unrelated top-level window.
 * The foreground AFTER that destruction is not reliable evidence of user
 * Alt+Tab. Conversely an unrelated foreground captured BEFORE destruction
 * must never authorize stealing focus from that external application.
 *
 * No forced window activation here: only permission for the caller's
 * existing same-thread SetActiveWindow / SetForegroundWindow sequence.
 */
static int spider_sdl3_setup_restore_focus(
    int has_quit,int closing_snapshot_valid,int preclose_is_owned,
    int afterclose_is_owned,int preclose_is_null,int afterclose_is_null) {
  if(has_quit || !closing_snapshot_valid)return 0;
  if(preclose_is_owned)return 1;
  /* Wine sometimes exposes NULL foreground even while an owned modal
   * processes WM_CLOSE. Preserve the older NULL fallback, but only if
   * closing itself also saw NULL and the final foreground is our own or
   * still NULL. A known foreign foreground is NEVER eligible. */
  if(preclose_is_null && (afterclose_is_owned || afterclose_is_null))
    return 1;
  return 0;
}

#endif /* SPIDERMAN2K1FIX_SDL3_SETUP_FOCUS_POLICY_H */
