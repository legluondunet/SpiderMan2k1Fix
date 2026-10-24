# SpiderMan2k1Fix

**A modern compatibility and widescreen fix for Spider-Man (2001) on PC — Windows and Linux (Wine).**

[![Windows x86 build](https://github.com/legluondunet/SpiderMan2k1Fix/actions/workflows/spiderman2k1fix-windows-x86.yml/badge.svg)](https://github.com/legluondunet/SpiderMan2k1Fix/actions/workflows/spiderman2k1fix-windows-x86.yml)
[![Linux / Wine / Proton compatible](https://img.shields.io/badge/Linux%20%2F%20Wine%20%2F%20Proton-compatible-428b40?logo=linux&logoColor=white)](#installing-on-linux-with-wine)

### Screenshots

<p align="center">
  <a href="assets/screenshots/main-menu.webp"><img src="assets/screenshots/main-menu.webp" alt="Spider-Man main menu, centered 4:3" width="49%"></a>
  <a href="assets/screenshots/gameplay.webp"><img src="assets/screenshots/gameplay.webp" alt="Spider-Man widescreen gameplay with HUD" width="49%"></a>
</p>

SpiderMan2k1Fix is an **unofficial, fan-made patch** for the original Windows PC release of *Spider-Man*. It helps the game run more comfortably on modern displays and operating systems while preserving its original look when desired.

It adds borderless fullscreen, desktop-resolution output, widescreen support, corrected presentation for introductory movies and comic-book artwork, improved input handling, and an integrated configuration file. It also removes the game's CD check for the supported executable.

**License:** The SpiderMan2k1Fix source code is distributed under the [GNU General Public License, version 3](LICENSE) (**GPL-3.0-only**). This license covers the project's own code, **not** the original game or its assets.

**Original game required:** SpiderMan2k1Fix is a compatibility patch, not a remake or a replacement for the game. **No files from the original game are included**: this repository does not distribute `SpideyPC.exe`, game data, textures, audio, videos or other game assets. You must provide your **own legally obtained copy of the original Windows PC game**.

**Independent fan project:** SpiderMan2k1Fix is **not affiliated with, sponsored by, or endorsed by Activision, Neversoft, LTI Gray Matter / Gray Matter Interactive, or any other developer, publisher, distributor or rights holder associated with the original game**. *Spider-Man* and all related trademarks and game content remain the property of their respective owners.

> **Development build — not a stable release.** The fix is still being tested and may not work reliably. Experimental Windows x86 DLL builds can be found in [GitHub Actions](https://github.com/legluondunet/SpiderMan2k1Fix/actions/workflows/spiderman2k1fix-windows-x86.yml).

## Features under development

- **Display:** borderless fullscreen at desktop resolution, widescreen or original 4:3, automatic FOV, corrected opening videos and comic artwork.
- **Gameplay UI:** corrected HUD layout, compass, instructions, boss bars and the in-game ESC menu at widescreen resolutions.
- **Compatibility:** an in-memory CD check bypass, a configurable 30 FPS limiter and optional Alt+Tab/input fixes.
- **Settings:** `SpiderMan2k1Fix.ini` for display, audio and controls, generated and repaired automatically.
- **Screenshots:** press **F9** to capture lossless `scrnNNNN.png` files beside the game executable. This path still needs broader Windows/Wine runtime validation.

For detailed behavior, diagnostics and limitations, see the [documentation index](docs/README.md).

## Requirements and compatibility

- An installed copy of the **original Windows PC version of Spider-Man**, including its game data.
- The supported **32-bit** `SpideyPC.exe` executable.
- Windows, or Linux with a working **32-bit-capable Wine** installation.
- Two SpiderMan2k1Fix DLLs: `winmm.dll` and `SpiderMan2k1Fix.dll`.

SpiderMan2k1Fix currently checks the game's executable before applying its main patches. The supported `SpideyPC.exe` has this SHA-256 checksum:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

Other executable versions, modified executables, and some third-party wrappers may not be compatible. Do not rename an unsupported executable or bypass the compatibility check.

### Installing on Windows

1. Install *Spider-Man* normally and locate the directory containing `SpideyPC.exe`.
2. Back up any existing `winmm.dll`, `SpiderMan2k1Fix.dll`, or `SpiderMan2k1Fix.ini` files in that directory.
3. For **testing only**, download the `SpiderMan2k1Fix-Windows-x86` artifact from a successful [GitHub Actions Windows x86 run](https://github.com/legluondunet/SpiderMan2k1Fix/actions/workflows/spiderman2k1fix-windows-x86.yml) and extract its DLL files. No stable release is available.
4. Copy `winmm.dll` and `SpiderMan2k1Fix.dll` into the **same directory** as `SpideyPC.exe`:

   ```text
   Spider-Man/
   ├── SpideyPC.exe
   ├── winmm.dll
   ├── SpiderMan2k1Fix.dll
   └── SpiderMan2k1Fix.ini    # generated automatically on first launch
   ```

5. Start the game using `SpideyPC.exe`.

On the first supported launch, SpiderMan2k1Fix creates `SpiderMan2k1Fix.ini` automatically if it does not already exist. You do not need to create it manually.

**Configuration during testing and upgrades:** GitHub Actions artifacts contain only the two DLLs and public README/CHANGELOG; they do not include an INI. Keep your existing `SpiderMan2k1Fix.ini` when updating the DLLs to avoid losing personal settings.

**Upgrading from an earlier SpiderManFix beta:** Remove the old `SpiderManFix.dll` (it is no longer used). To keep your settings, rename `SpiderManFix.ini` to `SpiderMan2k1Fix.ini` **before launching** the game. If no new-named INI exists, fresh defaults will be generated. Keep a backup of your original INI.

**Important:** Do not overwrite another mod's `winmm.dll` without backing it up first. Two different `winmm.dll` proxy mods cannot normally occupy the same location under the same filename.

### Installing on Linux with Wine

1. Install *Spider-Man* in a Wine prefix and locate the directory containing `SpideyPC.exe`.
2. Copy the **same two Windows DLLs** (`winmm.dll` and `SpiderMan2k1Fix.dll`) next to `SpideyPC.exe`.
3. Configure Wine to load the local `winmm.dll` **before** its built-in implementation.

   In `winecfg`, open **Libraries**, add `winmm` and set it to **Native then Builtin**.

   Alternatively, start the game from a terminal in its directory:

   ```bash
   WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
   ```

   If you use a custom Wine prefix, select it explicitly:

   ```bash
   WINEPREFIX="$HOME/Games/spider-man-prefix" WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
   ```

4. Launch the game. On a supported executable, SpiderMan2k1Fix initializes and creates `SpiderMan2k1Fix.ini` if necessary.

**Wine override explained:** `winmm=n,b` means **native, then built-in**. This allows Wine to load SpiderMan2k1Fix's local `winmm.dll` proxy first. The proxy forwards the original multimedia functions while loading `SpiderMan2k1Fix.dll`.

If you use Lutris or another Wine launcher, add a DLL override with **Name:** `winmm` and **Value:** `n,b` in that game's Wine configuration (or set the equivalent environment variable). You generally do not need a separate override for `SpiderMan2k1Fix.dll`.

**Proton (Steam):** Proton uses Wine's DLL-loading mechanism. If you run the Windows game through Steam/Proton, place the same two DLLs next to `SpideyPC.exe` and add this to the game's **Steam → Properties → Launch Options**:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

**Badge note:** The **Windows x86 build** badge reports GitHub Actions compilation/test status. The **Linux / Wine / Proton compatible** badge describes the intended compatibility layer for those same Windows x86 DLLs; **it is informational, not the result of a Linux/Proton gameplay CI test**. Individual Wine or Proton versions may require additional testing.

## Using SpiderMan2k1Fix

There is **no separate launcher or graphical configuration utility**. Start *Spider-Man* as usual and change its settings through the in-game menus or `SpiderMan2k1Fix.ini`.

### Basic configuration

The fix creates `SpiderMan2k1Fix.ini` automatically. **Close the game before editing it**; the game may otherwise overwrite your changes.

```ini
[Display]
Resolution=Auto
AspectRatio=Auto
FPSLimit=30

[HUD]
HUDWidescreenFix=1

[Debug]
Log=0
```

Choose `AspectRatio=4:3` for the classic game presentation. `HUDWidescreenFix=0` turns off the widescreen HUD and native ESC corrections. For all available options (audio, keyboard, joystick, experimental focus-pause settings) and INI recovery behavior, see the [configuration and troubleshooting guide](docs/references/CONFIGURATION.md).

### Debug log

Logging is **disabled by default** (`[Debug] Log=0`). Set `Log=1` and restart to create `SpiderMan2k1Fix.log` beside the game. Each launch overwrites the previous log. [More details](docs/references/CONFIGURATION.md#debug-logging).

### Automatic INI recovery

Invalid INI settings can trigger automatic repair with a numbered `SpiderMan2k1Fix_N.old` backup; recognized valid settings are recovered. Do not edit `[Internal] RawHex` manually. [Recovery details](docs/references/CONFIGURATION.md#automatic-ini-recovery).

## Troubleshooting

- **The fix does not load:** verify that both DLLs are beside `SpideyPC.exe`, that the game's SHA-256 matches, and that Wine/Proton uses `winmm=n,b`.
- **Settings were reset:** inspect the numbered `SpiderMan2k1Fix_N.old` backup.
- **Input, focus, audio or graphics problems:** these remain under development. See [detailed troubleshooting](docs/references/CONFIGURATION.md#troubleshooting-cases) and the [known issues and technical analysis](docs/DEVELOPMENT.md#10-major-bugs-root-causes-and-fixes--historical-incident-register).
- **Need diagnostics:** temporarily set `[Debug] Log=1`, restart the game, and save its log before the next launch.

### F9 PNG screenshots

Press **F9** to capture a lossless PNG beside `SpideyPC.exe`. The original game key and its debounce are preserved. See [capture behavior, verification status and optional diagnostics](docs/references/F9_PNG_CAPTURE.md).

### Native ESC menu in widescreen

The gameplay ESC menu retains its original 4:3 proportions on supported widescreen displays. This correction follows the `[HUD] HUDWidescreenFix` switch. [Scope, child options and 1920×1080 Wine validation](docs/references/NATIVE_PAUSE_MENU_43.md).

## Adaptive widescreen HUD

Enabled by default with `HUDWidescreenFix=1`, the fix adjusts the player HUD, compass, mission instructions and supported boss UI while leaving game world geometry alone. Not every boss and resolution has been visually tested. See the [HUD implementation, source signatures and validation notes](docs/DEVELOPMENT.md#7-display-and-rendering-research).

## Roadmap

These are **planned improvements** rather than confirmed working features:

- **Further HUD coverage:** test additional boss encounters and ultrawide aspect ratios (such as 21:9).
- **Audio improvements:** reduce or eliminate unwanted audio noise and artifacts.
- **Widescreen 3D geometry:** investigate 3D objects that appear displaced near the left or right edges of the screen in landscape/widescreen mode.
- **Electro:** investigate and fix the outstanding Electro-related issue.

## Building from source

For **experimental, unvalidated builds**, GitHub Actions makes compiled DLLs available under **Actions → SpiderMan2k1Fix Windows x86 → successful run → Artifacts**. A successful CI build is not evidence that the game works correctly.

The source package includes scripts for building the 32-bit Windows DLLs on Linux with an **MSVCRT-targeting MinGW-w64 toolchain**:

```bash
./build.sh
```

If your system compiler targets UCRT instead, consult `build-native-msvcrt.sh` in the source tree. The build scripts check the 32-bit Windows architecture and expected imports/exports. The resulting `winmm.dll` and `SpiderMan2k1Fix.dll` are Windows DLLs even when built on Linux.

Developer-only tests, PowerShell utilities and WinDbg experiments live under [ChatGPT/](ChatGPT/README.md). Run the regression suite from the repository root with `python3 -m pytest -q ChatGPT/tests`; GitHub Actions uses the same path. These developer tools are not included in the CI DLL artifact.

## Project and contributions

SpiderMan2k1Fix is an independent compatibility project maintained by **[legluondunet](https://github.com/legluondunet)**.

Bug reports and reproducible test results are welcome, especially for different Windows versions, Wine versions, GPUs, resolutions and controller models. When reporting an issue, include your platform, game executable checksum, relevant INI settings, and a debug log when available.

**Project repository:** <https://github.com/legluondunet/SpiderMan2k1Fix>

## Credits and acknowledgements

Special thanks to **[Krystalgamer](https://github.com/krystalgamer)** for making their work on the original *Spider-Man* PC game publicly available, including the [partial PC decompilation](https://github.com/krystalgamer/spidey-decomp) and [research into the game's CD check](https://krystalgamer.github.io/spidey-breaking/index.html). These references have helped the community understand the original game's internals and have been valuable background material for this compatibility project.

## License and third-party rights

SpiderMan2k1Fix is released under the **[GNU General Public License v3.0](LICENSE) (GPL-3.0-only)**. The license applies to the code provided by this project; **it does not grant any rights to the original *Spider-Man* game, its executables, artwork, sound, or other proprietary content**.

This project distributes **no original game files**. You must supply your own legally obtained copy of *Spider-Man* for Windows.

This is an **unofficial, independent fan project**. It is not affiliated with, endorsed by, or sponsored by Activision, Neversoft, LTI Gray Matter / Gray Matter Interactive, or any other company or rights holder involved in the original game's development, publication or distribution. *Spider-Man* and associated intellectual property belong to their respective owners.
