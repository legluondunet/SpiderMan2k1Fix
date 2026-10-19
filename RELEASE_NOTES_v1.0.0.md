# SpiderMan2k1Fix v1.0.0

First stable release of **SpiderMan2k1Fix**, an unofficial compatibility, widescreen and quality-of-life fix for the original Windows PC version of *Spider-Man (2001)*.

## Features
- Borderless fullscreen at native desktop resolution, with widescreen or original 4:3 gameplay.
- Automatic widescreen FOV adjustment, corrected introductory videos and comic-book artwork.
- Existing HUD aspect-ratio protections (fully adaptive HUD remains future work).
- Alt+Tab keyboard and mouse input recovery.
- Integrated `SpiderMan2k1Fix.ini` configuration instead of the external `Spidey.cfg` file.
- No-CD memory patch for the verified original executable, without modifying it on disk.
- Configurable 30 FPS cap, joystick and keyboard mapping, audio settings, automatic INI creation/repair.
- Debug log disabled by default (`Log=0`).

## Download and installation
Download **`SpiderMan2k1Fix-1.0.0-Windows-x86.zip`** from the assets below (not the auto-generated *Source code* archive). Extract **`winmm.dll`** and **`SpiderMan2k1Fix.dll`** into the folder containing **`SpideyPC.exe`**. The included `SpiderMan2k1Fix.ini` provides the default settings; the game can also generate it automatically.

**Linux / Wine:** Use the same two Windows DLLs and set a native-then-builtin override for `winmm`, for example:

```bash
WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
```

If upgrading from earlier SpiderManFix betas, remove the obsolete `SpiderManFix.dll` and rename your customized `SpiderManFix.ini` to `SpiderMan2k1Fix.ini` (keep a backup).

The game is **not included**. The patch is designed for the original 32-bit `SpideyPC.exe` with SHA-256:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

See the [README](https://github.com/legluondunet/SpiderMan2k1Fix#readme) for full installation, compatibility notes, options and known limitations.

**Validation:** Maintainer-tested in-game under Wine; 55 automated tests and Windows x86 / MSVCRT build checks passed.

*This is an unofficial fan-made compatibility patch. No copyrighted game data is distributed.*
