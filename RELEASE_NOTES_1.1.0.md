# SpiderMan2k1Fix v1.1.0

This release introduces a new built-in **native WAV audio-loader correction** for the original *Spider-Man (2001)* PC game. The game's original DirectSound engine is preserved.

## What's new

- **Fixes the original WAV crackling / truncated audio:** The game's WAV loader requests only whole 2,048-byte PKR sectors and omits the final partial sector of many WAV files. SpiderMan2k1Fix restores only the verified missing bytes at the original resource-loading stage.
- **Enabled automatically:** No new INI setting is required for the exact supported executable. The hidden troubleshooting override `[Debug] ExperimentalAudioLoaderTail=0` disables the repair.
- **Strict original-file verification:** Original `data.pkr` PCM16 WAVs are decoded and checked using per-resource CRC32. The correction requires an exact matching RIFF length, fully identical existing prefix, identical missing bytes across all possible source aliases, and a fresh matching native read into a private 2,048-byte scratch buffer. Unverifiable cases are left unchanged.
- **Original DirectSound preserved:** No replacement audio backend, no game asset rewriting, and no late DirectSound-buffer tail mutation. The old `ExperimentalAudioPKRTailRestore` setting and workaround have been removed.
- **Optional helicopter sample cleanup:** The independent `[Debug] ExperimentalAudioHeli1Declick=1` corrects two known anomalous original samples in `Heli1.wav` when explicitly enabled; it is off by default.
- **Reverse-engineering documentation:** The original WAV/DirectSound functions, exact x86 addresses, proven cause, rejected alternatives and test evidence are preserved in [the audio research reference](https://github.com/legluondunet/SpiderMan2k1Fix/blob/main/docs/references/diagnostics/audio/ORIGINAL_AUDIO_FUNCTIONS_AND_WAV_BUG.md).

All previously released v1.0.0 compatibility, widescreen, controls and graphical features remain available.

## Tested

On **27 October 2026**, a Linux/Wine gameplay test of the fallback-free audio fix confirmed **no audible parasites**. The trace includes:

- 656/656 original WAVs verified by CRC32;
- 95 logged native final-sector repairs, totaling 91,406 bytes;
- 3 repaired duplicate-source alias loads (2 × 460 and 1 × 1,988 bytes), already included in those 95 events;
- 49 exact full-PCM matches and zero differences;
- no legacy DirectSound tail repairs or unmatched PCM-bearing tail requests in the observed scenario.

Windows x86 MSVCRT compilation and regression tests are covered by GitHub Actions. **Native Windows gameplay validation of the final default-on configuration is still pending.** Results above are specific to the supported original executable and tested Wine scenarios, not a guarantee for all game sequences.

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
- The built-in native loader fix is verified under Linux/Wine; a final native Windows gameplay acceptance test remains outstanding.

This release contains **no retail game executable or proprietary game assets**.

Developed by **legluondunet assisted by ChatGPT (OpenAI)**.
