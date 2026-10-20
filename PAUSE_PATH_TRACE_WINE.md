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
```

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

- `PAUSE-PATH: background Flip pause-check ENTER` marks reaching the
  existing Flip pause boundary while Spider-Man is out of focus.
- `PAUSE-PATH: Flip pause-check EXIT elapsed-ms=...` measures time spent
  inside **the selected existing pause handler**.
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
