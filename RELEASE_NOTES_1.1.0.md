# SpiderMan2k1Fix v1.1.0

This update improves audio playback in the original *Spider-Man (2001)* PC game. All features introduced in v1.0.0 remain available.

## Main Features

- **Automatic audio fix:** Enabled by default, with no extra files or configuration required.

## Bugs Resolved

- **Crackling sound effects:** Fixed distorted and noisy audio caused by a bug in the original game.

For implementation details and test results, see the [technical audio documentation](docs/references/diagnostics/audio/PKR_NATIVE_AUDIO_REPAIR.md).

## Known Issues

The following issues remain present in v1.1.0:

- **FPS-dependent gameplay:** Certain bosses and levels may not work correctly at 30 FPS. Set `FPSLimit=20` in `SpiderMan2k1Fix.ini` when needed.
- **Delayed pause under Wine:** Automatic pausing when the game loses focus may occasionally be delayed.

## Installation on Windows and Linux (Wine)

1. Install the original *Spider-Man (2001)* PC game.

2. Download the `SpiderMan2k1Fix-1.1.0-Windows-x86.zip` archive from this release.

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

Keep your existing `SpiderMan2k1Fix.ini` when upgrading to preserve your settings.

## Credits

Developed by **legluondunet assisted by ChatGPT (OpenAI)**.

Special thanks to [Krystalgamer](https://krystalgamer.github.io/index.html), whose reverse-engineering work on the original PC game contributed to the development of this patch.