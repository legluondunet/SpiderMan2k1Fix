# Wine/X11: no-Flip Alt+Tab pause fallback (experimental)

The permanent SpiderMan2k1Fix **PAUSED** overlay is not the game's native
Escape pause menu. Normally the custom pause starts at DirectDraw `Flip`,
because that is the verified game render-thread boundary.

## Why this experiment exists

In `SpiderMan2k1Fix(4).log` (2026-11-04), five observed focus losses
between 04:54:51 and 04:56:39 entered pause 167–206 ms after the Wine
`WM_KILLFOCUS` event. At 04:57:45.009, the game received another
`WM_KILLFOCUS` with a NULL foreground HWND, but the log ended without
another Flip or PAUSED presentation. This final case is inconclusive: it
does not prove the game stopped rendering indefinitely.

## How to try

Set environment variable `SPIDERMAN2K1FIX_FOCUS_TIMER=1` **for the game**.
It is absent/disabled by default, and makes no INI changes.

When enabled, a Windows `SetTimer` with a 50-ms period uses a `TimerProc`
on the *same game window thread*. It calls the existing pause logic only
if (a) the game previously held focus; (b) another window, or a verified
inactive game thread with NULL foreground, is now active; and (c) there
has been no Flip for at least 250 ms. The normal extra 150-ms focus
debounce remains in force. No thread is suspended; no new window is
created or activated.

It can only work if the game still dispatches `WM_TIMER` messages during
background focus. If Wine or the game stops the entire message pump,
it cannot start a pause without new, significantly riskier machinery.

## Validation matrix

1. Launch with the environment variable absent: verify unchanged normal
   menu, video, and 3D Alt+Tab behavior.
2. Enable it and confirm `FOCUS-TIMER: EXPERIMENT ARMED` in the log.
3. In each of the opening Bink cinematics: Alt+Tab to a native Linux app
   while the movie is playing, stay outside for at least 5 seconds, then
   return. Record whether the video visibly freezes and whether PAUSED
   is displayed immediately.
4. In a 3D level: repeat Alt+Tab twice in a row, confirming that both
   instances pause and that gameplay, controller and audio resume.
5. Compare X11 `_NET_ACTIVE_WINDOW` timestamp traces against Wine
   `WM_ACTIVATEAPP` / `WM_KILLFOCUS`, `FOCUS-TIMER`, `FOCUS-PROBE`
   and `FOCUS-SURFACE` markers.
6. A successful CI build is not a runtime test. Confirm on real Windows
   before considering a default-on switch.

**Known uncertainties:** TimerProc delivery depends on the game message
pump; the last 3D loss in the log is not proof the fallback will run.
DirectDraw composition outside the Flip boundary may render a stale
backbuffer, particularly in cinematics. The branch remains experimental.

legluondunet assisted by ChatGPT (OpenAI)

## Timer delivery diagnosis (2026-11-04)

After the `v1.1.0-10c870b` test, the timer was **armed** at
05:22:41.752 but there was no `FOCUS-TIMER: no-Flip candidate` in the
entire session. Three ordinary pauses succeeded; the final loss at
05:27:32.883 ended the Wine log immediately, without evidence of further
`Flip` or callback. This does **not** establish that the callback ran,
nor that the message pump continued during the user's long Alt+Tab.

A bounded heartbeat has therefore been added to the **same opt-in
experiment**. With `SPIDERMAN2K1FIX_FOCUS_TIMER=1` and `[Debug] Log=1`:

- `FOCUS-TIMER: EXPERIMENT ARMED` proves only timer registration.
- `FOCUS-TIMER: CALLBACK heartbeat=` proves actual `WM_TIMER`
  dispatch. The first three callbacks are logged even in the foreground,
  then transitions and a sample every 30 seconds (30-line cap).
- `FOCUS-TIMER: no-Flip candidate` requires both callback dispatch
  *and* a blocked Flip for at least 250 ms while unfocused.
- If the log shows initial heartbeats but none after the last
  `WM_KILLFOCUS`, the game's background message pump may have stopped;
  the existing same-thread TimerProc cannot safely freeze the game then.
  Do not treat that inference as proof without a concurrent host-side
  X11/window trace.

This change is diagnostic only; no new worker thread or cross-thread
DirectDraw calls were introduced.
