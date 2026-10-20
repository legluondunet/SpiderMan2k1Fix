# Cross-platform focus-event pause — experimental prototype

**Status: unvalidated on actual Spider-Man on Windows 11 and Wine 11.17/X11.**
The current production default is unchanged. No DirectInput8 COM/IAT
interception, key injection, synthetic input, `SuspendThread`, window
repositioning, or focus stealing is used.

## Why this experiment exists

The older `PauseOnFocusLoss=1` waits for foreground loss at the
`IDirectDrawSurface7::Flip` call. If the game stops presenting
temporarily, it cannot even observe loss until a later Flip. The
experimental DirectInput8 reacquisition hooks help under Wine, but
previously prevented startup on native Windows 11. They are **not**
suitable as a cross-platform default.

We need to determine whether an immediate Win32 focus notification can
make the game pause at a *safe* call boundary promptly, without
introducing a Windows startup regression.

### What this prototype actually does

- Subclasses only the game's known window procedure, after successful
  `IDirectDraw7::SetCooperativeLevel`. Listens for
  `WM_ACTIVATEAPP(FALSE)` and `WM_KILLFOCUS` and records a
  `GetTickCount` timestamp. `WM_ACTIVATEAPP(TRUE)` and
  `WM_SETFOCUS` record restoration, if the game is foreground.
- Uses the already-validated `Flip` call **only as the first safe pause
  gate**. This gate pumps Windows messages (including Alt+Tab/focus
  restoration) and preserves `WM_QUIT`. The state set by WndProc is
  read without waiting inside WndProc.
- Logs a bounded `FOCUS-EVENT` line per transition and entry/exit,
  including elapsed milliseconds: **loss event → Flip gate entered**,
  **time spent waiting**, and **loss event → focus restoration**.
- Falls back to `GetForegroundWindow` at Flip if loss notification
  wasn't delivered yet, to avoid accidentally continuing gameplay.
- Keeps the old `PauseOnFocusLoss` code untouched. If
  `ExperimentalFocusEventPause=1` and both are set, the **new gate
  takes precedence** at Flip, avoiding two nested waits.
- Rejects activating when `ExperimentalFocusTrace=1` or
  `ExperimentalAltTabReacquire=1`, because those modes may already
  subclass the window and hook DirectInput.

**Limit:** This is not yet a true immediate *engine/simulation* pause.
It only records the focus event immediately and pauses at the NEXT
`Flip` when that boundary is reached. A movie/decoder/audio thread
could continue independently. If measured event → gate delay is too
long, the next step is to **identify and validate the game's own update
boundary** rather than blocking arbitrary graphics or DirectInput
calls. No unverifiable game-executable hook is introduced here.

## Configuration

Copy the `SpiderMan2k1Fix.ini` from this branch or preserve your
current installation and edit the `[Input]` options as follows:

```ini
[Debug]
Log=1

[Input]
ExperimentalFocusEventPause=1
PauseOnFocusLoss=0
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
RestoreCursorOnFocusLoss=1
```

Use both compiled x86 DLLs from the **same CI artifact on this
branch**. Back up your previous DLLs and INI first. No changes to your
game EXE or Wine prefix are needed. In Lutris use native
`winmm` override `n,b` (or `WINEDLLOVERRIDES=winmm=n,b`).

**CAUTION:** The additional WndProc observer is deliberately
experimental; Windows 11 and Wine must be tested **independently**.
If startup or focus behavior regresses, restore original DLLs and set
`ExperimentalFocusEventPause=0`. Do not use this build as a stable
release before validating both platforms.

## Manual validation and evidence

1. **Native Windows 11 (first):** start with stock game and the new
   branch's DLLs/INI. Verify intro movies, menu, keyboard/mouse,
   gameplay, and a clean normal exit **without using Alt+Tab**. If the
   game does not start, stop and preserve the log: do not try Wine yet.
2. Alt+Tab from menu and gameplay: note whether the visible game
   simulation or animation really stops while another window has focus,
   how quickly, and whether the keyboard/mouse become usable on return.
3. Repeat with `ExperimentalFocusEventPause=0` on identical DLLs and
   game data. This is a clean A/B control for our WndProc changes.
4. Repeat on **Wine 11.17/X11**, same configuration and real game;
   run via Lutris with `winmm=n,b`.
5. For each platform, save a separate `SpiderMan2k1Fix.log` and
   report approximate observed pause lag, Alt+Tab input recovery lag,
   audio/movie behavior, and clean exit. Logs may disclose local paths.

Expected when event observer installs:

```text
FOCUS-EVENT: PROTOTYPE enabled (Win32 message observer, next-Flip gate, NOT an immediate engine pause)
FOCUS-EVENT: observer attached (no DirectInput interception)
FOCUS-EVENT: WM_ACTIVATEAPP(FALSE) elapsed-ms=0 ...
FOCUS-EVENT: GATE ENTER: time-from-focus-loss elapsed-ms=N ...
FOCUS-EVENT: WM_ACTIVATEAPP(TRUE) elapsed-ms=M ...
FOCUS-EVENT: GATE EXIT: time-in-gate elapsed-ms=P ...
```

If `FOCUS-EVENT: GATE ENTER` follows `WM_ACTIVATEAPP(FALSE)`
by >100 ms (or no gate is logged for several seconds), the Flip
boundary is **not** prompt enough for an engine-wide pause.
`GetTickCount` gives millisecond granularity but does not measure
the actual moment the X11/window manager focus changed before Wine
delivered `WM_ACTIVATEAPP`.

Importantly, **a successful pause does not by itself fix
`DIERR_NOTACQUIRED` on focus return**. The game's keyboard/mouse
recovery remains a separate, cross-platform problem.

Do not merge this prototype or PR #5 without explicit approval and
native Windows plus Wine results.

---

legluondunet assisted by ChatGPT (OpenAI)
