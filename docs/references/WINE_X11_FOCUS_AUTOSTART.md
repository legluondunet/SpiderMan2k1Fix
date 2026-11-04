# Experimental integrated focus backend (Windows and Wine/X11)

**Draft only.** This change builds on the manually tested Wine/X11 bridge
from PR #102, without modifying it.

## User experience

- **Windows:** Only the pre-existing Win32 focus detector runs.
  The native Linux file in the ZIP is inert. No Wine imports are
  linked directly by the Windows DLL.
- **Wine/Proton under X11:** On the game's first regular Flip,
  the DLL detects Wine by a dynamically resolved
  `ntdll!wine_get_version`. If the native helper is installed,
  `kernel32!wine_get_unix_file_name` converts absolute PE paths
  to absolute Linux paths. `ntdll!__wine_unix_spawnvp` starts the
  native observer as a detached Linux process. The bridge remains
  Win32-only until the observer writes a **fresh, well-formed GAME or
  OTHER** sample newer than its spawn time, verified nonblockingly
  on later game Flip calls. If no valid sample arrives within 4
  seconds, automatic bridge activation is abandoned for that run.
- **Without Linux helper, working X11 or supported Wine Unix APIs:**
  Existing Win32 logic remains active, with its known Wine limitations.
  Merely creating the Linux subprocess never activates the bridge.
- There is **no manual terminal command, no xdotool dependency, no
  manual environment variable, no Lutris-specific code**.
- The helper is the only platform-specific component. PAUSED, its
  150ms debounce, Bink/DirectSound suspension and keyboard/gamepad
  resume stay in the same existing Windows DLL.

### Install tree

```text
SpideyPC.exe
winmm.dll
SpiderMan2k1Fix/
  SpiderMan2k1Fix.dll
  SDL3.dll
  linux/
    spiderman-focus-helper
```

The native `spiderman-focus-helper` is **ELF 64-bit** linked against
`libX11.so.6`. The 32-bit PE game can launch this host-native 64-bit
program via Wine, so no Linux 32-bit X11 dependencies are necessary.
Manjaro's current x86_64 libX11 is a representative target. Builds
on Ubuntu 24.04 may not run on older distributions with an older
glibc; this is an experiment, not a released compatibility guarantee.

The Windows x86 artifact strips POSIX execute bits when downloaded
from GitHub Actions. Before first launch, the DLL invokes
`/usr/bin/chmod u+x -- <exact helper path>` through Wine's native
`__wine_unix_spawnvp` with an **argv array, never a shell**. Failure
falls back to Win32 focus.

The native helper searches `/proc` for exactly one running
`SpideyPC.exe` Linux PID, reads X11 focus using `XGetInputFocus`
and `_NET_WM_PID` on the focused window (or parents), and atomically
writes `GAME`, `OTHER`, or `UNKNOWN` every 200ms to the existing
bridge state file beside `SpideyPC.exe`. It never injects input or
activates windows. On game exit it terminates and removes its file.
Multiple game processes produce `UNKNOWN` and do not trigger the
X11-specific pause.

The Win32 DLL rejects states older than 1200ms; this retains the
exact on-Flip X11 detection validated with the older shell helper.

### Safety and manual override

- `SPIDERMAN2K1FIX_X11_BRIDGE=0` explicitly disables the integrated
  helper and restores Win32-only behavior for A/B testing.
- `SPIDERMAN2K1FIX_X11_BRIDGE=1` retains the manually tested
  PR #102 host-helper mode (the integrated helper is **not**
  launched as well).
- If the native helper dies (SIGTERM/SIGKILL) or its heartbeat becomes
  stale, **never automatically resume** the game: Wine's foreground
  state may still be wrong, so that would resume videos, gameplay and
  sound *behind* Firefox. Instead, the DLL attempts up to two bounded
  native helper restarts (via Wine's argv-only spawn), polling even while
  the game is inside its blocking PAUSED loop. Each replacement must
  publish a fresh, valid X11 sample. PAUSED then exits only through the
  existing explicit key/gamepad press-release after `GAME` focus.
  Fresh `UNKNOWN` is not an observer outage and does not spawn a
  duplicate helper.
- If both restart attempts fail while PAUSED, do **not** resume the
  game automatically and never trust a permanently stale Wine foreground
  value by itself. The game can instead recover after an **observed
  Win32 external → game focus cycle after the terminal host failure**:
  the Win32 foreground, active window and focused window must all be
  the game, continuously for 150ms, before the usual explicit
  key/gamepad press-release is accepted. This is a conservative
  emergency fallback with weaker guarantees than working native X11.
  It must be tested against Firefox keyboard interception and repeated
  Alt+Tab; if no new Win32 external→game cycle occurs, the game
  remains paused and can be terminated using desktop process controls.
  No automatic media/gameplay resume is permitted.
- If Wine does not expose one of the three dynamically loaded
  functions, or path conversion/launch fails, no native process is
  started and Windows behavior is unchanged.
- **Wayland and XWayland sessions are conservatively disabled.**
  The Win32 launcher checks `XDG_SESSION_TYPE=wayland` and, when the
  session type is unavailable, `WAYLAND_DISPLAY`. The native ELF
  independently rejects the same environment before publishing any
  focus state. This avoids treating XWayland's local input focus as
  authoritative for unrelated native Wayland applications. On failure
  to detect Wayland from inside Wine, the fresh X11 handshake still
  prevents activating a bridge that never delivered data.

### ABI review

A direct Unixlib pair requires a Wine-recognized builtin PE DLL
and Wine's private Unixlib call conventions. This project ships an
ordinary Windows PE DLL, so bolting a Unixlib onto it would add
runtime and packaging fragility. Instead, the prototype uses two
well-defined Wine extensions that were present in Wine 11.0 and
an independent native process, while importing none of them at
Windows DLL load time.

These Wine interfaces are **not** Windows APIs. They may change
between Wine releases and must remain dynamically resolved with a
safe fallback.

### Acceptance gates before merge

1. CI build Windows x86 DLL, ELF64 Linux helper, complete flat
   installation artifact and release ZIP inventory.
2. Windows x86 static guards and Python regression tests. The CI
   additionally launches an **actual Xvfb server** with a mock
   `SpideyPC.exe` Linux process and checks `GAME`, `OTHER`,
   `UNKNOWN`, rejection under Wayland even with `DISPLAY`, helper
   termination cleanup, and automatic cleanup on game exit.
3. On Wine/GE-Proton, install unmodified ZIP **without environment
   variables** and without running `tools/spiderman-x11-focus-bridge.sh`.
4. Confirm logs show `FOCUS-X11-AUTO: native X11 helper spawned
   automatically` and `FOCUS-X11: host-focus-state=GAME`, `OTHER`.
5. Repeat the 3D Alt+Tab scenario that previously missed focus,
   including Firefox typing while the game shows PAUSED, then manual
   keyboard/gamepad wake.
6. Repeat all four opening Bink videos and verify pause timing,
   sound restoration and safe return.
7. Terminate the observer with SIGTERM while PAUSED and verify
   `FOCUS-X11-AUTO: observer heartbeat lost; native restart 1/2`,
   `fresh native X11 state verified`, then **no unpause until the user
   returns to GAME and explicitly wakes**. Ensure no repeating
   pause/resume loop and no Firefox keystroke interception. Repeat
   while in 3D and Bink, then with SIGKILL (stale file).
   If the helper disappears again after attempt 2/2, Win32 must
   report an external state and then a fresh return to GAME for at
   least 150ms, followed by a **manual key**. Neither a stale Wine
   foreground nor switching to Firefox alone should unblock PAUSED.
   No automatic wake or background audio is permitted.
8. On real Windows, confirm no helper spawn, no new dependency and
   normal Alt+Tab behavior.
9. The obsolete #100 timer experiment and #101 deferred-return
   experiment must not be incorporated into the consolidated build.
   The native X11 observer + Win32 fallback are the maintained paths;
   native Windows and Wayland behavior still require runtime validation.

legluondunet assisted by ChatGPT (OpenAI)
