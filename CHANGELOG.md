# Changelog

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
