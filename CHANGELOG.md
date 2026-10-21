# Changelog

## Unreleased — dimmed pause scene (PR #12)

- Add a 40%-opaque black tint across the *game client area* only during a confirmed focus pause. The underlying scene remains visible; the previous white comic-style `PAUSE` message with red frame stays centered above it at full brightness.
- Native log exposed repeated `FOCUS-DIM: creation failed` from the layered child. Replace it with a **non-topmost owned layered popup**, supported for unmanifested 2001 executables, and exclude the validated shaped child banner's *exact* red/white pixels from the popup region. The popup uses constant alpha (`LWA_ALPHA`), not the previously unreliable color key, and is skipped if creation, region masking or alpha setup fails. Each failure now logs its Win32 code.
- Add `[Input] PauseDim=1` (default on for active focus pauses; `0` disables only dimming). Focus pause itself remains opt-in. Destroy dim child on explicit resume and `WM_QUIT`, before DirectSound/Bink resume; no changes to input hooks, audio, timing, rendering surfaces or the game EXE.
- Runtime visual validation is required on native Windows 11 and DirectDraw; CI alone does not prove transparent compositor behavior.

## Unreleased — Windows Ghost-window false-focus experiment (PR #11)

- Diagnose repeated spontaneous 5–6-second menu/cinematic/3D pauses: external foreground window `class=Ghost` with the same helper PID/thread across all occurrences, indicating Windows' unresponsive-window surrogate, not a verified user Alt+Tab.
- Add `[Input] DisableWindowsGhosting=1` **opt-in** (default 0): calls Microsoft's `DisableProcessWindowsGhosting` after supported EXE/INI validation, before first game window; the change lasts for the process lifetime and removes the Windows ghost-window fallback.
- Preserve real focus-loss pausing, Any-key wake, Bink and DirectSound synchronization, windowed rendering, and diagnostic foreground-owner logs. Keep Windows' normal ghost-window behavior by default pending tests.
- Add regression tests, diagnostics and rollback instructions; this is a test experiment, **not yet a verified release fix**.

## Unreleased — centered transparent focus pause indicator

- Display `PAUSE` in the center of the game window, inside a red outline with no filled background and comic-inspired bold italic white text with a dark outline, while `PauseOnFocusLoss` is latched. Use a shaped child-window region to exclude all empty pixels (avoiding the black interior observed with DirectDraw and layered-window color keying).
- Display the appropriate French wake hint based on `PauseKey`: any key, Space, or F9.
- Draw the indicator in a nonactivating child window (no topmost desktop window, no replacement of the game's DirectDraw/3D drawing methods).
- Automatically hide it before Bink and DirectSound playback resumes and if the game quits while paused.
- Add `[Input] PauseOverlay=1` (default on for visible focus pauses; configurable to `0` for visual rollback); no change to focus/input/media pause behavior.

## Unreleased — synchronized focus pause for Bink and DirectSound (PR #9)

- Extend the optional Alt+Tab focus pause with `[Input] PauseBinkOnFocusLoss=1`: invoke the game's own Bink pause/resume mechanism for cinematics and audio-only Bink music.
- Add `[Input] PauseDirectSoundOnFocusLoss=1`: stop only active voices in the original game's 32-slot secondary DirectSound table; resume the same buffers with their original looping state and preserved playback position.
- Keep the two new options disabled by default and preserve the existing `DirectInputReacquire`, `PauseOnFocusLoss`, `PauseKey=Any`, and cursor-recovery controls.
- Preserve exact-executable signature guards, avoid COM vtable interception, update configuration templates, tests, and rollback documentation.
- **Windows 11 validation:** tester reports successful menu/cinematic/3D pausing with no audio desynchronization. In the recorded session, 11 and 9 playing secondary voices were stopped and successfully resumed on separate 3D focus pauses.
- **Known observation:** intermittent audio crackling occurred under ProcDump but was not reproduced on a normal game launch without ProcDump. Cause not established; investigation deferred.
- Not yet merged or released; final integration requires explicit approval.

## 1.0.0 — first stable release (2026-10-19)

- Renamed the project, main DLL, INI, logs, source file, build artifacts, and documentation to **SpiderMan2k1Fix**. The `winmm.dll` proxy retains its required name. Existing beta INI files need to be renamed manually to preserve custom settings.

- Retained the validated borderless fullscreen, desktop resolution, Auto/4:3 presentation, intro-video and comic artwork fixes, automatic FOV, No-CD patch, 30 FPS limiter, Alt+Tab input recovery, and integrated game settings.
- Retained automatic INI creation and numbered `.old` backups with recovery of known valid values.
- Default logging is disabled (`[Debug] Log=0`) and diagnostic string formatting on key graphics hot paths is skipped when logging is off.
- Removed obsolete sprite-entry trampoline and 31-callsite profiling, geometry/draw histograms, drawing-phase shortcuts, comic and intro-video tracing, and forwarding-only Lock, BltFast, projection and vertex-buffer diagnostic hooks.
- Removed unverified native-edge experiments: raster bias, culling/Z A/B tests, pixel readbacks, synthetic/original triangle witnesses, diagnostic Clear and EndScene hooks.
- Removed the corresponding obsolete INI keys from the validation allowlist. Existing invalid/obsolete keys are handled by the existing INI auto-repair and backed up.
- Added release-specific regression checks for hook preservation and removal of obsolete diagnostic paths.
- No alteration was made to the SHA-256 executable guard or to the existing virtual configuration and input recovery implementations.

The cleaned 1.0.0 source has been compiled and validated in-game by the maintainer. No measured FPS performance claim is made.
