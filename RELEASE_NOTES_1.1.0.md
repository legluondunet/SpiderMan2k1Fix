# SpiderMan2k1Fix v1.1.0

## What's new

- Fixed crackling and distorted sound effects caused by a bug in the original game.
- The audio fix is enabled automatically — no configuration or extra files needed.

[Technical explanation and test results](docs/references/diagnostics/audio/PKR_NATIVE_AUDIO_REPAIR.md)

## Installation (Windows / Linux Wine)

1. Install your own original *Spider-Man (2001)* PC game.
2. Download `SpiderMan2k1Fix-1.1.0-Windows-x86.zip` from this release.
3. Extract `winmm.dll` and `SpiderMan2k1Fix.dll` alongside `SpideyPC.exe`.
4. Run the game normally on Windows. On Linux/Wine, launch from the game directory with:

   ```bash
   WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
   ```

Keep your existing `SpiderMan2k1Fix.ini` to preserve settings. The patch can create an INI if none exists.

**Supported original executable only:** `SpideyPC.exe` SHA-256:
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`

## Known limitations

- FPS-dependent stages/boss fights may require `FPSLimit=20` instead of 30.
- Background focus pausing under Wine may occasionally be delayed.

This release contains **no retail game executable or proprietary game assets**.

Developed by **legluondunet assisted by ChatGPT (OpenAI)**.
