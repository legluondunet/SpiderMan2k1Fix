# Passive focus-pause graphics-path trace (Windows 11 / Wine)

This is a **diagnostic**, NOT a new pause implementation. It is disabled
unless both `[Debug] Log=1` and the environment variable
`SPIDERMAN2K1FIX_PAUSE_PATH_TRACE=1` are present. The trace observes existing
DirectDraw/Direct3D7 calls that SpiderMan2k1Fix already handles; it does not
hook a new API, suspend threads, alter frame timing, change DirectInput, or
attempt to stop audio.

## Test configuration for Wine 11.17 / X11

Use the current, known-working two DLLs **from the passing diagnostic CI
artifact**, and keep the game's existing INI. Do **not** extract an old
default INI over your settings.

```ini
[Debug]
Log=1

[Input]
PauseOnFocusLoss=1
ExperimentalFocusEventPause=0
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
RestoreCursorOnFocusLoss=1
```

In **Lutris → Configure game → System options → Environment variables**, add:

```text
SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY=1
SPIDERMAN2K1FIX_PAUSE_PATH_TRACE=1
SPIDERMAN2K1FIX_EXTENDED_PRESENT_TRACE=1
```

The third variable enables **additional experimental COM wrappers** for otherwise
untraced `IDirectDrawSurface7` methods (`BltBatch`, `BltFast`, `GetDC`,
`ReleaseDC`, `Lock`, `Unlock`, and overlay operations), and alternative
`IDirect3DDevice7` primitive methods (`Draw*Strided`, `Draw*VB`, plus
`BeginScene`/`EndScene`). They forward all arguments and results to
the original methods without modification. They are not injected unless both
`SPIDERMAN2K1FIX_PAUSE_PATH_TRACE=1` and
`SPIDERMAN2K1FIX_EXTENDED_PRESENT_TRACE=1` are set with `[Debug] Log=1`.

**Experimental warning:** The extra wrappers modify the game's DirectDraw
cloned vtable and D3D7 native device vtable, and have not been validated
in live gameplay on native Windows. For normal or fallback testing, remove
`SPIDERMAN2K1FIX_EXTENDED_PRESENT_TRACE`. This keeps the already tested basic
diagnostic and DI8 input retry intact.

The first variable keeps the separately validated Wine input-retry experiment.
The second only enables passive logging. For native Windows validation,
do not enable the unvalidated input-retry environment variable.

## Three observations

Perform one run with a focus loss during each stage. Keep Spider-Man
in the background for at least 15 seconds; then return to it and quit
normally. Save each run's `SpiderMan2k1Fix.log` before launching the next
run (logging resets the file at each new launch).

1. **Opening video:** switch away *mid-video*, not between two videos.
   Does a second video begin while the game is in the background?
2. **Main menu:** switch away while the menu is visible.
3. **3D gameplay:** switch away while the character/world is moving.
   Does the world continue for 8–15 seconds or longer?

Send the three logs along with observed audio/video behavior.

## Reading the log

The new opt-in **25 ms independent foreground observer** starts after
DirectDraw window initialization, outside the game render thread:

- `PAUSE-PATH: FOCUS OBSERVER ready`: polling started.
- `PAUSE-PATH: FOCUS LOST tick=...`: first independently observed loss of foreground.
- `PAUSE-PATH: FOCUS BACKGROUND elapsed-ms=...`: snapshots near
  **1, 5, 10 and 20 seconds**, even when the game is not calling Flip.
  These include cumulative counts for the instrumented Blt and D3D draw APIs.
- `PAUSE-PATH: FOCUS RESTORED ... background-ms=...`: independent estimate of
  time out of focus.
- `PAUSE-PATH: background Flip pause-check ENTER ... focus-loss-to-gate-ms=N`:
  observed delay from focus loss to reaching the Flip pause boundary.
  `-1` means the observer had not measured that particular focus transition;
  it is **not** a measured zero or negative delay.

Because the observer polls every 25 ms, the delay has roughly 25 ms
sampling uncertainty (plus thread scheduling). It is a diagnostic-only
worker and **does not dispatch window messages, intercept DirectInput,
suspend gameplay or control audio**.

- `PAUSE-PATH: background Flip pause-check ENTER` marks reaching the
  existing Flip pause boundary while Spider-Man is out of focus.
- `PAUSE-PATH: Flip pause-check EXIT elapsed-ms=...` measures time spent
  inside **the selected existing pause handler**.
- `PAUSE-PATH: EXTENDED DX7 ... ENABLED` confirms extra API wrappers are active.
- `PAUSE-PATH: background BltFast`, `Lock`, `Unlock`, `UpdateOverlay`,
  `DrawPrimitiveVB`, `DrawPrimitiveStrided`, etc. indicate alternate
  rendering paths still called after focus loss.
- `PAUSE-PATH: EXTENDED SNAPSHOT` appears alongside the passive observer's
  1/5/10/20-second checkpoints. Counts remaining constant while the user
  *sees* motion implies either buffering, an untraced API or independently
  timed rendering/media; no single counter proves a visual frame was presented.
- `PAUSE-PATH: background Blt ... target=primary|back|other` indicates
  DirectDraw Blt calls while unfocused. Video-related full-frame Blts may
  use this API; *not every Blt is video presentation*.
- `PAUSE-PATH: background DrawPrimitive` or
  `DrawIndexedPrimitive` indicates that existing D3D7 paths continue
  executing while unfocused.

Background Blt/Draw lines are throttled to at most **one line per second
per API**, so their `count` values are cumulative. `flip` is the
most recently observed Flip count, not an independent wall clock.

The absence of a log line does **not** prove the corresponding engine
component is suspended: it may run through an unobserved API, and a Flip
may not occur for the entire video. Both pause implementations still
only wait at Flip; neither explicitly stops Bink decoding, sound, or
the game's simulation.

## Safety

- Diagnostic off by default; full behavior unchanged without the environment variable.
- To revert, remove `SPIDERMAN2K1FIX_PAUSE_PATH_TRACE` from Lutris.
- If the game becomes unstable, remove the diagnostic DLLs and restore
  the last successful pair.
- This is an **unmerged draft PR** pending Wine and native Windows checks.

legluondunet assisted by ChatGPT (OpenAI)

## Passive Win32 focus-message comparison (new opt-in diagnostic)

Set this additional environment variable **only for the comparison test**:

```text
SPIDERMAN2K1FIX_WIN32_FOCUS_MESSAGE_TRACE=1
```

It activates only if `[Debug] Log=1` and
`SPIDERMAN2K1FIX_PAUSE_PATH_TRACE=1` are also enabled.
Do not change `ExperimentalFocusEventPause=0` or
`ExperimentalAltTabReacquire=0` in the INI, and preserve
`SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY=1`.

These diagnostics install three **thread-local, pass-through Win32 hooks**
(`WH_CALLWNDPROC`, `WH_CALLWNDPROCRET`, and `WH_GETMESSAGE`) on
the game's GUI thread.
They log the focus messages `WM_ACTIVATEAPP`, `WM_KILLFOCUS`,
`WM_SETFOCUS`, `WM_ACTIVATE`, `WM_NCACTIVATE` before and after the
existing game window procedure. They do **not** replace/subclass that
WndProc, consume messages, or change focus, pause, rendering, input or audio.

Every five seconds, a separate observer records a
`WIN32-FOCUS: HEARTBEAT` line **even when Win32 still reports the game
as foreground**, including local wall-clock time, monotonic tick,
`GetForegroundWindow`, and the game GUI thread's
`GetGUIThreadInfo` active/focus windows and its success flag.

The same heartbeat now also reports cumulative **real message-retrieval**
counts (`dequeue-remove`, `dequeue-peek`, `dequeue-focus`) and the
number of messages reaching the game HWND (`window-delivery`), with
the last observed message ID and timestamps. `WH_GETMESSAGE` fires
when `GetMessage`/`PeekMessage` *actually retrieves* a queued message.
It **does not fire for empty-queue polls**. Therefore, increasing counts
prove the game's GUI thread is processing posted messages; flat counts
alone **cannot** distinguish a game thread polling an empty queue from
one that is not pumping messages at all. Also, messages delivered via
`SendMessage` may reach `WH_CALLWNDPROC` without a dequeue event.

In particular, if X11 reports Firefox as active for 10+ seconds while
Wine continues reporting `is-game=1`, a rising dequeue counter implies
the Win32 thread is handling *some* queued messages; we must still test
where focus notification was lost/delayed, not automatically blame Wine.
`WIN32-FOCUS: EVENT` entries also carry wall-clock time and Win32 tick.

On an X11 desktop, start `tools/spiderman-x11-focus-trace.sh` **before**
launching the game with your normal Wine command. The variable settings
must be prepended to that same command (or configured equivalently in its
environment); do not replace the user's custom Wine runner command.
Perform separate >10-second Alt+Tab intervals in two different cinematics,
return to the game, then stop the X11 logger with Ctrl+C. Save both
`SpiderMan2k1Fix.log` and the corresponding timestamped X11 trace.

**What to look for:** if X11 changes `_NET_ACTIVE_WINDOW` from a
`WM_CLASS=spideypc.exe` window to another app while Win32 heartbeat
still reports `is-game=1`, check whether `WIN32-FOCUS: EVENT`
logged any focus loss. The presence or absence of the Win32 event is
important evidence; a missing event in the log does not by itself prove
that Wine never generated it, because these hooks observe only messages
dispatched to the game's GUI thread.

**Safety:** These hooks are a new experimental diagnostic and have only
been built and source-tested, not tested live under Wine/Windows.
Back up both DLLs before testing. If the game freezes or crashes,
remove `SPIDERMAN2K1FIX_WIN32_FOCUS_MESSAGE_TRACE=1` first.
The prior background Flip pause and DI8-RETRY remain unchanged.
