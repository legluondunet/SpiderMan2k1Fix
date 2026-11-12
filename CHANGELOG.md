# Changelog

## Unreleased — v2.0.0-rc.2 (November 9, 2026)

**Integrated release candidate; not yet a published v2.0.0 stable release.**
This candidate combines merged Save Game confirmation PR #138, the
Pause/cutscene SDL3 trigger-drift correction from merged PR #139,
duplicate-instance selection from merged PR #137, and release metadata.
Exact RC2 package tests on Windows 11 and Wine/Proton are mandatory.
The previously published stable v1.1.0 remains available.

### Controls and SDL3

- SDL3 is the **only supported controller mapping backend**, enabled by default
  without player-facing experimental switches. Press **Start** on a mapped
  controller, then release controls to neutral before playing; keyboard
  fallback remains independent. Per-model profiles remain supported.
- Start selection is available from the **first safe foreground intro frame**.
  SDL3 handles menus, skippable Bink movies, gameplay, Pause and GAME OVER.
- **Save Game controller confirmation (merged PR #138):** South/Cross/A can
  accept a native save slot and its YES confirmation while the renderer
  still reports 3D; hardware-confirmed on GE-Proton (PS3) and Windows 11
  (Xbox 360).
- **Pause/cutscene rearm (merged PR #139, issue #60):** mapped controllers
  no longer remain disarmed after context transitions solely because of an
  LT/RT midrange value from 5000 to 10999. Trigger actions retain their original
  11000 press / 5000 release hysteresis and fresh Start/DirectInput ownership
  retains its strict neutral gate. The user confirmed improved behavior on
  Windows 10, with further RC2 regression testing required.
- **Same-model SDL3 pads (merged PR #137):** the universal keyboard
  bridge identifies connected instances independently even when other devices
  share the same VID:PID; competing simultaneous Start edges fail closed and
  native DirectInput keeps its strict unique-model safety policy.
  Two Xbox-compatible devices sharing `045E:028E` were tested with successful
  Start handoffs on Windows 11 (November 9) and Wine/GE-Proton (November 12,
  alongside a PS4 controller). Mirrored physical-device identities and
  deliberately simultaneous Start presses remain unconfirmed on hardware.
- Controller switching and unplug/replug remain guarded by neutral-first
  selection, foreground ownership, strict live SDL3 instance identity and
  the original game's separately preserved fatal-DirectInput-unplug safety hook.
  Unreachable legacy native reconnect code was retired.
- Remove obsolete `[Joystick]`, QuickExit F10/F12, native-owner and SDL3
  experimental settings from fresh INIs while preserving valid player
  preferences and the original 252-byte `[Internal] RawHex`; update explanatory
  comments and one-time migration of recognized older generated INIs.
- Built-in guarded Left Alt recovery and background focus pause protect
  keyboard/gamepad input; Bink and DirectSound media are synchronized.

### Graphics, compatibility and packaging

- Permanent source-verified widescreen HUD, centered 4:3 native Pause/ESC
  artwork, original veil/outer gutters and synchronized entry-iris curtains;
  original 4:3 rendering and HUD draw ordering are preserved.
- Keep the previously released No-CD, native resolution/borderless,
  30 FPS default limiter, original PNG capture and native WAV-loader
  corrections.
- Bundle the official unmodified SDL3 3.4.18 Windows x86 runtime, its license,
  `winmm.dll`, `SpiderMan2k1Fix.dll`, and the optional native Linux X11
  focus helper in a single directly extractable CI artifact.

### Verification scope and release gate

- **Accepted before RC:** Windows 11 native DS4 (`054C:09CC`) and Xbox 360
  (`045E:028E`), repeated Alt+Tab and controller changes, plus earlier
  Linux Wine/Proton controller/graphics regression sessions. The source
  tested as `v1.1.0-b66c6c4` was file-identical to final merged `main`
  at `c746080d`.
- **Required for final v2.0.0:** a green *exact RC2 commit* Windows x86 CI
  (tests, DLLs, SDL3, Linux helper and package), followed by the maintainer's
  real Windows and Wine smoke test of that downloaded RC.
- **Not yet claimed:** full Steam Deck/Steam Input compatibility, native
  analog-axis preservation, every SDL3 device, and native Windows PS3
  hardware. Limited Steam Deck hardware observations are tracked in issue #120
  for follow-up after v2.0.0; none imply complete compatibility.
- No automatic release or tag is created by the CI workflow. See
  [RC test plan](docs/references/RELEASE_2_0_RC_TEST_PLAN.md).
- Historical development reports and superseded experimental findings remain
  in the [research archive](docs/references/CHANGELOG_EXPERIMENT_HISTORY_PRE_20261101.md)
  and [integration acceptance](docs/references/INTEGRATION_XBOX_MOUSE_ALT_FOCUS_ACCEPTANCE.md).

## 1.1.0 — October 27, 2026

- Fixed crackling and distorted sound effects caused by a bug in the original game's audio loading.
- The audio fix is enabled automatically — no configuration or extra files required.

[Technical details and testing notes](docs/references/diagnostics/audio/PKR_NATIVE_AUDIO_REPAIR.md)

## 1.0.0 — October 25, 2026

**First stable release of SpiderMan2k1Fix**, an unofficial compatibility patch for *Spider-Man (2001)* PC.

### Compatibility and Display

- Added an in-memory **No-CD patch**, without modifying the original executable.
- Replaced obsolete exclusive fullscreen modes with **borderless fullscreen** at the monitor's native resolution.
- Added automatic widescreen support with aspect-aware FOV adjustment and an optional centered **4:3 mode**.
- Preserved the original aspect ratios of menus, introductory videos, cinematics, and comic-book artwork.
- Corrected widescreen HUD elements, including health and web gauges, compass, objectives, boss health bars, and the in-game pause menu.
- Added multi-monitor support and Windows DPI scaling compatibility.
- Restored the original game icon in the Windows taskbar.

### Gameplay and Input

- Added a configurable **FPS limiter**, set to **30 FPS by default**, with support for 20 FPS in timing-sensitive sections.
- Fixed crashes and permanent keyboard/mouse input loss associated with Alt+Tab.
- Added automatic pausing when the game loses focus, with synchronized video and audio playback and keyboard-controlled resumption.
- Added automatic keyboard-layout detection, including support for French AZERTY controls, menu labels, and cheat-code entry.

### Configuration

- Replaced the original `Spidey.cfg` with a unified **`SpiderMan2k1Fix.ini`** configuration file.
- Preserved keyboard, gamepad, audio, and brightness settings.
- Added automatic validation and recovery of invalid or obsolete settings, with a single rolling `.ini.old` backup.
- Added configurable aspect ratio, frame-rate limit, keyboard-layout detection, and diagnostic logging.

### Fixes and Improvements

- Fixed black screens caused by unsupported legacy display modes.
- Fixed a crash that could occur when exiting the game.
- Fixed desktop mouse cursor visibility issues after switching applications.
- Added **F9 lossless PNG screenshots**.
- Added the patch version and source commit identifier to the game's main menu and diagnostic logs.
- Improved startup reliability, display initialization, and configuration recovery.
- Removed obsolete experimental code and development-only features from the stable build.

### Known Limitations

- Some bosses and levels still require a **20 FPS limit** to function correctly.
- Audio glitches inherited from the original game remain unresolved.
- Under Wine, automatic pausing after focus loss may occasionally be delayed.
- Only the supported original `SpideyPC.exe` executable is compatible with this release.

See the [README](README.md) for installation instructions, configuration, known issues, and possible future improvements.

### Release

[SpiderMan2k1Fix v1.0.0 — GitHub Releases](https://github.com/legluondunet/SpiderMan2k1Fix/releases/tag/v1.0.0)
