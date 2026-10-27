# Native PKR audio restoration (experimental development branch)

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

In `SpiderMan2k1Fix.ini`:

```ini
[Debug]
ExperimentalAudioLoaderTail=1
```

The switch defaults to `0` until further platform validation. The
independent `ExperimentalAudioHeli1Declick=1` opt-in corrects only two
known `Heli1.wav` samples in a verified **stopped** secondary
DirectSound buffer. It is **not** a partial-sector fallback.

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
were removed from the experimental branch. The original native
loader fix, strict source matching, optional Heli1 declick, and
opt-in read-only DirectSound source diagnostics are retained.

Legacy runtime audio diagnoses (`TAIL-RESTORED`, `TAIL-SKIP`,
Play-time ambiguous-source cases) are documented in the historical
investigation, but are **not** active fallback behavior in the
current experimental build.

## Verification status

The 27 Oct Linux/Wine runs establish correct native restore of both
formerly ambiguous WAV tail classes, and the user's subjective listening
reports are consistently crackle-free, including in that targeted area.
**This does not establish native Windows runtime validation**, nor does
it prove every conceivable edition/resource has been encountered.

After removing the old fallback, verify that CI tests and the Windows
x86 build pass, followed by a fresh Wine playthrough using **both**
DLLs from the new artifact. The key runtime expectations are:
`LOADER-HOOK ARMED`, native restores (including alias messages in the
targeted level), no legacy `TAIL-RESTORE ARMED` or Play-time
`TAIL-SKIP` diagnostics, intact Heli1 declick when enabled,
and continued clean audible playback.

No merge without explicit user approval.

legluondunet assisted by ChatGPT (OpenAI)
