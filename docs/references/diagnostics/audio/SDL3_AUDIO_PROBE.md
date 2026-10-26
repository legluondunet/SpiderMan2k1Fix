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
- The only intercepted original game instruction is the x86
  `CALL DXSOUND_Play` at 0x00472870 for one-shot sound.
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
- **Prototype limitations:** native game Stop and pause/focus transitions
  do not yet own/stop these SDL3 streams; they can continue playing
  briefly across Alt+Tab or game events. Source PCM is read on first
  matching sound, so a one-time disk read may briefly delay that frame.
  Original native buffer is left unplayed and its slot is available for
  immediate reuse, which may be observable in fast combat. These
  behaviors must be tested before broadening the engine migration.
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
