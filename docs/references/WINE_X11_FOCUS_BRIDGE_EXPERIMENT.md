# Experimental Wine/X11 native focus bridge

**Never enable by default.** This is a targeted opt-in experiment for
Manjaro/Xfce X11 and Wine/GE-Proton, not a Windows requirement.

## Confirmed root cause from synchronized 2026-11-04 host + game traces

`spiderman-alt-tab-x11-v2-20261104-101334.log` monitors the real Linux
`SpideyPC.exe` process, PID 1051496 (not `umu-run`).
`SpiderMan2k1Fix(20261104-091746).log` is the matching Wine log.

- At 10:14:14.682, X11 focuses Firefox; the Win32 game receives
  `WM_ACTIVATEAPP(FALSE)` only at 10:14:17.472. PAUSED is presented
  at 10:14:17.661, and the user returns at 10:16:16.
- At **10:16:30.893**, X11 focuses Firefox again. X11 continues to
  report Firefox until **10:17:27.991**, almost 57 seconds later.
  Throughout this period, Spider-Man continues presenting regular
  `Flip` frames (for example 10:16:59 and 10:17:25), but Win32's
  `GetForegroundWindow()` STILL reports the game HWND `00020088`
  and `fg-is-game=1`.
- There is **no** `WM_ACTIVATEAPP(FALSE)` at 10:16:30.
- A different window deactivation is logged at 10:17:33.110,
  after X11 had already returned to the game; it must not be
  conflated with the missed second Alt+Tab.
- The Linux process PID remained alive and made CPU progress while
  Firefox was focused. The engine was not stuck during that second
  Alt+Tab; the Win32 focus signal was *stale*.

This is why a Win32-only timer or a deferred `WM_ACTIVATEAPP` latch
does not solve the missed second switch.

## Architecture (intentionally opt-in)

The native shell helper `tools/spiderman-x11-focus-bridge.sh` uses
`xdotool getwindowfocus getwindowpid`. It checks for **one** real Linux
`SpideyPC.exe` process and writes an atomic two-state file in the
**same directory as `SpideyPC.exe`** every 200ms:

- `SPIDEY_X11_FOCUS_V1 GAME` — keyboard focus belongs to the game PID;
- `SPIDEY_X11_FOCUS_V1 OTHER` — X11 focus belongs to another known PID;
- `SPIDEY_X11_FOCUS_V1 UNKNOWN` — unavailable PID; never trigger a pause.

The Win32 DLL reads the file only when environment variable
`SPIDERMAN2K1FIX_X11_BRIDGE=1`. Every sample must be newer than
1200ms; missing, malformed or stale content is UNKNOWN. No shell
command is invoked from the game or DLL. There is no new native DLL
dependency, synthetic input or off-thread DirectDraw access.

If X11 says OTHER for at least 150ms while Win32 still reports the
game HWND, the **existing same-thread Flip PAUSED mechanism** runs.
Normal Win32 external-window handling remains unchanged.

The temporary `WH_KEYBOARD_LL` hook must additionally ignore all
keys unless the host helper reports GAME. This prevents a held/stale
Win32 foreground HWND from stealing keys typed in Firefox while
PAUSED is on screen. SDL3/DI gamepad resume is gated similarly.

## Testing

1. Under a native **X11**, not a Wayland-only, desktop install
   `xdotool` if needed.
2. Build/download this PR's Windows x86 artifact. Do **not** enable
   `SPIDERMAN2K1FIX_FOCUS_TIMER` simultaneously.
3. In Lutris, set `SPIDERMAN2K1FIX_X11_BRIDGE=1`. Retain
   `SPIDERMAN2K1FIX_FOCUS_RETURN_LATCH=1` only when also testing
   the separate return fallback; it is not required for the bridge.
4. Start Spider-Man. On the Linux host, navigate to the directory
   containing `SpideyPC.exe` and run:
   `bash /path/to/spiderman-x11-focus-bridge.sh "$PWD"`.
   Keep this helper running until after the game exits.
5. Confirm `FOCUS-X11: opt-in Linux X11 bridge enabled`,
   `FOCUS-X11: host-focus-state=GAME` then `OTHER` on Alt+Tab,
   and `FOCUS-X11: confirmed native-X11 focus loss` in the log.
6. Repeat Alt+Tab twice from 3D and menus, stay in Firefox for 5–20s.
   **Type in Firefox while PAUSED**: nothing should be swallowed and
   no resume should occur. Return to Spider-Man, press and release
   Space; exactly one resume must occur.
7. Remove the environment variable and stop the helper: the stable
   pre-bridge behavior must be unchanged.
8. Real Windows testing of the surrounding focus hooks is still
   required before any default-on setting or merge.

Limitations: requires a separately running X11 host helper; cannot
discover a focus change without both a valid fresh bridge sample and
a continued DirectDraw Flip. If the game genuinely stops calling
Flip, the existing return-latch experiment addresses a different
case. Avoid guessing which failure occurred without paired logs.

legluondunet assisted by ChatGPT (OpenAI)
