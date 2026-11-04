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
  native observer as a detached Linux process.
- **Without Linux helper or X11:** Existing Win32 logic remains active,
  with its known Wine limitations. No hard failure.
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
- If the helper disappears or returns `UNKNOWN` for more than
  ~2.2 seconds **during a custom PAUSED wait**, the temporary
  keyboard hook is released via the existing pause-exit path to
  avoid trapping the user. Valid `OTHER` focus from Firefox never
  triggers this fallback and should not swallow Firefox keystrokes.
- If Wine does not expose one of the three dynamically loaded
  functions, or path conversion/launch fails, no native process is
  started and Windows behavior is unchanged.
- Wayland-only/XWayland behavior is **not certified**. With a valid
  `DISPLAY` but unavailable X11 window PID, the helper yields
  `UNKNOWN`, not a false focus loss.

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
2. Windows x86 static guards and Python regression tests.
3. On Wine/GE-Proton, install unmodified ZIP **without environment
   variables** and without running `tools/spiderman-x11-focus-bridge.sh`.
4. Confirm logs show `FOCUS-X11-AUTO: native X11 helper spawned
   automatically` and `FOCUS-X11: host-focus-state=GAME`, `OTHER`.
5. Repeat the 3D Alt+Tab scenario that previously missed focus,
   including Firefox typing while the game shows PAUSED, then manual
   keyboard/gamepad wake.
6. Repeat all four opening Bink videos and verify pause timing,
   sound restoration and safe return.
7. Simulate observer termination while PAUSED and confirm keyboard
   becomes available again. Repeat with `_NET_WM_PID` unavailable.
8. On real Windows, confirm no helper spawn, no new dependency and
   normal Alt+Tab behavior.
9. Only after those gates, cleanup obsolete branches #100/#101
   and discuss consolidation/merge with the maintainer.

legluondunet assisted by ChatGPT (OpenAI)
