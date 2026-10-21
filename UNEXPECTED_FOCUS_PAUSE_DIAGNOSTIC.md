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

## Evidence from user's Windows 11 log

The log `SpiderMan2k1Fix(20261021-080901).log` shows multiple actual pause latches,
including menus and 3D, when Windows reports the *same helper PID/thread*
`27444/42824`, window class **`Ghost`**, and a changing window handle.
The game itself is process `32804` and owns window class `Spiderman PC`.
Foreground loss lasts 156–204 milliseconds before latching, consistent with
the coded 150 ms debounce. The repeated multi-second period is consistent
with Windows ghosting windows whose UI message pump does not service
messages for several seconds. This points toward Windows ghosting, but the
reason the game's message loop goes unscheduled is not yet confirmed.

## Experimental opt-in suppression to test

```ini
[Input]
DisableWindowsGhosting=1
```

This new option is **OFF by default**. After restarting Spider-Man, it calls
Microsoft's `DisableProcessWindowsGhosting` before the first game window
appears. The API does not have a reverse operation: to restore Windows'
normal hang-window handling, set `DisableWindowsGhosting=0` and restart.
It suppresses the system's movable/closable 'Not Responding' ghost surrogate
for the **entire process lifetime**. It **does not** repair a genuine hang;
the user must still be able to exit the game normally. If Spider-Man hangs
for real, Task Manager may be needed to terminate it.

Keep `PauseOnFocusLoss=1` and `PauseKey=Any` to verify a *real* Alt+Tab
still pauses and resumes correctly in menus, cinematic and gameplay.
Also verify that no spontaneous pause occurs for at least a minute without
switching applications. Report whether 3D input remains responsive.

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
