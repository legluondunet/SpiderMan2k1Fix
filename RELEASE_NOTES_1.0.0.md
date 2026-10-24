# SpiderMan2k1Fix 1.0.0 — Release notes (candidate)

> **Status:** Release candidate prepared for Windows 11 and Linux/Wine
> regression testing. **Not published** until the maintainer explicitly
> approves the 1.0.0 tag and the release assets. This document describes
> the candidate; it is not proof of a completed stable release.

## First stable version

SpiderMan2k1Fix is an unofficial compatibility patch for the **original
Spider-Man (2001) Windows PC release**. It leaves the proprietary
`SpideyPC.exe` and game data unchanged on disk. The fix consists of two
Windows 32-bit DLLs loaded through a local `winmm.dll` proxy.

This version consolidates the permanent compatibility fixes developed and
tested on native Windows 11 and Linux/Wine. Its focus is **preserving the
original game's behavior** on modern systems, with a few quality-of-life
additions.

### Key features

- In-memory **No-CD** bypass restricted to the exact supported retail
  executable, guarded by executable hash and original code signatures.
- **Borderless fullscreen** at the game window's monitor resolution:
  no original exclusive fullscreen/16-bit mode switching. Automatic
  widescreen 3D with aspect-aware FOV, and a selectable centered classic
  4:3 mode. Original videos, main menus and 2D artworks retain their
  intended proportions.
- Widescreen corrections for player HUD, compass, objectives, boss bars
  and the native ESC pause/quit menu on verified resolutions, including
  1920×1080 and 1920×1200.
- Configurable **FPS limiter**, default **30 FPS**. Certain original
  timed events may require 20 FPS: this is still a player-controlled
  setting, **not** an automatic level-specific frame-rate switch.
- **Alt+Tab stability** with input recovery and full focus-loss pause,
  including coordinated native Bink video/music and DirectSound
  secondary-voice pause and resume, with an any-key wake gesture.
- Preserved player keyboard/gamepad/audio/brightness settings and
  automatic layout support for French AZERTY and other supported OS
  keyboard layouts; compatibility with unsupported layouts is not
  guaranteed.
- A compact `SpiderMan2k1Fix.ini` with automatic safe recovery from
  obsolete/invalid settings and one rolling `.ini.old` backup.
- **F9 PNG screenshots**, stored alongside the game's executable.
- Source-derived version label **`v1.0.0-<commit>`** in the in-game
  menu and debug logs; the suffix identifies the precise binary build.

### 1.0.0 stabilization

- Remove the **obsolete, uncompiled Win32 popup** implementations
  `pause_overlay.c/.h` and `pause_dim.c/.h`. They are superseded by
  the DirectDraw-surface `pause_surface.c`; their historical code remains
  in Git history and archived technical notes.
- Preserve all active DirectDraw, Direct3D7, DirectInput, Bink,
  DirectSound, No-CD, INI recovery, HUD and native viewport hooks.
- Retain ABI and packaging guards: **32-bit PE**, MSVCRT (not UCRT),
  static libgcc, expected winmm exports, and the early
  `SpiderMan2k1Fix_DirectDrawGate` entry.
- Review high-risk code and limit release cleanup to operations that
  do not alter native hook timing or game memory ownership.

## Requirements and installation

- Own a legitimate copy of the original game and use **only**
  `SpideyPC.exe` SHA-256:
  `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
- Extract **`winmm.dll` and `SpiderMan2k1Fix.dll`** beside
  `SpideyPC.exe`, together with the existing game data.
- On Windows, run the original executable.
- On Wine, from the game folder:
  `WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe`.
  A 32-bit-capable Wine installation is required.
- The patch generates `SpiderMan2k1Fix.ini` on first launch.
  Do not replace or manually edit the original game's progress-save
  `save/SPIDRMAN.DAT`.

## Compatibility and limitations

- The build **does not** support alternate/modified game executables.
- 30 FPS is the default rather than a universal fix for every
  timing-sensitive boss/level. Players can select another FPS limit,
  including 20, from the INI.
- Widescreen/HUD support has been observed on selected resolutions; all
  monitors, DPI configurations, drivers and gameplay scenes have **not**
  been exhaustively validated.
- Original sound mix anomalies and game-progress save corruption risks
  have **not** been fixed or claimed fixed by this release.
- The F10 development/debug shortcuts are *not* part of the advertised
  standard gameplay features and remain disabled by default.
- The `winmm.dll` proxy conflicts with other mods that need the same
  filename unless explicitly chained (not provided by this project).
- GitHub Actions regression tests and Windows x86 DLL compilation do
  not replace actual Windows/Wine game testing.

## Release acceptance checklist

- [ ] Latest 1.0.0 candidate GitHub Actions: all tests pass and the
      **two DLLs** compile as PE32/MSVCRT with the right exports.
- [ ] Windows 11 full smoke: original intros, new game, menu, real 3D
      gameplay, native ESC pause/quit, return to menu, normal exit.
- [ ] Linux/Wine smoke: repeat menu, game, F9 PNG, Alt+Tab pause/resume,
      input and soundtrack restoration.
- [ ] Auto widescreen (1920×1080, 1920×1200 where available) and classic
      4:3; confirm no 640×480 exclusive-mode switch and no stretched 2D.
- [ ] Verify No-CD start with the **supported hash only**, 30 FPS default,
      custom 20 FPS, INI preservation and recovery, volume and controls.
- [ ] Confirm existing game-progress file remains intact and can still
      load/save using the **original game's controls**.
- [ ] Confirm tagged build logs/menu show **`v1.0.0-<commit>`**.
- [ ] Maintainer explicitly approves PR, tag `v1.0.0` and publication.

## Publishing (manual; intentionally not automated)

1. Approve/merge the stabilization PR **only after validation**.
2. Tag the exact reviewed `main` commit as `v1.0.0`.
3. Build both DLLs **from that tag**, verify the embedded version and
   commit, and package only `SpiderMan2k1Fix.dll`, `winmm.dll`,
   `README.md` and `CHANGELOG.md` in
   `SpiderMan2k1Fix-1.0.0-Windows-x86.zip`.
4. Verify the ZIP contents, SHA-256 and DLL exports, upload the ZIP to
   GitHub Releases and use these notes for the release description.
5. Only *after* publication, update user-facing links to the tagged
   release if desired. No release tag or GitHub Release is created by
   this document or PR.

## Credits

Developed by **legluondunet assisted by ChatGPT (OpenAI)**.
Original game and third-party rights remain with their respective owners.
Special thanks to krystalgamer for publicly available research into
the original PC game and disc check.
