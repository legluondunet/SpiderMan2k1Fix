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
   by format, sample rate, original PCM size (or its strictly validated
   4-byte-aligned **+2-byte native allocation**) and **every PCM byte
   before the final incomplete 2,048-byte file sector**. Sources with
   a non-44-byte data offset, ambiguous match or earlier mismatches
   are never modified.
5. The corrupted final partial sector can start with some intact
   bytes. Find the *actual first differing byte anywhere inside that
   final sector*, then restore only the remaining suffix, under a
   single native buffer Lock/Unlock. Where the native allocation has
   exactly two extra alignment bytes, fill that one PCM16 sample
   with silence rather than reading beyond the original data.
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

## Confirmed 4-byte DirectSound allocation and ThugHit2 repair

The subsequent 27 Oct `v1.0.0-7a497fd` log identified the
R2-07 format (15,140 PCM bytes, 16,000 Hz) as an unambiguous
**1,024-byte PCM-prefix match** for the original
`data\\audio/ThugHit2.wav` (15,138 PCM bytes, 16,000 Hz).
The same event, `PLAY-TRACE n=160` / slot 19, was stopped with
play cursor 0 before native playback and reported `repaired=0`.

Of 26 previously unmatched length/rate pairs, **25** had one
original of the **same rate and identical first 1,024 bytes** whose
length was exactly **2 bytes shorter**. All 25 source lengths are
`N % 4 == 2`, and all runtime lengths are `N+2`, divisible by 4.
This is strong evidence of a **native 4-byte buffer-size
alignment**, although the exact original engine allocation
instruction has not yet been disassembled. The previously ambiguous
79,308-byte looping buffer had **two** sources sharing its first
1,024 bytes (`traffic.wav` and `traffic2.wav`), so the forensic
prefix was insufficient for identity. However, the later full
pre-sector identity gate **uniquely identified** the exact +2-byte
source `traffic2.wav` (79,306 original PCM bytes), and its
79,308-byte stopped looping buffer was successfully restored.

For `ThugHit2.wav`, the PCM mismatch in R2-07 starts at
`14,292 + 44 = 14,336 = 7 * 2,048`, just as with the
other repaired original WAVs. The source has 846 PCM bytes after
this boundary and the padded native buffer has 848 bytes
(26.5 ms at 16 kHz).

The new draft repair accepts an original only when all the
existing strict identity and safety conditions hold, plus one
*specifically constrained* length alternative:

- Runtime buffer is exactly original PCM length, **or**
  original PCM length is `4k+2` and native buffer length is
  exactly `N+2`, aligned to 4 bytes.
- An extra two bytes must not cross into a new 2,048-byte
  WAV-file sector. The same standard 44-byte header, sample
  rate, entire matching pre-sector PCM, single unique source,
  stopped DirectSound buffer and Lock/Unlock checks still apply.
- The matched original trailing PCM is copied exactly and
  the **one extra 16-bit sample** is filled with silence
  (`0x0000`); no source is ever read out of bounds.
- The existing exact-length repair remains unchanged. The log
  adds `buffer_bytes` and `pad_bytes` to
  `TAIL-RESTORED`, so padded repairs are auditable.
- Arbitrary truncated/sliced loops, ambiguous originals,
  buffers already playing and any earlier PCM mismatch remain
  untouched. The 79,308-byte `traffic2.wav` loop is a verified
  +2-byte-aligned original, **not** an arbitrary truncation.

**Listening test passed for combat, 27 Oct 2026.** The user
explicitly confirmed that fight sounds are now OK. The supplied
`v1.0.0-6ad8871` in-game log shows **49 native
`TAIL-RESTORED` events across 49 distinct original WAVs**:
**27** required precisely two allocation padding bytes and
**22** had exact original PCM length. Among them:

```text
PKR-AUDIO: TAIL-RESTORED slot=25 loop=0 source=data\\audio\\/ThugHit2.wav pcm_bytes=15138 buffer_bytes=15140 pad_bytes=2 sector_tail_offset=14292 first_diff=14292 restored_bytes=848 mismatched_bytes=840 occurrence=1 (native DirectSound preserved)
PKR-AUDIO: PLAY-TRACE n=188 phase=before ... slot=25 ... repaired=1 ... bytes=15140 hz=16000 status=0x00000000 play=0 write=0 ...
PKR-AUDIO: PLAY-TRACE n=188 phase=after ... slot=25 ... repaired=1 ... status=0x00000001 ...
PKR-AUDIO: TAIL-RESTORED slot=2 loop=1 source=data\\audio\\/traffic2.wav pcm_bytes=79306 buffer_bytes=79308 pad_bytes=2 sector_tail_offset=77780 ... (native DirectSound preserved)
```

The log has **zero** `SOURCE-MISS`, **zero** DirectSound
HRESULT failures in the 256 traced Play pairs, **zero**
PKR-AUDIO warnings/errors, and **24** `TAIL-SKIP` messages
all due to `already-identical` data. It also reached gameplay
and returned via native 3D cleanup to the menu.

This validates the combat symptom and this in-game audio path,
**not** every level, sound, focus-transition case or Windows/Wine
configuration. If later use reveals a regression, set
`ExperimentalAudioPKRTailRestore=0` and restart the game.

## Open issue: helicopter loop contains repeated impulses (27 Oct 2026)

After validating the fight sounds by listening, the user identified
two closely spaced unwanted clicks while approaching a helicopter.
Their 30.015-second stereo 48-kHz PCM recording,
`spider-man-audio(5).wav`, exhibits particularly strong, narrow
impulses around **0.704 s and 1.524 s**, then **3.224 s and
4.044 s** and so on, in a pattern repeating every **2.520 s**
throughout the recording. A whole-waveform correlation at a
2.520-second offset is about **0.842**. This precisely equals
`Heli1.wav`'s original PCM duration:
**60,480 bytes / (12,000 samples/s × 2 bytes/sample) = 2.520 s**.

The earlier Windows in-game log confirmed that the first `Heli1.wav`
native DirectSound loop was corrected from the CRC32-verified
original: `TAIL-RESTORED ... source=.../Heli1.wav
pcm_bytes=60480 buffer_bytes=60480 pad_bytes=0
sector_tail_offset=59348 restored_bytes=1132`.
The second concurrent helicopter buffer matched original PCM
exactly. **The presence of repeated clicks after tail repair does
not demonstrate renewed final-sector corruption.** Their exact
relationship to the original asset and/or native playback processing
is not yet proven. In particular, two impulses inside each full
2.520-second period cannot be explained solely by one ordinary
buffer-wrap discontinuity.

The **read-only** source diagnostic inspects CRC32-validated
`Heli1.wav` PCM in memory at initialization. The subsequent
27 Oct `v1.0.0-c547dcb` Windows log confirms **both original
Heli1 plays** in distinct locations:

- Second 3D cutscene: first `Heli1.wav` at slot 14 with
  `loop=1`, **1,132 bytes** restored (all late-sector damage);
  a concurrently started slot 15 `Heli1.wav` buffer was already
  PCM-identical to the original.
- Near the bank: slot 10 `Heli1.wav`, `loop=1`, again
  **1,132 bytes** restored.

Original source (from `data.pkr`, 656/656 CRC32-verified WAVs):
`HELI1-SOURCE loop_seam_delta=703`, but internal sample-to-sample
jumps of **30,719 at 779.917ms**, **63,488 at 1009.167ms**,
and **20,803 at 1386.083ms**. These are signed-sample differences,
not repaired-buffer mismatch counts, and reveal several striking
internal discontinuities even inside the original WAV. The weak
loop seam and the two audible impulses per repeating 2.520s
recorded pattern disfavor the theory of a *single loop-wrap click*.
The specific waveform intervals causing the two audible anomalies
are not yet unambiguously mapped: 2.520s periodicity alone does
not prove the source is responsible, and source jumps may be
intentional rotor transients.

**Next exact source comparison, opt-in only:** add
`ExperimentalAudioHeli1SourceDump=1` under `[Debug]`.
This writes a single new **60,524-byte** PCM WAV
(44-byte standard WAV header + 60,480 bytes unmodified original PCM)
to the current user's **Windows TEMP** on startup, using
`CREATE_NEW` (never overwrite), provided `data.pkr` verified
successfully. Look for:
`PKR-AUDIO: HELI1-DUMP CREATED C:\\...\\SpiderMan2k1Fix-Heli1-Original-<PID>.wav`.
The source is never played by this diagnostic, the archive and
DirectSound buffers are not modified, and the default is **0**.
Upload this small temporary WAV so we can compare its actual
impulse waveforms to `spider-man-audio(5).wav` offline before
considering any optional, source-specific declick. The dump
is independent from tail-repair and should be disabled after
inspection.



Cross-compilation and regression tests alone cannot establish audio
quality. The widened sector detector, new native-only call hook,
menu/scene re-entry, focus/Alt+Tab, fast successive web swings and
busy combat require renewed Windows/Wine listening tests. If the
experiment introduces audio regressions, turn off
`ExperimentalAudioPKRTailRestore` and restart.

This work remains confined to draft PR #67 until explicit user
approval for a merge.

legluondunet assisted by ChatGPT (OpenAI)
