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
