# Native PKR audio restoration (experimental)

## Objective and confirmed behavior

For the SHA-verified 2001 PC `SpideyPC.exe`, the original `data.pkr`
contains 656 valid PCM16 mono WAV entries (all original CRC32 checks
passed on the user's Windows installation). The original game often
copies all complete 2,048-byte *WAV-file* blocks into the native
DirectSound buffers but corrupts the **last partial block**.

Complete byte comparisons against the CRC32-validated PKR WAV show
many different sounds with the same boundary:

```text
(first differing PCM byte + 44-byte WAV header) % 2048 == 0
```

Examples: `Heli1.wav`, `wind2.wav`, `wind3.wav`,
`achum.wav`, `WebShot3.wav`, `ThugStp4.wav`, `Swing5.wav`.
Do not claim the exact faulty executable instruction has been located:
this is a verified buffer-symptom signature, not a disassembly-level
root cause.

The prototype first successfully routed `Swing5.wav` to SDL3, but
a subsequent user test on 2026-10-27 **confirmed that Swing5 also
sounds clean using DirectSound alone after source-backed tail
restoration**. The general repair yielded a major improvement in
helicopters, wind and combat; some short artifacts were still audible
during busy fights. The experimental SDL3 engine and `SDL3.dll`
are therefore no longer needed in the patch build.

## Configuration

In the game's `SpiderMan2k1Fix.ini`:

```ini
[Debug]
Log=1
ExperimentalPKRAudioIndex=1
ExperimentalAudioPKRTailRestore=1
```

Both PKR options default to `0`; either one triggers the read-only
PKR archive index. Tail restoration requires a valid source index and
is enabled **only** by `ExperimentalAudioPKRTailRestore=1`.
For an A/B comparison disable that key and restart the game.

Legacy `ExperimentalSDL3AudioProbe`,
`ExperimentalSDL3Swing5Replace` and
`ExperimentalSDL3PKRAudioIndex` were retired. The INI validator
may remove them during automatic cleanup. No `SDL3.dll` is packaged.
There is no required external `Swing5.wav` test file.

## How the fix works

1. Read `data.pkr` **read-only**, validate PKR3 metadata and per-file
   original CRC32, decode zlib entries via permissively licensed,
   pinned miniz, parse actual RIFF/WAV chunk boundaries, and cache
   compatible PCM16 mono WAVs in memory (bounded).
2. Intercept **one** SHA-gated original 32-voice callsite
   `0x00472870` (the original `CALL DXSOUND_Play` to
   `0x00504230`). Verify its exact original instruction bytes
   before patching. There is **no** native Stop hook or alternative
   playback engine.
3. The read-only index probe optionally identifies the WAV source
   by exact format, PCM byte size and initial PCM prefix, and
   compares full buffers (up to two times per source) for diagnostics.
4. Before native Play, only if the original secondary DirectSound
   buffer is **stopped**, identify a unique CRC-validated original WAV
   by format, sample rate, exact size and **every PCM byte before the
   final incomplete 2,048-byte file sector**. Sources with a
   non-44-byte data offset, ambiguous match or earlier mismatches
   are never modified.
5. The corrupted final partial sector can start with some intact
   bytes. Find the *actual first differing byte anywhere inside that
   final sector*, then restore only the remaining suffix, under a
   single native buffer Lock/Unlock.
6. **Always call the game's original DXSOUND_Play**. The game retains
   its usual DirectSound voice allocation, IsPlaying, looping, stop,
   stereo balance, volume, pitch and focus behavior.

There is no individual list of affected sounds or per-file checksum
workaround. The fix does not write or replace any game archive.

## Useful log lines

```text
PKR-AUDIO: SOURCE-INDEX total=656 valid=656 other=0
PKR-AUDIO: CACHE WAV PCM16 mono=656 bytes=32033406 crc_errors=0 ...
PKR-AUDIO: TAIL-RESTORE ARMED ...
PKR-AUDIO: PLAY-HOOK INSTALLED exact original DXSOUND_Play CALL 0x472870 ...
PKR-AUDIO: FULL-PCM DIFF source=.../Heli1.wav ...
PKR-AUDIO: TAIL-RESTORED slot=... loop=1 source=.../Heli1.wav pcm_bytes=60480 sector_tail_offset=59348 first_diff=59348 restored_bytes=1132 mismatched_bytes=1129 ...
PKR-AUDIO: FULL-PCM IDENTICAL source=... ...
```

A `FULL-PCM DIFF` can be expected: diagnostics compare the buffer
**before** tail restoration. A later `IDENTICAL` is a later,
independent observation and does not prove that the same DirectSound
buffer object was reused.

Remaining audible glitches may arise from buffers the original game
already has playing, unidentified/ambiguous source data, earlier-than-
final-sector differences, or legitimate hard Stops/voice recycling.
To distinguish them, the new code emits bounded:

```text
PKR-AUDIO: TAIL-SKIP reason=playing ...
PKR-AUDIO: TAIL-SKIP reason=ambiguous-source ...
PKR-AUDIO: TAIL-SKIP reason=no-source-prefix ...
PKR-AUDIO: TAIL-SKIP reason=difference-before-final-sector ...
PKR-AUDIO: TAIL-SKIP reason=already-identical ...
PKR-AUDIO: TAIL-SKIP reason=lock-failed ...
```

A skip is **not an error** and never suppresses native Play. Repeated
messages are capped at 24 per reason.

## Diagnostic for remaining short fight noises

The 27 October listening test still reported a brief sound at the end
of some fight effects even after 25 tail repairs (17 distinct WAVs).
All 24 logged tail skips were `already-identical`; there were no
logged `playing`, `no-source-prefix`, or `difference-before-final-sector`
skips. A 216-byte suffix at 22,050 Hz spans less than 5 ms, so a
roughly one-second artifact cannot be attributed to that suffix alone.

To investigate **without modifying playback**, add to `[Debug]`:

```ini
Log=1
ExperimentalPKRAudioIndex=1
ExperimentalAudioPKRTailRestore=1
ExperimentalPKRAudioPlayTrace=1
```

`ExperimentalPKRAudioPlayTrace` is off by default and recognized
by INI validation. It logs a maximum of **256 paired snapshots**
(`PKR-AUDIO: PLAY-TRACE`), immediately before and after the original
`DXSOUND_Play`. Each reports event number, phase, millisecond clock,
voice slot, looping flag, whether source-backed tail repair ran,
buffer pointer, buffer length, sample rate, playing status,
play/write cursors and individual DirectSound query HRESULTs.

This is **read-only telemetry**, not a cure: no native Play/Stop,
SetCurrentPosition, cursor reset, voice recycling, sound routing, or
audio archive changes are introduced. Querying DirectSound and
writing verbose logs may have a small timing cost. Disable the trace
after reproducing a few combat artifacts. Future diagnosis should
correlate cursor and status transitions with an audible repro, and
must not assume a nonzero cursor alone proves a defect.

## Residual combat PCM example: R2-07

An earlier native DirectSound buffer capture,
`SpiderMan2k1Fix-CombatPCM-R2-07.wav`, is mono 16-bit PCM at
16,000 Hz, with 15,140 PCM bytes (0.473125 s). It contains an abrupt
new waveform beginning at **PCM byte 14,292** (0.446625 s), which
corresponds to WAV-file offset **14,336 = 7 * 2,048**. The last 848
file bytes (424 samples; 26.5 ms) are affected. The sample therefore
supports **additional last-partial-sector corruption before playback**,
not solely an incorrect DirectSound play cursor.

In the user's 2026-10-27 `v1.0.0-76f862d` log, two native plays
(`PLAY-TRACE n=145`, slot 27; `n=170`, slot 30) report the same
PCM length and sample rate with `repaired=0`. Both buffers were
stopped and had play cursor zero at launch. Size/rate alone does **not**
prove either is the same original audio as R2-07.

The existing restore path returned silently when no CRC-verified
original had the same PCM length, sample rate, and a **44-byte
WAV header**, even though other known effects were repaired.
Read-only, bounded, deduplicated telemetry now explains this case:

```text
PKR-AUDIO: SOURCE-MISS reason=no-original-size-rate ... pcm_bytes=15140 hz=16000 same_size_rate=0
PKR-AUDIO: SOURCE-MISS reason=nonstandard-original-wav-header ... pcm_bytes=15140 hz=16000 same_size_rate=N
```

The subsequent 27 Oct 2026 `v1.0.0-4444c67` Windows log
**confirmed** `SOURCE-MISS reason=no-original-size-rate` for
`pcm_bytes=15140 hz=16000`, slot 21, matching the size/rate of R2-07.
Across the session 32 distinct unmatched size/rate pairs were logged;
all reported `same_size_rate=0`, while 25 native buffers were tail
restored. The 256-event play trace had expired before this particular
slot-21 report; thus this log cannot establish that effect's play
cursor at its exact playback event. Do not assert that PCM length/rate
alone proves audio identity.

The new **read-only `SOURCE-PREFIX` probe** runs only once for each
new unmatched size/rate combination, subject to the existing
96-combination diagnostic cap. It compares the initial **1,024 exact
PCM bytes** of the stopped buffer against the already CRC-validated
`data.pkr` WAV cache **regardless of original duration or rate**.
The log reports the number of prefix matches, number at the same
sample rate, and the first matched original WAV's name, byte length,
and sample rate, or `matches=0`.

This tells us whether a sound omitted by the exact-size index might
still have a differently sized original in `data.pkr`. A prefix
match is only a **lead**, not proof sufficient to rebuild a tail.
No unknown buffer is patched and DirectSound Play remains native.
If `matches=0` for the 15,140-byte/16 kHz effect, investigating
additional source archives or the native loading path becomes
necessary; guessing the missing waveform is not acceptable.

## Validation boundary

Cross-compilation and regression tests alone cannot establish audio
quality. The widened sector detector, new native-only call hook,
menu/scene re-entry, focus/Alt+Tab, fast successive web swings and
busy combat require renewed Windows/Wine listening tests. If the
experiment introduces audio regressions, turn off
`ExperimentalAudioPKRTailRestore` and restart.

This work remains confined to draft PR #67 until explicit user
approval for a merge.

legluondunet assisted by ChatGPT (OpenAI)
