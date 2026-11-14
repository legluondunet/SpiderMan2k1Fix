# SpiderMan2k1Fix v2.0.0

This release modernizes the original *Spider-Man (2001)* PC game on Windows and Linux (Wine/Proton), without modifying the supported original executable. It retains all improvements from v1.0.0 and the v1.1.0 audio fix.

## Main features

- **SDL3 gamepad support:** the only supported controller mapping backend, enabled automatically. Select a controller by pressing **Start**, then release its controls to neutral. Supports native menus, intro and Bink video skip, gameplay, Pause and Game Over. The keyboard remains independently usable. Per-model mappings are saved separately.
- **Gamepad Save Game confirmation:** controller South (Xbox **A**, PlayStation **Cross**) can choose a save slot and confirm **YES** without needing keyboard Enter.
- **Multiple identical controller models:** separate SDL3 instance IDs allow switching between two Xbox-compatible controllers with the same `045E:028E` VID:PID. Competing simultaneous Start claims are refused; the original DirectInput ownership safety rules are preserved.
- **Focus and Alt+Tab:** borderless fullscreen, cursor recovery, guarded Left Alt release, background PAUSED screen, and synchronized Bink and DirectSound pause/resume. Includes an optional bundled native Linux X11 focus helper (launched automatically where supported).
- **Native presentation:** proper widescreen 3D and FOV with preserved centered 4:3 video/menu/comic content, native 4:3 Pause overlay and widescreen HUD/scene iris.
- **4:3 Save Game screens (merged [PR #144](https://github.com/legluondunet/SpiderMan2k1Fix/pull/144)):** the original **SAVE GAME**, **INPUT NAME** and **SAVE COMPLETED** background/title render paths are centered, including the background frozen beneath the external-focus **PAUSED** panel. This correction is **enabled automatically** on the supported retail executable; no experimental environment variable is required. Original graphics are left unmodified if strict binary/callsite checks fail.
- **Safety and configuration:** automatic cleaning of obsolete generated INI options, preservation of user preferences and original binary configuration bytes, No-CD in-memory patch, F9 PNG screenshots and stable audio WAV-loader repair.

## Verification

The integrated RC2 passed **951 automated regression tests** and was checked on **Windows 11** and **Wine/GE-Proton on Linux**, with maintainer confirmation of normal gameplay and controller operations, Alt+Tab, sound/video, Save Game/YES, native Pause, Game Over and normal exit. The two same-VID:PID Xbox controllers were tested on both systems; a PS4 controller was also used for Wine switching. See the [RC2 validation record](https://github.com/legluondunet/SpiderMan2k1Fix/blob/main/docs/references/RELEASE_2_0_RC_TEST_PLAN.md) and [PR #136](https://github.com/legluondunet/SpiderMan2k1Fix/pull/136).

After RC2, the SAVE GAME render fix from PR #144 passed targeted Wine 11.17 testing at 1920×1080 (including real Alt+Tab focus-loss capture). The merged `main` DLL workflow and draft standalone-installer workflows passed CI on November 13, 2026. **This does not validate the new ISO/CUE installer:** its original-disc extraction, registry independence, Windows 11 and Wine end-to-end behavior still require real-CD smoke tests before publishing the official v2.0.0 tag.

## Installation — Windows and Linux (Wine)

The planned v2.0.0 distribution is **one `SpiderMan2k1Fix.exe`** embedding
the project's WinMM proxy, fix DLL, SDL3 Windows x86 runtime, license and
optional Linux X11 helper. It **does not** include copyrighted original
game assets or an original game executable.

1. Obtain `SpiderMan2k1Fix.exe` from the
   [official release page](https://github.com/legluondunet/SpiderMan2k1Fix/releases)
   **after final original-CD validation and publication**.
2. Put it beside one legally obtained retail `.iso` or `.cue` image of
   *Spider-Man (2001)*, with the CUE-referenced `.bin` or `.img` track.
3. Run `SpiderMan2k1Fix.exe` (Windows) or `wine SpiderMan2k1Fix.exe`
   (Linux). It installs the original game into `SpiderMan2k1Fix/`, deploys
   the embedded components, verifies and launches. Later starts verify
   before play; no original Wise/DirectX setup is executed.

Useful switches: `--install-only` (install or explicitly upgrade known
bundled resources without playing), `--verify` (read-only check),
`--launch-no-verify` (direct launch, no presence/hash preflight),
`--inspect image.iso` or `--inspect image.cue [track.bin|track.img]`
(read-only unsupported-disc report), `--verbose`, `--version`, `--help`
and `-h`. ISO/CUE paths are accepted as arguments **only for `--inspect`**.

The launcher and gameplay logs are placed next to the standalone EXE:
`SpiderMan2k1Fix_installer.log` and `SpiderMan2k1Fix.log`. The separate
`SpiderMan2k1Fix_inspect.log` is for disc diagnostics.

Only the approved original `SpideyPC.exe` SHA-256 is supported:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

The SAVE GAME aspect correction is permanently enabled for the supported
executable; do **not** set obsolete trial environment variables.

## Known limitations and follow-up

- The **NEW YORK sign's missing electric effect on W** ([#146](https://github.com/legluondunet/SpiderMan2k1Fix/issues/146)), **unexpected moving shadows** ([#147](https://github.com/legluondunet/SpiderMan2k1Fix/issues/147)) and **bank rooftop window sound order** ([#145](https://github.com/legluondunet/SpiderMan2k1Fix/issues/145)) are open investigation reports, not claimed fixes. Their origins (original game vs. patch or rendering environment) have not yet been established.
- Advanced **Steam Deck/Steam Input**, virtual controller identity, analog-axis fidelity and untested controller models remain tracked in [issue #120](https://github.com/legluondunet/SpiderMan2k1Fix/issues/120); successful preliminary Deck experiments do not imply complete Deck support.
- Native Windows 11 **DualShock 4 audio-output selection** has a separate unresolved follow-up in [issue #118](https://github.com/legluondunet/SpiderMan2k1Fix/issues/118). The game follows system audio routing unless separately configured.
- A physical controller mirrored into multiple SDL3 handles and deliberately simultaneous competing Start presses have not been confirmed through hardware testing. Ambiguous ownership must fail closed.
- As in earlier releases, game timing may depend on FPS in certain levels. If the default `FPSLimit=30` causes issues, try `FPSLimit=20` in `SpiderMan2k1Fix.ini`.

## Credits

Developed by **legluondunet assisted by ChatGPT (OpenAI)**.

Thanks to [Krystalgamer](https://krystalgamer.github.io/index.html) for reverse-engineering contributions to the original PC game.

SpiderMan2k1Fix is an independent fan-made project; it is not affiliated with the original game's developers or publishers.
