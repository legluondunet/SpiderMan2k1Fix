# Changelog

## Unreleased — experimental controller and focus integration (draft PR #91)

**Not released or merged.** The following summarizes the **combined
test branch** only; the released v1.1.0 binary and its defaults remain
unchanged. Scope and final release version require maintainer approval.

### Tested changes in the combined branch

- **Sony DualShock 4 and Xbox 360-compatible gamepads:** Optional SDL3
  selection and handoff to the original DirectInput game controller,
  controller-specific profiles, menu/video Cross/A confirmation and
  Circle/B cancellation, native 3D controls and guarded Xbox RT Swing.
  The supported DS4 `054C:09CC` and Xbox `045E:028E` controllers were
  tested individually and together on Windows 11. Wine/Proton trials
  also exercised Xbox 3D gameplay and controller-backed focus wake.
  **These controller features still require experimental INI opt-ins.**
  Generic DS3, Steam Deck and arbitrary controllers are not validated.
- **Permanent game-only mouse disable:** Original mouse cursor, menu
  hit-testing and mouse polling are OFF even if an old INI contains
  `MouseEnabled=1`; keyboard/controller navigation remains available.
  The operating-system desktop pointer is not disabled and is
  restored when switching away from the game.
- **Opt-in Left Alt recovery after Alt+Tab:** Strictly guarded,
  default-OFF `[Input] ExperimentalAltLatchRecovery=1` repairs a
  stale *original game* Left Alt latch after a confirmed external
  focus-return; it does not rewrite real keyboard state or synthesize
  global Alt/Tab keys. The combined native Windows 11 test logged
  **six actual `0x7F->0x80` repairs** and no stuck keys reported.
- **Focus pause and media stability:** The combined Windows 11 test
  completed **9/9 background PAUSED/resume cycles**, restored **37/37
  active DirectSound voices** and recorded seven successful Bink
  pause/resume pairs, supported by the maintainer's **20/20 physical
  checks**. Under Wine/Proton, the background PAUSED ribbon may be
  delayed until a video transition or late/absent in 3D. This is an
  **accepted limitation only when game controls, rendering and audio
  resume without functional regressions**.
- **Configuration and diagnostics:** Obsolete mouse keys are recognized
  for migration but ignored at runtime. An INI auto-repair triggered
  by **two duplicate `MouseEnabled` entries** was correctly diagnosed
  and the regenerated file was subsequently validated. Detailed
  controller, native input and focus telemetry remains behind opt-in
  debug settings.
- **Documentation:** Updated installation/gamepad information and
  a player-focused FAQ separating intentional lack of game cursor
  from a missing desktop pointer, and distinguishing game-audio faults.
  Installation now packages the official Windows x86 SDL3 runtime
  under the patch subdirectory.

### Release and research boundaries

The combined experimental build has passed **741 regression tests and
Windows x86 CI packaging**, plus Windows 11 and Wine/Proton game tests
on the reported hardware. **No release is authorized by these results
alone:** direct integration onto the newer `main`, final default
settings, final-build smoke tests and an approved version/tag are
separate steps. The separate DS4 audio-routing PR #86, Xbox profile
prototype #78, and generic controller research remain outside the
tested-release claims.

For full source-linked evidence see
[combined hardware acceptance](docs/references/INTEGRATION_XBOX_MOUSE_ALT_FOCUS_ACCEPTANCE.md)
and the [release-readiness audit](docs/references/INTEGRATION_RELEASE_READINESS_20261101.md).
The complete superseded development diary (including experimental
hypotheses and earlier unvalidated statements) is preserved
[verbatim in the historical changelog archive](docs/references/CHANGELOG_EXPERIMENT_HISTORY_PRE_20261101.md).

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
