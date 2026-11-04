# Wine/X11 Alt+Tab: deferred pause on app reactivation (experimental)

## Observation, 4 November 2026

`SpiderMan2k1Fix(7).log` (`v1.1.0-617dff5`, GE-Proton11-12 under
Manjaro) shows a successful first 3D Alt+Tab:
- 08:51:13.249: `WM_ACTIVATEAPP(FALSE)` and `WM_KILLFOCUS`.
- 08:51:13.431: `FOCUS-PAUSE` latched after a 173 ms Flip-sampled
  external-window candidate.
- 08:51:13.451: custom `PAUSED` panel presented.
- 08:52:29.689: explicit manual resume.

A second long Alt+Tab did not pause:
- 08:55:06.354–355: `WM_ACTIVATEAPP(FALSE)`, `WM_KILLFOCUS`;
  foreground becomes `NULL`.
- No later Flip, PAUSED, focus-return event or other log entry in
  the supplied file; the user independently reports a long absence.
- `FOCUS-TIMER: EXPERIMENT ARMED` appears at startup in the earlier
  timer experiment, but there is **not one** `FOCUS-TIMER: CALLBACK
  heartbeat` across the whole session. Successful `SetTimer` cannot
  substitute for actually dispatching `WM_TIMER`.

## Alternative, strictly opt-in

Enable `SPIDERMAN2K1FIX_FOCUS_RETURN_LATCH=1` in the game environment.
This is independent of `SPIDERMAN2K1FIX_FOCUS_TIMER`. Test the return
latch on its own with the timer variable unset.

The already-existing game-window subclass observes only
`WM_ACTIVATEAPP(FALSE)` / `WM_ACTIVATEAPP(TRUE)`. It records the
elapsed duration and requires at least 150 ms. After a prolonged
app deactivation, it queues **one** deferred PAUSED request.

On the **first subsequent foreground Flip**, the same rendering thread
runs the existing custom pause path, rendering PAUSED on the game
DirectDraw surfaces, pausing Bink/DirectSound and waiting for an explicit
key/gamepad resume. If a normal Flip-driven pause already latched while
background, it cancels the deferred record so it cannot display twice.

No background polling thread, synthetic input, forced activation,
off-thread DirectDraw access or Bink call in WndProc.

## Important limitation

This is a **fallback on RETURN**, not an assertion that Spider-Man
freezes immediately in the background. If the engine stops both Flip
and Win32 message dispatch after deactivation, no same-thread renderer
can display a PAUSED frame while it is away. Whether simulation/audio
continues must be tested separately. This approach does not solve
instantaneous pause during Bink cinematics without extra evidence.

## Test matrix

1. Do not enable the timer. Enable `SPIDERMAN2K1FIX_FOCUS_RETURN_LATCH=1`
   and `[Debug] Log=1`; use the built PR artifact.
2. First, from 3D gameplay, switch away and return after 1 second. Verify
   normal Flip-driven pause still wins; resume once, never twice.
3. Second, from gameplay, switch away for 15–30 seconds. On return, check
   `FOCUS-RETURN: sustained app deactivation ...`, followed by
   `FOCUS-RETURN: deferred PAUSED latch at first returned Flip` if
   the original background pause was missed. Manual resume must still work.
4. Repeat with an opening Bink cinematic. Record whether playback stops
   *during* background and whether PAUSED appears on return; they are
   distinct tests.
5. Check brief task switching (<150 ms), normal game startup, menu and
   Escape native Pause: no spurious custom pause.
6. Runtime Windows testing is required before a merge or default enable.

This PR is experimental; **do not merge without user validation**.

legluondunet assisted by ChatGPT (OpenAI)

## Retest 8: focus dispatch completion probe

The user's `SpiderMan2k1Fix(8).log` (`v1.1.0-f3ff0f1`, CI
merge-test binary) again shows one successful 3D loss followed by a
second failed loss.

- 09:14:50.795: `WM_ACTIVATEAPP(FALSE)`/`WM_KILLFOCUS`;
  09:14:50.965 confirmed PAUSED; 09:14:50.985 panel presented;
  09:16:01.530 activation; 09:16:03.233 manual resume.
- Before the last focus change, `menu-active=1`: last observed
  `Flip` at 09:17:56.037, roughly **1.20 seconds before** the
  09:17:57.238 `WM_ACTIVATEAPP(FALSE)`/`WM_KILLFOCUS`.
  A `Blt` was still observed at 09:17:57.118.
- The file ends immediately after logging `WM_KILLFOCUS`.
  There is no later recorded reactivation, so the previous
  return-only fallback had no opportunity to present PAUSED.
  Neither a native WndProc blockage nor a stopped message pump
  can be proved by this log.

A bounded, strictly opt-in `FOCUS-RETURN: ORIGINAL-WNDPROC enter/returned`
pair now brackets forwarding of `WM_ACTIVATEAPP`, `WM_ACTIVATE`,
`WM_KILLFOCUS` and `WM_SETFOCUS` to the game's original handler.

- `enter WM_KILLFOCUS` without `returned WM_KILLFOCUS` suggests
  the original handler may be stuck; compare with a host-side process
  snapshot before concluding.
- A matching `returned WM_KILLFOCUS` proves the original handler
  completed and the later silence is elsewhere (render loop, message
  pump, or game termination).
- Event-level `FOCUS-RETURN` traces are now bounded and logged
  whenever the experiment is enabled, independent of `[Debug] Log`.

This adds **diagnostics only**, no change to the pause algorithm.
