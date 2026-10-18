# Changelog

## 1.0.0 — first stable source release (2026-10-18)

- Retained the validated borderless fullscreen, desktop resolution, Auto/4:3 presentation, intro-video and comic artwork fixes, automatic FOV, No-CD patch, 30 FPS limiter, Alt+Tab input recovery, and integrated game settings.
- Retained automatic INI creation and numbered `.old` backups with recovery of known valid values.
- Default logging is disabled (`[Debug] Log=0`) and diagnostic string formatting on key graphics hot paths is skipped when logging is off.
- Removed obsolete sprite-entry trampoline and 31-callsite profiling, geometry/draw histograms, drawing-phase shortcuts, comic and intro-video tracing, and forwarding-only Lock, BltFast, projection and vertex-buffer diagnostic hooks.
- Removed unverified native-edge experiments: raster bias, culling/Z A/B tests, pixel readbacks, synthetic/original triangle witnesses, diagnostic Clear and EndScene hooks.
- Removed the corresponding obsolete INI keys from the validation allowlist. Existing invalid/obsolete keys are handled by the existing INI auto-repair and backed up.
- Added release-specific regression checks for hook preservation and removal of obsolete diagnostic paths.
- No alteration was made to the SHA-256 executable guard or to the existing virtual configuration and input recovery implementations.

The cleaned 1.0.0 source has been compiled and validated in-game by the maintainer. No measured FPS performance claim is made.