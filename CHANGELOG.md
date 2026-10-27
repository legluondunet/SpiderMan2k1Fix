# Changelog

## 1.1.0 — October 27, 2026

- Identified the root cause of audio crackling in the original game: its native WAV loader reads only complete 2,048-byte PKR resource sectors and omits the final partial sector.
- Added a **signature-guarded native loader correction** that restores only the missing RIFF/WAV remainder after matching the entire loaded prefix and verifying the original sources independently by CRC32.
- Validates each additional read through the original native PKR reader in a private 2,048-byte scratch buffer before writing only the exact missing remainder to the game's preallocated WAV buffer.
- Safely handles **duplicate original WAV sources** only if every matching source has identical remaining bytes. Leaves metadata-only remainders, ambiguous or unverified assets unchanged.
- Retained the game's original DirectSound playback engine and the separate opt-in two-sample `Heli1.wav` declick; removed the obsolete late DirectSound tail repair and its `ExperimentalAudioPKRTailRestore` INI option.
- Kept bounded read-only source/PCM diagnostics and optional DirectSound Play tracing; no SDL3 backend or external audio assets are required.
- **Linux/Wine validation (27 October 2026):** confirmed native repair of the previously ambiguous two 460-byte and one 1,988-byte PCM tails, unchanged full-PCM comparison results, and user-confirmed crackle-free playback **without** the fallback.
- **Enabled the native WAV loader correction by default** for the supported executable, without adding a setting to the generated INI. An existing manually configured `ExperimentalAudioLoaderTail=0` remains a hidden troubleshooting override; `=1` is no longer necessary.
- Preserved the original SHA-256-bound WAV/DirectSound function addresses, disassembly, failure mechanism and rejected hypotheses in the permanent technical research documentation (PR #71).
- **Verification:** Linux/Wine gameplay and listening confirmed no audible crackling with the late DirectSound fallback removed; Windows x86 compilation and static regression tests passed. Native Windows gameplay verification of the final default-on build remains pending.
- **Release:** minor version 1.1.0 introduces the built-in audio repair; only the supported original `SpideyPC.exe` is compatible.

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
