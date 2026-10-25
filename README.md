# SpiderMan2k1Fix

SpiderMan2k1Fix is an **unofficial fan-made patch** for the PC version of *Spider-Man*, released by Activision in 2001. It improves compatibility with modern Windows systems and Linux/Wine while adding new features.

## Screenshots

<p align="center">
  <a href="assets/screenshots/main-menu.webp"><img src="assets/screenshots/main-menu.webp" alt="Spider-Man main menu, centered 4:3" width="49%"></a>
  <a href="assets/screenshots/gameplay.webp"><img src="assets/screenshots/gameplay.webp" alt="Spider-Man widescreen gameplay with HUD" width="49%"></a>
</p>

## Main Features

- **No-CD:** Bypasses the original CD-ROM check through an in-memory patch, without modifying the game executable.
- **Modern display support:** Runs in borderless fullscreen at the monitor's native resolution, with automatic widescreen 3D (`Auto`) or a centered classic 4:3 mode. Also supports multi-monitor configurations.
- **Automatic FOV adjustment:** Automatically adjusts the field of view to match the desktop resolution and aspect ratio, providing proper widescreen rendering without distorting the 3D scene.
- **Aspect ratio corrections:** Preserves the original proportions of menus, cinematics, introductory videos, comic-book artwork, and other 2D elements.
- **Widescreen HUD:** Corrects the proportions and positioning of HUD elements, including health and web gauges, compass, mission objectives, boss health bars, and in-game pause menus. Tested at 1920×1080 (16:9) and 1920×1200 (16:10).
- **FPS limiter:** Configurable frame-rate cap, set to **30 FPS by default**. Lower limits, such as 20 FPS, can be selected for timing-sensitive gameplay.
- **Alt+Tab stability:** Automatically pauses the game when it loses focus, synchronizes video and audio playback, and restores keyboard and mouse input when gameplay resumes. Press any key after returning to the game to resume. Also restores the Windows desktop cursor when necessary.
- **Keyboard layouts:** Automatically detects the active keyboard layout and adapts the game's controls, keyboard labels, and cheat-code entry. The original game only supported QWERTY layouts.
- **DPI awareness:** Handles Windows display scaling to ensure correct window dimensions and rendering on high-DPI displays.
- **Settings preservation:** Keyboard, gamepad, audio, and brightness settings are managed through `SpiderMan2k1Fix.ini`, with automatic recovery of invalid settings and a single rolling `.ini.old` backup. The original `Spidey.cfg` file is no longer needed.
- **PNG screenshots:** Press **F9** to save lossless screenshots (`scrnNNNN.png`) in the game directory.
- **Debug logging:** Can generate a `SpiderMan2k1Fix.log` diagnostic file to help troubleshoot compatibility issues. Enable it by setting `Log=1` in the INI file.
- **Build identification:** Displays the patch version and source commit in the game's main menu and diagnostic logs.
- **Original taskbar icon:** Restores the game's original icon in the Windows taskbar.

## Bugs Resolved

- **Alt+Tab crashes and input loss:** Fixed crashes and permanent loss of keyboard or mouse controls when switching between the game and other applications.
- **Focus-loss instability:** Added automatic game pausing with synchronized video and audio playback, preventing gameplay and media synchronization issues while the game is in the background.
- **Crash on exit:** Fixed a crash that could occur when quitting the game.
- **Broken F9 screenshots on Windows:** Fixed screenshots that appeared entirely purple or contained no visible game graphics.
- **Unsupported display modes:** Prevented the game from switching to obsolete fullscreen resolutions and 16-bit display modes that could produce a black screen on modern monitors.
- **Hidden desktop cursor:** Fixed cases where the Windows mouse pointer remained invisible after switching away from the game.

**Successfully tested on Windows 11 and Linux/Wine.**

## Installation on Windows and Linux (Wine)

1. Install the original *Spider-Man (2001)* PC game.
2. Download the latest version of SpiderMan2k1Fix from the [GitHub Releases](https://github.com/legluondunet/SpiderMan2k1Fix/releases) page.
3. Extract `winmm.dll` and `SpiderMan2k1Fix.dll` into the game's installation directory, alongside `SpideyPC.exe`.
4. Launch the game normally under Windows.

   For Linux/Wine users, open a terminal in the game's installation directory and run:

   ```bash
   WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
   ```

**Important:** The patch requires the original supported `SpideyPC.exe` executable (SHA-256):

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

Other versions or modified copies of the executable are not supported.

**No original game files are included in this project.** You must provide your own legally obtained copy of the game.

## Configuration

The original game's menus allow players to configure keyboard controls, gamepad bindings, audio levels, and other game settings.

SpiderMan2k1Fix preserves these settings in a single configuration file, `SpiderMan2k1Fix.ini`, which is automatically created in the game's installation directory. This file replaces the original `Spidey.cfg`, which is no longer used.

The INI file also provides additional settings specific to SpiderMan2k1Fix:

### Main Settings

| Setting | Description |
|---|---|
| `AspectRatio=Auto` | Automatically adapts 3D gameplay to the monitor's aspect ratio. |
| `AspectRatio=4:3` | Uses the original 4:3 aspect ratio with centered pillarboxing. |
| `FPSLimit=30` | Limits gameplay to 30 FPS by default. Set to `20` for timing-sensitive sections or `0` to disable the limiter. |
| `KeyboardLayout=Auto` | Automatically detects and uses the active keyboard layout. |
| `KeyboardLayout=Off` | Disables automatic keyboard-layout detection and preserves the original QWERTY mapping. |
| `Log=1` | Enables diagnostic logging to `SpiderMan2k1Fix.log`. |

**Important:**

- Close the game before manually editing the INI file.
- Keep your existing INI file when updating the patch.
- The `[Internal] RawHex` value preserves original game settings and should not be modified manually.

For advanced settings and detailed explanations, see the [Configuration and Troubleshooting Guide](docs/references/CONFIGURATION.md).

## Troubleshooting

**The patch does not load**

Make sure both DLL files are located alongside the supported `SpideyPC.exe`. Under Linux/Wine, verify that the `winmm=n,b` DLL override is active.

**The game behaves unexpectedly**

Enable diagnostic logging by setting `Log=1` under `[Debug]` in `SpiderMan2k1Fix.ini`, then try to reproduce the issue.

If the problem persists, report it through [GitHub Issues](https://github.com/legluondunet/SpiderMan2k1Fix/issues) and attach the generated `SpiderMan2k1Fix.log` file.

## Building from Source

SpiderMan2k1Fix can be compiled on Linux using a 32-bit MSVCRT-targeting MinGW-w64 toolchain:

```bash
./build.sh
```

The build produces Windows x86 DLLs compatible with the supported game executable.

Automated builds and tests are available through [GitHub Actions](https://github.com/legluondunet/SpiderMan2k1Fix/actions).

Bug reports and contributions are welcome through [GitHub Issues](https://github.com/legluondunet/SpiderMan2k1Fix/issues).

## Documentation

Additional technical information is available for users and developers:

- [Project Documentation](docs/README.md)
- [Configuration and Troubleshooting](docs/references/CONFIGURATION.md)
- [Development and Reverse Engineering](docs/DEVELOPMENT.md)
- [Automatic Keyboard Layout Detection](docs/references/KEYBOARD_LAYOUT_TECHNICAL.md)
- [F9 PNG Screenshot Capture](docs/references/F9_PNG_CAPTURE.md)
- [Changelog](CHANGELOG.md)

## Credits

Developed by **legluondunet assisted by ChatGPT (OpenAI)**.

Special thanks to [Krystalgamer](https://github.com/krystalgamer) for the publicly available [partial PC game decompilation](https://github.com/krystalgamer/spidey-decomp) and [research into the original CD-ROM check](https://krystalgamer.github.io/spidey-breaking/index.html).

## License and Disclaimer

SpiderMan2k1Fix is distributed under the [GNU General Public License v3.0](LICENSE) (**GPL-3.0-only**).

This license applies only to SpiderMan2k1Fix and does not cover the original game's executable, graphics, audio, or other proprietary assets.

This is an independent fan-made project and is not affiliated with, sponsored by, or endorsed by Activision, Neversoft, Gray Matter Interactive, or any other original developer, publisher, distributor, or rights holder.

*Spider-Man* and all associated trademarks and game content remain the property of their respective owners.
