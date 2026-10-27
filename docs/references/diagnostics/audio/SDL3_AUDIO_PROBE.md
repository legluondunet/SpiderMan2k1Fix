# SDL3 isolated audio prototype

**Status:** experiment, not a replacement sound engine.

The game's native DirectSound, source PKR handling and original game
voices remain entirely unchanged. SDL3 independently decodes and plays
a chosen ORIGINAL WAV. This establishes whether the source audio can
play cleanly without Spider-Man's loader and DirectSound pipeline.

## Test with the Windows x86 Actions artifact

1. Install `winmm.dll`, `SpiderMan2k1Fix.dll` and `SDL3.dll` next to
   the original supported `SpideyPC.exe`.
2. Extract/copy an **unaltered original WAV**, such as `Swing5.wav`,
   from your own original `data.pkr`. Save a copy next to the EXE as
   `SpiderMan2k1Fix-SDL3-test.wav`.
3. In `SpiderMan2k1Fix.ini`:
   ```ini
   [Debug]
   Log=1
   ExperimentalSDL3AudioProbe=1
   ```
4. Launch the game, find a quiet location, and press **F7** once.
   SDL3 plays the file once, outside the game's normal voice system.
   Another F7 replays. Game focus is required.
5. Inspect `SpiderMan2k1Fix.log` for
   `SDL3-PROBE: ARMED`, `SDL3-PROBE: audio initialized`, `SDL3-PROBE: PLAY`
   or an actionable error (missing DLL, missing WAV, device failure).

Use `ExperimentalSDL3AudioProbe=0` afterward. F8 diagnostics remain
independent. No game audio is routed to SDL3 yet; comparing the
isolated WAV to gameplay sound is an **auditory feasibility test**, not
proof that SDL3 fixes the original game-side corruption.

Runtime SDL3 3.4.18 x86 is downloaded by CI from the official
https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18 and
its ZIP SHA-256 verified. SDL3 is under the zlib license; the
SpiderMan2k1Fix sources continue under the repository's LICENSE.

Next step after listening: identify the precise source-to-PCM corruption,
and only then add an optional full-engine mode that faithfully handles
32 voices, volume/pan/pitch/loops/Stop/lifetime and avoids double sound.

legluondunet assisted by ChatGPT (OpenAI)

## Phase 2: SDL3 automatically replaces only Swing5 in gameplay

**User's phase-1 F7 test (Windows) passed:** 18 successful source WAV
playbacks, no audible distortion; all calls to SDL3 were successful.

The first native gameplay integration is now separately OFF by default.
Keep the same original `SpiderMan2k1Fix-SDL3-test.wav` copy, and use:

```ini
[Debug]
Log=1
ExperimentalSDL3AudioProbe=0
ExperimentalSDL3Swing5Replace=1
```

Start a level and swing repeatedly. You do **not** need to press F7.
Observe whether swing sounds are audible, clean, respect volume/pitch,
whether other game audio still works, and whether game pause/return
to menu works. In the log, confirm:

```
SDL3-SWING5: ARMED
SDL3-SWING5: INSTALLED exact game CALL 0x472870
SDL3-SWING5: REPLACED slot=... original_pcm=32632 ...
```

Precise behavior:
- The one-shot `CALL DXSOUND_Play` at 0x00472870 is intercepted.
  Additionally, the native `DXSOUND_Stop` entry (0x005042A0)
  has a signature-checked 8-byte x86 trampoline; the original native
  Stop runs after SDL3 voice cancellation.
- Check that the native DirectSound voice buffer contains **exactly**
  32632 bytes, PCM16 mono @16 kHz, and that its **full PCM SHA-256**
  is the already-confirmed malformed Swing5 fingerprint.
- Separately ensure SDL3 loads and SHA-256-verifies the user's original
  clean Swing5.wav copy, with correct PCM format, byte length and 16 kHz
  sample rate. No other game asset can be substituted via filename alone.
- When both checks pass, re-create stereo samples, applying the native
  DirectSound pan/volume attenuation and native pitch/frequency, then
  start an SDL3 audio stream. Only after SDL3 successfully queues and
  resumes the audio do we skip the original `DXSOUND_Play` on that
  one voice; the original 2 game voice metadata bytes are maintained.
- All other effects and looping sounds keep native DirectSound.
  Any failure to hash, load SDL3, find the exact source, allocate
  a buffer, or initialize a sound stream **falls back** to native Play.
  No changes to original PKR contents, WAV contents, DirectSound
  PCM bytes, or game audio voice allocation.
- **Phase 3 lifecycle (needs in-game validation):** native Stop cancels
  the matching SDL3 voice; a new native Play on the same slot first
  releases any previous SDL3 voice; slot buffer replacement is detected
  at the Flip boundary using a retained DirectSound COM reference.
  One-shots expire after their estimated PCM duration plus 120ms device
  margin. Alt+Tab confirmed-focus pause now suspends/resumes SDL3
  devices alongside the original DirectSound voices. Return to menu
  closes remaining SDL3 voices. Current native volume, pan, and
  frequency are sampled each Flip; when a change occurs the remaining
  *clean* source frames are rebuilt and queued on a new stream. Mid-sound
  changes can still create a short discontinuity because the audio
  device has some output latency.
- **Remaining limitations:** this remains a single-sound proof, NOT a
  32-voice replacement or a proven general WAV/PKR loader repair.
  Original PCM is loaded from a separate user-provided file on first
  match, and its initial disk read may stall that one frame. The
  native DirectSound slot remains stopped (instead of reporting
  `DSBSTATUS_PLAYING`) during its corresponding SDL3 sound. Any other
  native Play caller not passing through the gated 0x00472870 site can
  therefore bypass our replacement tracking. Native unload and full
  process shutdown lifecycle still need dedicated validation before
  expanding to other effects and loops.
- Note that *no repair is attempted on the malformed native buffer*:
  this test proves only that source-original data can be played
  instead of the game-decoded version on the matching one-shot.
- A missing or invalid original WAV means **no replacement**, never
  silence. To compare, set `ExperimentalSDL3Swing5Replace=0` and
  relaunch the game.

Do not enable separate PR #66 tail-repair experiments in this test:
this branch is based on `main`. The existing F7 probe may still be
enabled independently if needed. Do not merge this PR until the
in-game Swing5 replacement has been verified by the user.

legluondunet assisted by ChatGPT (OpenAI)

## Phase 3 test plan – native lifecycle

The user already confirmed `Swing5` sounds clean in-game (six REPLACED
events in the previous log). With the new phase-3 CI build, keep the
same two INI switches: `ExperimentalSDL3AudioProbe=0` and
`ExperimentalSDL3Swing5Replace=1`.

1. Swing repeatedly in one gameplay segment, vary direction and
   camera position and listen for unexpected truncations, doubled
   effects or crackles.
2. During a swing, Alt+Tab to another application. Confirm the game
   and SDL3 effect **pause together** and resume after the explicit
   focus-return key. Check `SDL3-VOICE: PAUSE affected=...` and
   `SDL3-VOICE: RESUME affected=...`.
3. Return to the main menu and re-enter gameplay. Check no stale
   swing keeps sounding from the previous session; log
   `SDL3-VOICE: STOP ... cause=return-to-menu` when applicable.
4. Repeat swings quickly to trigger slot reuse/native Stop; watch
   `SDL3-VOICE: STOP ... cause=native-DXSOUND_Stop` or
   `native-voice-replay`, and ensure the other voices remain audible.
5. Examine `SDL3-VOICE: UPDATE` for actual mid-sound volume/pan/hz
   changes and compare timbre. These are logged only for up to 32
   updates to limit the trace volume.

No Steam, installer, source PKR, game configuration archive or original
WAV is modified by these tests. The option remains opt-in. CI unit
tests and compilation are NOT substitutes for listening and gameplay
testing under Windows/Wine.

legluondunet assisted by ChatGPT (OpenAI)


## Phase 4 – Automatic source identification from data.pkr (READ ONLY)

This is the first step towards a whole-game SDL3 backend without
hundreds of per-sample checksums. It is **NOT yet** the whole-game
replacement; all sounds except the already-validated Swing5 remain on
DirectSound. No WAVs need to be individually extracted for this test.

Enable on a new test run:

```ini
[Debug]
Log=1
ExperimentalSDL3AudioProbe=0
ExperimentalSDL3Swing5Replace=1
ExperimentalSDL3PKRAudioIndex=1
```

The DLL reads the original `data.pkr` **read-only from the same
directory as SpideyPC.exe**. Its private PKR3 decoder:
- checks the archive magic, footer, directory/file counts, file offsets
  and stored/uncompressed sizes against the original archive length;
- accepts uncompressed (-2) or zlib-compressed (+2) entries using a
  pinned, MIT-licensed `miniz` inflate component;
- validates the **CRC32** of every decoded WAV candidate before its
  PCM data may enter the in-memory index;
- validates RIFF/WAVE chunk boundaries and native mono PCM16 format,
  including the *actual* WAV `data` chunk position (does **not**
  assume a fixed 44-byte header);
- caches only supported mono16 PCM WAVs, up to 2,048 samples,
  16 MiB per WAV, 128 MiB total. The exact byte contents are kept
  in memory; original game archives and buffers are never written.

On each 32-voice native Play (including loops) a **passive** source
probe checks rate and exact PCM byte size. Only compatible candidates
cause a short 1,024-byte DirectSound buffer read. A unique matching
prefix gives a log like:

```
PKR-AUDIO: SOURCE-INDEX total=... valid=... other=...
PKR-AUDIO: CACHE WAV PCM16 mono=... bytes=... crc_errors=0 (READ-ONLY; no SDL3 override)
PKR-AUDIO: PROBE ARMED ...
PKR-AUDIO: MATCH slot=... loop=0 original=.../SomeSound.wav bytes=... hz=... matching_prefix=1024 candidates=1 (READ-ONLY)
```

A partial head match is **not a full proof of source identity**. If
multiple entries share the first 1,024 bytes, the program reports
`AMBIGUOUS`. A buffer corrupted earlier in its first 1,024 bytes may
not match. Neither case may be automatically redirected to SDL3
without stronger verification and sound resource lifecycle parity.
The game continues normal DirectSound playback for every sound except
the existing SHA-protected Swing5 substitution. Do not mistake
`PKR-AUDIO: MATCH` for `SDL3-SWING5: REPLACED`.

There is no full PCM game-source comparison in the present build;
source-identification diagnostics are intentionally limited to **192 distinct WAV sources**
logged once each to control log volume while keeping later game effects visible. No unsupported WAV is silently
treated as valid PCM. A missing, invalid or CRC-failing archive
disables this feature, leaving the normal game path alone. Do not use
a different `data.pkr` from a different game/version.

The next step (not yet implemented) requires checking a full original
WAV identity, handling native `DXSOUND_IsPlaying` and resource
allocation, and then allowing verified one-shot source PCM to be
played by SDL3 without suppressing legitimate DirectSound sounds.

### Third-party decoder

`third_party/miniz/` contains the minimum pinned upstream 2026-10-04
sources from `richgel999/miniz`, commit
`2948eebafc5b9f1c69bebed80333e403a09bd62d`, under its own
permissive MIT license copied verbatim into
`third_party/miniz/LICENSE`. This license remains applicable to the
vendored source. Unused ZIP-writing and deflate APIs are disabled.

### Testing limitations

Synthetic RIFF/zlib/CRC test vectors and static constraints are
tested automatically. The user's **real** `data.pkr` archive is
not available in this source tree or CI, so we must check the
`PKR-AUDIO: SOURCE-INDEX` / `MATCH` output in a real game session.
A successful 32-bit DLL compilation alone does not validate the PKR
archive inventory against the actual game data.

legluondunet assisted by ChatGPT (OpenAI)

## 27 October – Real PKR index validation and whole-PCM comparison

User run `SpiderMan2k1Fix(20261027-000444).log` **confirmed
the original game archive scanner works on Windows**:
`PKR-AUDIO: SOURCE-INDEX total=656 valid=656 other=0`,
`CACHE ... bytes=32033406 crc_errors=0`; no archive failure.
48 native Play-to-PKR match records were logged before the old fixed
48-message cap, covering seven original assets:
`achum.wav`, `wind2.wav`, `wind3.wav`, `Heli1.wav`,
`eleczap0.wav`, `ThugStp1.wav`, `ThugStp4.wav`.
`Heli1.wav` is the 60480-byte 12 kHz native loop in slots 27/28.
Seven clean Swing5 replacements on the existing SDL3 path were also
recorded, with normal one-shot voice expirations. No generic audio
replacement occurred; gameplay still uses DirectSound for all other
sounds.

**Important distinction:** this log predates the improved *one
source per log line* build, so 48 logged records contain heavy repeated
footstep/electrical effects. The 1024-byte prefix does not establish
whether the corrupted suffix is present in a native secondary buffer.

### New opt-in FULL-PCM read-only diagnostic

Keep the same `ExperimentalSDL3PKRAudioIndex=1`. On each uniquely
identified original WAV, also read the **entire** game-owned
DirectSound PCM buffer via Lock/Unlock and compare it byte for byte
with the CRC-verified original WAV already decoded in the source
index. Compare no more than 2 times per distinct source, 256
comparisons total, and only when the sample is at most 1 MiB. This
bounded read-only check does not write PCM data, alter native playback
or use source samples as replacement audio.

The log reports either

```
PKR-AUDIO: FULL-PCM IDENTICAL source=data\audio\/Heli1.wav slot=27 loop=1 bytes=60480 compare=1/2 (READ-ONLY)
```

or

```
PKR-AUDIO: FULL-PCM DIFF source=... slot=... loop=... bytes=...
          first_diff=... last_diff=... mismatched_bytes=...
          first_mod_2048=... compare=1/2 (READ-ONLY)
```

The `first_mod_2048` field is relative to raw *PCM*, not to a RIFF
header; do not conflate it with the previously observed WAV-file
offset alignment. A mismatch is proof of divergence **at that
buffer-read point**, not a proof that the original PKR loader is the
cause. An identical PCM does not rule out loop transition, mixing or
sound-device glitches. The user should provide another full log
after hearing the recurring helicopter sound and several fight effects.

Only once clean PKR sources and their native identities are
established should we expand SDL3 automatic playback. Native
`DXSOUND_IsPlaying` parity for redirected voices and loop lifetime
handling remain blockers for general audio takeover.

legluondunet assisted by ChatGPT (OpenAI)


## Phase 5 – User log establishes a universal 2,048-byte WAV tail fault

Real log `SpiderMan2k1Fix(20261027-001614).log` from the supported EXE:

- **53 complete PCM comparisons across 30 distinct game audio resources**.
- **53/53 DIFF, zero IDENTICAL** in this particular sampled set.
- **Every first differing PCM byte** falls at the boundary
  `(first_diff + 44) % 2048 == 0`, i.e. the start of a new
  2,048-byte block of a standard RIFF/WAV with 44-byte PCM header.
- The entire pre-boundary prefix matches the CRC-verified original
  archive WAV, and corruption lies in the trailing partial block.
  Examples:
  `Heli1.wav` bytes=60480 first_diff=59348;
  `wind2.wav` 85176/83924;
  `Swing5.wav` 32632/30676;
  `WebShot3.wav` 15596/14292;
  `ThugStp4.wav` 10840/10196;
  `SpStep2.wav` 14324/14292.
- This strongly suggests a **shared incomplete final-block copy
  or sector-accounting bug** during loading/copying of original WAV
  data into game-owned DirectSound buffers. The responsible native
  instruction is NOT YET located. We must not claim the archive
  itself is corrupt: all 656 original WAV entries passed CRC32.

### Phase 5 experimental universal tail restoration (OFF by default)

Instead of adding one SHA-256 case for every damaged sound or prematurely
emulating all 32 DirectSound voices, test a **source-backed repair of
only the demonstrated corrupted final partial WAV block** before the
normal native `DXSOUND_Play`.

Use the following in the same `[Debug]` section:

```ini
Log=1
ExperimentalSDL3AudioProbe=0
ExperimentalSDL3Swing5Replace=1
ExperimentalSDL3PKRAudioIndex=1
ExperimentalAudioPKRTailRestore=1
```

With `ExperimentalAudioPKRTailRestore=0`, all game-owned native
DirectSound buffers are left unchanged as before. The new opt-in repair
works only after the CRC-verified original PKR cache is available.
It requires *all* the following:

1. A native 32-slot `DXSOUND_Play` call, mono PCM16 and rate
   exactly matching one source, buffer up to 1MiB.
2. A **stopped** secondary DirectSound buffer. Already-playing
   buffers are excluded to avoid changing samples during playback.
3. One and only one original source in `data.pkr` matching the exact
   buffer byte size, sample rate, and original first 1024 PCM bytes.
4. That original WAV's actual PCM data offset is exactly 44 bytes.
   Other WAV layouts are not altered.
5. Every byte of the game buffer **up to the final full 2048-byte
   WAV sector** is identical to the original PCM data.
6. The first byte at that expected sector boundary **does** differ
   from the original. If another region differs, or the buffer was
   already intact, leave the entire buffer alone.

Only then, under the *same* DirectSound Lock/Unlock, restore the
partial final sector from the original CRC-verified WAV in memory.
The original `DXSOUND_Play` still executes. There is no gameplay
sound engine rerouting, no change to native `IsPlaying`, voice
allocation, volume, pitch, pan, loops, Stop or pause.

Expected distinct first events:

```
PKR-AUDIO: TAIL-RESTORE ARMED ...
PKR-AUDIO: FULL-PCM DIFF source=.../Heli1.wav ...
PKR-AUDIO: TAIL-RESTORED slot=... loop=1 source=.../Heli1.wav pcm_bytes=60480 sector_tail_offset=59348 restored_bytes=1132 occurrence=1 (native DirectSound preserved)
```

The `FULL-PCM DIFF` record is expected because the read-only probe
runs **before** the repair at each native Play. `TAIL-RESTORED`
confirms restoration took place. To compare, set
`ExperimentalAudioPKRTailRestore=0` and restart.

The existing Swing5-specific SDL3 playback has **priority**. If it
successfully substitutes the audio, the original DirectSound buffer is
never played and no tail restoration is necessary. If SDL3 is
disabled/fails, the source-backed general repair can restore Swing5
before native Play as well.

### Real-world validation still required

This is a new experiment, **NOT a validated fix**. Verify recurring
helicopter sound, wind loops and combat/grenade/web effects; listen
for residual clicks, pitch/frequency changes, voice truncations or
concurrent sound glitches. Watch for missing sound effects, crashes,
or unexpected pauses. If they occur, turn off only
`ExperimentalAudioPKRTailRestore`; the previous stable
Swing5 SDL3 behavior will remain available.

The 2048-byte alignment strongly suggests a loading-time problem but
does not yet identify the responsible game function. Even successful
restoration does not prove all source WAVs or other sound paths
(including movie soundtracks, streaming effects or other PCM
formats) are covered.

Keep this test only in DRAFT PR #67, separate from PR #66; do not
merge before user gameplay validation.

legluondunet assisted by ChatGPT (OpenAI)
