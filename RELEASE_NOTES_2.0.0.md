# SpiderMan2k1Fix v2.0.0

This release modernizes the original *Spider-Man (2001)* PC game on Windows and Linux (Wine/Proton), without modifying the supported original executable. It retains all improvements from v1.0.0 and the v1.1.0 audio fix.

## Main features

- **SDL3 gamepad support:** the only supported controller mapping backend, enabled automatically. Select a controller by pressing **Start**, then release its controls to neutral. Supports native menus, intro and Bink video skip, gameplay, Pause and Game Over. The keyboard remains independently usable. Per-model mappings are saved separately.
- **Gamepad Save Game confirmation:** controller South (Xbox **A**, PlayStation **Cross**) can choose a save slot and confirm **YES** without needing keyboard Enter.
- **Multiple identical controller models:** separate SDL3 instance IDs allow switching between two Xbox-compatible controllers with the same `045E:028E` VID:PID. Competing simultaneous Start claims are refused; the original DirectInput ownership safety rules are preserved.
- **Focus and Alt+Tab:** borderless fullscreen, cursor recovery, guarded Left Alt release, background PAUSED screen, and synchronized Bink and DirectSound pause/resume. Includes an optional bundled native Linux X11 focus helper (launched automatically where supported).
- **Native presentation:** proper widescreen 3D and FOV with preserved centered 4:3 video/menu/comic content, native 4:3 Pause overlay and widescreen HUD/scene iris.
- **Safety and configuration:** automatic cleaning of obsolete generated INI options, preservation of user preferences and original binary configuration bytes, No-CD in-memory patch, F9 PNG screenshots and stable audio WAV-loader repair.

## Verification

The integrated RC2 passed **951 automated regression tests** and was checked on **Windows 11** and **Wine/GE-Proton on Linux**, with maintainer confirmation of normal gameplay and controller operations, Alt+Tab, sound/video, Save Game/YES, native Pause, Game Over and normal exit. The two same-VID:PID Xbox controllers were tested on both systems; a PS4 controller was also used for Wine switching. See the [RC2 validation record](https://github.com/legluondunet/SpiderMan2k1Fix/blob/main/docs/references/RELEASE_2_0_RC_TEST_PLAN.md) and [PR #136](https://github.com/legluondunet/SpiderMan2k1Fix/pull/136).

The stable `2.0.0` build must independently pass the repository's Windows x86 CI, packaging and SHA-256 checks before the official tag or GitHub release is published.

## Installation — Windows and Linux (Wine)

1. Install a legally obtained copy of the original *Spider-Man (2001)* PC game.
2. Obtain the **v2.0.0 Windows x86 ZIP** from the [official GitHub Releases page](https://github.com/legluondunet/SpiderMan2k1Fix/releases) when published.
3. Extract the ZIP **into the game's directory**, keeping `winmm.dll` next to `SpideyPC.exe` and the `SpiderMan2k1Fix/` directory containing `SpiderMan2k1Fix.dll`, `SDL3.dll`, the SDL3 license and optional `linux/spiderman-focus-helper`. **Do not flatten the subdirectories.**
4. Keep your existing `SpiderMan2k1Fix.ini` and saved games; the patch generates an INI on first run only if none exists. Launch `SpideyPC.exe` normally under Windows.

For Wine users, from the game installation directory:

```bash
WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
```

Only the original supported `SpideyPC.exe` SHA-256 is accepted:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

No original game executable, game assets or copyrighted data are included in this patch.

## Known limitations and follow-up

- Advanced **Steam Deck/Steam Input**, virtual controller identity, analog-axis fidelity and untested controller models remain tracked in [issue #120](https://github.com/legluondunet/SpiderMan2k1Fix/issues/120); successful preliminary Deck experiments do not imply complete Deck support.
- Native Windows 11 **DualShock 4 audio-output selection** has a separate unresolved follow-up in [issue #118](https://github.com/legluondunet/SpiderMan2k1Fix/issues/118). The game follows system audio routing unless separately configured.
- A physical controller mirrored into multiple SDL3 handles and deliberately simultaneous competing Start presses have not been confirmed through hardware testing. Ambiguous ownership must fail closed.
- As in earlier releases, game timing may depend on FPS in certain levels. If the default `FPSLimit=30` causes issues, try `FPSLimit=20` in `SpiderMan2k1Fix.ini`.

## Credits

Developed by **legluondunet assisted by ChatGPT (OpenAI)**.

Thanks to [Krystalgamer](https://krystalgamer.github.io/index.html) for reverse-engineering contributions to the original PC game.

SpiderMan2k1Fix is an independent fan-made project; it is not affiliated with the original game's developers or publishers.
