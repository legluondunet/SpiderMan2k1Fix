# Unexpected automatic focus pause in Windows 11 3D gameplay — diagnostic

## Report (2026-10-21)

During active 3D gameplay **without the user switching applications**, the
game reportedly pauses by itself after about **six seconds**. The game scene
turns black and the centered red-outline PAUSE label remains visible.
Pressing a keyboard key does not resume it. Using Alt+Tab to explicitly
reselect the Spider-Man window, then pressing a key, resumes gameplay —
but the issue returns after about six seconds.

This behavior is **not yet explained**. The existing focus-loss debounce is
**150 milliseconds**, not six seconds. The current game code latches pause
when another process owns the foreground for long enough. While the game is
not the foreground window, the first resume key intentionally cannot wake
it. We need to determine **which foreground window** is being reported.

## Safe diagnostics (no ProcDump)

1. Back up `SpiderMan2k1Fix.ini` and both local DLLs.
2. Install this diagnostic test build, changing nothing else.
3. Ensure these lines appear in your current INI:
   ```ini
   [Debug]
   Log=1

   [Input]
   PauseOnFocusLoss=1
   PauseOverlay=1
   PauseKey=Any
   DirectInputReacquire=1
   PauseBinkOnFocusLoss=1
   PauseDirectSoundOnFocusLoss=1
   RestoreCursorOnFocusLoss=1
   ```
4. Launch **normally without ProcDump**, start 3D gameplay, **do not touch
   Alt+Tab**, and wait until the unwanted pause occurs. When possible, use
   Alt+Tab to select the game, press the wake key, and allow a second
   involuntary pause. Quit using the normal Windows close gesture if needed.
5. Share only `SpiderMan2k1Fix.log`. The relevant lines are:
   ```text
   FOCUS-PAUSE: confirmed external focus loss; pause latched until manual resume
   FOCUS-PAUSE-OWNER: external-foreground-latched game=... pid=... foreground=... pid=... class=... exe=... paused-after-ms=...
   FOCUS-PAUSE-OWNER: resume-foreground-state game=... foreground=... exe=...
   ```

The logger records only the **executable filename**, not the full process
path or window title. The log is conditional on `[Debug] Log=1`, and emits
on pause latch and resume, not every frame.

## Isolation and rollback

First compare with `PauseOverlay=0`, keeping `PauseOnFocusLoss=1`, and
restart the game. This tests whether the shaped overlay is relevant to the
unexpected loss of focus. **If the unwanted pause still occurs, focus
detection — not the drawing code — must be investigated.**

For uninterrupted gameplay until a fix is found, set
`PauseOnFocusLoss=0` and restart the game. This temporarily disables the
focus-pause behavior (and consequently the synchronized media pause on
Alt+Tab); the old unfixed Alt+Tab input problem can reappear, so avoid
switching applications while playing in this configuration.

This is a **diagnostic-only PR**, which does not alter focus decisions,
audio or rendering. Do not merge until the culprit is identified and a
subsequent corrective change is tested.

---
legluondunet assisted by ChatGPT (OpenAI)
