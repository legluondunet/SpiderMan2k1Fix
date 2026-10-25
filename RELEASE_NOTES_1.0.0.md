# SpiderMan2k1Fix v1.0.0

The first stable release of SpiderMan2k1Fix, an unofficial compatibility patch for the original *Spider-Man (2001)* PC game, designed to improve compatibility with modern Windows systems and Linux/Wine.

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
- **Settings preservation:** Keyboard, gamepad, audio, and brightness settings are managed through `SpiderMan2k1Fix.ini`, with automatic recovery of invalid settings and a single rolling `.ini.old` backup. The original `Spidey.cfg` is no longer used.
- **PNG screenshots:** Press **F9** to save lossless screenshots (`scrnNNNN.png`) in the game directory.
- **Debug logging:** Can generate a `SpiderMan2k1Fix.log` diagnostic file to help troubleshoot compatibility issues. Enable it by setting `Log=1` in the INI file.
- **Build identification:** Displays the patch version and source commit in the game's main menu and diagnostic logs.
- **Original taskbar icon:** Restores the game's original icon in the Windows taskbar.

**Successfully tested on Windows 11 and Linux/Wine.**

## Bugs Resolved

- **Alt+Tab crashes and input loss:** Fixed crashes and permanent loss of keyboard or mouse controls when switching between the game and other applications.
- **Focus-loss instability:** Added automatic game pausing with synchronized video and audio playback, preventing gameplay and media synchronization issues while the game is in the background.
- **Crash on exit:** Fixed a crash that could occur when quitting the game.
- **Broken F9 screenshots on Windows:** Fixed screenshots that appeared entirely purple or contained no visible game graphics.
- **Unsupported display modes:** Prevented the game from switching to obsolete fullscreen resolutions and 16-bit display modes that could produce a black screen on modern monitors.
- **Hidden desktop cursor:** Fixed cases where the Windows mouse pointer remained invisible after switching away from the game.

## Known Issues

The following issues remain present in v1.0.0:

- **FPS-dependent gameplay:** Certain bosses and levels may not work correctly at 30 FPS. Set `FPSLimit=20` in `SpiderMan2k1Fix.ini` when needed.
- **Audio issues:** The original game's crackling and other sound playback artifacts remain unresolved.
- **Delayed pause under Wine:** Automatic pausing when the game loses focus may occasionally be delayed.

## Installation on Windows and Linux (Wine)

1. Install the original *Spider-Man (2001)* PC game.

2. Download the `SpiderMan2k1Fix-1.0.0-Windows-x86.zip` archive from this release.

3. Extract `winmm.dll` and `SpiderMan2k1Fix.dll` into the game's installation directory, alongside `SpideyPC.exe`.

4. Launch the game normally under Windows.

   For Linux/Wine users, open a terminal in the game's installation directory and run:

   ```bash
   WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
   ```

The patch automatically creates `SpiderMan2k1Fix.ini` on first launch.

**Important:** The patch requires the original supported `SpideyPC.exe` (SHA-256):

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

No original game files are included in this release.

## Credits

Developed by **legluondunet assisted by ChatGPT (OpenAI)**.

Special thanks to [Krystalgamer](https://krystalgamer.github.io/index.html), whose reverse-engineering work on the original PC game contributed to the development of this patch.