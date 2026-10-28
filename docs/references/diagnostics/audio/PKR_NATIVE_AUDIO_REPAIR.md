# Native PKR WAV audio restoration (built-in correction)

## Confirmed root cause

The supported, SHA-256-verified Spider-Man 2001 PC executable uses the
original native PKR reader to load sound assets. Disassembly confirmed that
the WAV loader obtains a byte length at native callsite `0x503CB4`, divides
the length by **2,048** at `0x503CD3`, and asks the native reader at
`0x503CD8` for only the *complete* sectors. The final partial WAV-file
sector is therefore omitted before DirectSound is initialized.

`data.pkr` contains **656 CRC-verified PCM16 mono WAVs** in the tested
edition. The game remains responsible for voice management and all native
DirectSound playback; no SDL3 or external WAV files are needed.

## Current correction: native loader, not DirectSound playback

The [root-cause investigation](NATIVE_AUDIO_LOADER_ROOT_CAUSE.md) documents
the exact guarded native hook, multiple Wine test runs, and the
source-matching and memory-safety conditions.

1. Validate the original `SpideyPC.exe` hash and exact native call
   signatures before installing the hook.
2. Independently decode and verify every original WAV source from
   `data.pkr` using PKR3 bounds, per-resource CRC32 and RIFF parsing.
3. Call the original sector reader for all complete blocks, unchanged.
   Only if the already-allocated WAV staging buffer has an incomplete
   final sector, look for matching original WAV bytes.
4. Require the **entire loaded prefix, original RIFF length, audio data
   extent and CRC-backed source contents** to match. If multiple source
   names match, **all missing remainder bytes must be identical**.
   Metadata-only tails need no repair.
5. Ask the **original native PKR reader** for one extra 2,048-byte sector
   into a separate scratch buffer. Copy *only* the expected remainder
   into the game's existing, exact-size allocation, and only after
   the new native bytes exactly agree with the independently verified
   original source.
6. Preserve original DirectSound calls, playback timing, looping, stops,
   pitch, volume and focus behavior.

All unverified source identities, mismatching remainders, incomplete
checks and unsupported modes **fail closed without modifying PCM**.

### Configuration

**The native loader correction is enabled by default**, with no entry
written to the generated `SpiderMan2k1Fix.ini`. The code verifies the
original `SpideyPC.exe` hash and the exact loader call signatures
before installing the fix.

For exceptional troubleshooting, users may manually place
`ExperimentalAudioLoaderTail=0` under `[Debug]` to turn off just
this fix. Existing `ExperimentalAudioLoaderTail=1` settings are still
accepted but redundant. The compatibility override is intentionally
**not present in the factory INI**.

The independent `ExperimentalAudioHeli1Declick=1` opt-in corrects
only two known `Heli1.wav` samples in a verified **stopped**
secondary DirectSound buffer. It is **not** a partial-sector fallback.

Optional diagnostics:

```ini
[Debug]
ExperimentalPKRAudioIndex=1
ExperimentalPKRAudioPlayTrace=1
Log=1
```

Audio source indexing is also initialized automatically when the loader
fix or Heli1 declick is enabled.

## Retired DirectSound-buffer tail fallback

The earlier experimental `ExperimentalAudioPKRTailRestore` method
identified damaged tails at DirectSound Play time and corrected a
stopped buffer. It was independently successful at eliminating audible
crackling under Wine but was redundant once the loader root cause
was confirmed.

Following the **27 Oct 2026 targeted Linux/Wine test** running
`v1.0.0-04b2172`, the user confirmed **no audio parasites**.
That test logged:

- Two successful native alias restores of **460 bytes** each;
- One successful native alias restore of **1,988 bytes**;
- **2,908 bytes** restored across these previously ambiguous PCM tails;
- **Zero** late DirectSound tail repairs;
- **44** exact read-only full-PCM comparisons, without differences;
- Only a harmless 20-byte, metadata-only skip for `BreakFree.wav`.

The user explicitly approved removing the fallback. Accordingly,
`SpiderPKRAudioIndex_TryTailRestore`,
`SpiderPKRAudioIndex_EnableTailRestore`, the corresponding
DirectSound mutation logic and the
`ExperimentalAudioPKRTailRestore` configuration key
were removed from the main source code. The original native
loader fix, strict source matching, optional Heli1 declick, and
opt-in read-only DirectSound source diagnostics are retained.

Legacy runtime audio diagnoses (`TAIL-RESTORED`, `TAIL-SKIP`,
Play-time ambiguous-source cases) are documented in the historical
investigation, but are **not** active fallback behavior in the
current source build.

## Verification status

The 27 Oct Linux/Wine runs establish correct native restore of both
formerly ambiguous WAV tail classes, and the user's subjective listening
reports are consistently crackle-free, including in that targeted area.
**This does not establish native Windows runtime validation**, nor does
it prove every conceivable edition/resource has been encountered.

The fallback-free build was verified under Linux/Wine with the
three alias restorations and user-confirmed clean playback.
The new default-on configuration is covered by regression tests and
Windows x86 compilation, but still requires a runtime smoke test
after these defaults change. The loader should report
`LOADER-HOOK ARMED` even when the factory INI contains no audio-fix
key. The legacy Play-time `TAIL-SKIP` messages must not return.
Native Windows runtime validation remains pending.

## 1.1.0 release — archived original release notes (27 October 2026)

The public [v1.1.0 release notes](../../../../RELEASE_NOTES_1.1.0.md)
were shortened for players. The following is the **original technical
`What's new` and `Tested` content** from the release-notes file,
preserved here as a dated record, not as a claim of validation under
every Windows environment.

### Original What's new

- **Fixes the original WAV crackling / truncated audio:** The game's WAV loader requests only whole 2,048-byte PKR sectors and omits the final partial sector of many WAV files. SpiderMan2k1Fix restores only the verified missing bytes at the original resource-loading stage.
- **Enabled automatically:** No new INI setting is required for the exact supported executable. The hidden troubleshooting override `[Debug] ExperimentalAudioLoaderTail=0` disables the repair.
- **Strict original-file verification:** Original `data.pkr` PCM16 WAVs are decoded and checked using per-resource CRC32. The correction requires an exact matching RIFF length, fully identical existing prefix, identical missing bytes across all possible source aliases, and a fresh matching native read into a private 2,048-byte scratch buffer. Unverifiable cases are left unchanged.
- **Original DirectSound preserved:** No replacement audio backend, no game asset rewriting, and no late DirectSound-buffer tail mutation. The old `ExperimentalAudioPKRTailRestore` setting and workaround have been removed.
- **Optional helicopter sample cleanup:** The independent `[Debug] ExperimentalAudioHeli1Declick=1` corrects two known anomalous original samples in `Heli1.wav` when explicitly enabled; it is off by default.
- **Reverse-engineering documentation:** The original WAV/DirectSound functions, exact x86 addresses, proven cause, rejected alternatives and test evidence are preserved in [the audio research reference](https://github.com/legluondunet/SpiderMan2k1Fix/blob/main/docs/references/diagnostics/audio/ORIGINAL_AUDIO_FUNCTIONS_AND_WAV_BUG.md).

All previously released v1.0.0 compatibility, widescreen, controls and graphical features remain available.

### Original Tested

On **27 October 2026**, a Linux/Wine gameplay test of the fallback-free audio fix confirmed **no audible parasites**. The trace includes:

- 656/656 original WAVs verified by CRC32;
- 95 logged native final-sector repairs, totaling 91,406 bytes;
- 3 repaired duplicate-source alias loads (2 × 460 and 1 × 1,988 bytes), already included in those 95 events;
- 49 exact full-PCM matches and zero differences;
- no legacy DirectSound tail repairs or unmatched PCM-bearing tail requests in the observed scenario.

Windows x86 MSVCRT compilation and regression tests are covered by GitHub Actions. **Native Windows gameplay validation of the final default-on configuration is still pending.** Results above are specific to the supported original executable and tested Wine scenarios, not a guarantee for all game sequences.

**Previously listed under “Known limitations”:** “The built-in native
loader fix is verified under Linux/Wine; a final native Windows gameplay
acceptance test remains outstanding.” This is the recorded test coverage
boundary, **not an observed audio limitation or a confirmed Windows defect**.

## 1.1.0 release — archived detailed changelog (27 October 2026)

The public [v1.1.0 changelog](../../../../CHANGELOG.md) intentionally
summarizes the single player-facing improvement in two short bullets.
The original technical release notes are archived below, rather than
requiring players to read implementation details.

- Identified the root cause of audio crackling in the original game: its native WAV loader reads only complete 2,048-byte PKR resource sectors and omits the final partial sector.
- Added a **signature-guarded native loader correction** that restores only the missing RIFF/WAV remainder after matching the entire loaded prefix and verifying the original sources independently by CRC32.
- Validates each additional read through the original native PKR reader in a private 2,048-byte scratch buffer before writing only the exact missing remainder to the game's preallocated WAV buffer.
- Safely handles **duplicate original WAV sources** only if every matching source has identical remaining bytes. Leaves metadata-only remainders, ambiguous or unverified assets unchanged.
- Retained the game's original DirectSound playback engine and the separate opt-in two-sample `Heli1.wav` declick; removed the obsolete late DirectSound tail repair and its `ExperimentalAudioPKRTailRestore` INI option.
- Kept bounded read-only source/PCM diagnostics and optional DirectSound Play tracing; no SDL3 backend or external audio assets are required.
- **Linux/Wine validation (27 October 2026):** confirmed native repair of the previously ambiguous two 460-byte and one 1,988-byte PCM tails, unchanged full-PCM comparison results, and user-confirmed crackle-free playback **without** the fallback.
- **Enabled the native WAV loader correction by default** for the supported executable, without adding a setting to the generated INI. An existing manually configured `ExperimentalAudioLoaderTail=0` remains a hidden troubleshooting override; `=1` is no longer necessary.
- Preserved the original SHA-256-bound WAV/DirectSound function addresses, disassembly, failure mechanism and rejected hypotheses in the permanent technical research documentation (PR #71).
- **Verification:** Linux/Wine gameplay and listening confirmed no audible crackling with the late DirectSound fallback removed; Windows x86 compilation and static regression tests passed. Native Windows gameplay verification of the final default-on build remains pending.
- **Release:** minor version 1.1.0 introduces the built-in audio repair; only the supported original `SpideyPC.exe` is compatible.

Any further changes must go through a separately reviewed PR.

legluondunet assisted by ChatGPT (OpenAI)
