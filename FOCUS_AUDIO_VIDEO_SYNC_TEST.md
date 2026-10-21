# Experimental synchronized focus pause — native Bink, DirectSound investigation

**Not a complete 3D audio fix. Do not merge into `main` before native Windows 11 tests.**

## Confirmed from the original, SHA-256-supported game executable

The 32-bit executable imports `_BinkPause@8` through its Import Address Table at
virtual address `0x0053B210`, and **calls it itself** at `0x0050BDE5`
with exactly two arguments: the movie handle and the pause state.
Two movie-handle globals are used at `0x00AC0BA4` and `0x00AC1020`.
The game's native pause flag for the latter is `0x00AC101D`.

Separately, it imports the DirectSound factory by ordinal 11, calls it via
`0x00517A70`, and stores the resulting game sound device in
`0x006B7920`, with another interface/buffer at `0x006B7924`.
This is **not** an enumeration of the active secondary sound buffers.

## First PR stage: optional native Bink pause

Add to the user's **existing** `SpiderMan2k1Fix.ini`:

```ini
[Debug]
Log=1

[Input]
DirectInputReacquire=1
PauseOnFocusLoss=1
PauseKey=Any
PauseBinkOnFocusLoss=1
```

The new option is **0 by default** in all factory INI templates.
The original (now merged) Alt+Tab input fix is unchanged. When a real
external focus loss latches the Flip pause, the experimental code calls
the same `BinkPause(handle,1)` API imported by the game, for any currently
registered movie handles. On resumption, it calls `BinkPause(handle,0)`
**only if the handle has not changed** and the game's own movie pause flag
does not indicate it was paused independently.

If an exact native callsite signature, module or import check fails,
the feature disables itself. It never replaces a Bink export or DirectInput
or DirectSound COM vtable. Movie handle lifetime may involve other threads,
so even with these guards, Windows/ProcDump tests are mandatory before merge.

Expected diagnostic lines (when enabled):

```text
FOCUS-MEDIA: experimental native BinkPause enabled; DirectSound audio NOT paused
FOCUS-MEDIA: Bink pause handle[0]=... BinkPause-result=...
FOCUS-MEDIA: Bink resume handle[0]=... BinkPause-result=...
```

If `Bink` handle messages are absent, no eligible movie was playing at that
specific Flip pause boundary.

## Stage 1: DirectSound was not paused (superseded by stage 2 below)

Some **3D music** appears to be streamed using the audio-only Bink handle at \`0x00AC1020\`, so this experiment may also pause that music. **Other game sound effects and DirectSound audio** run outside the Bink movie pause. The code
records the **read-only** original game's DirectSound device and primary
buffer candidate pointers on focus loss/resume:

```text
FOCUS-AUDIO: pause DSOUND device=... primary-candidate=...; no sound-buffer changes
FOCUS-AUDIO: resume DSOUND device=... primary-candidate=...; no sound-buffer changes
```

This **does not stop** 3D audio. Stopping the primary DirectSound buffer is
not enough: secondary buffers may continue playing even after the primary
buffer's `Stop` method, depending on cooperative level. Correct 3D audio
pause requires locating all active game-owned secondary buffers or a
verified game-native music/SFX pause function, preserving playback cursors
and playback flags, handling streaming sound and lifetime safely.

Do not bluntly hook arbitrary COM vtables: an older DirectInput COM probe
caused an `E_INVALIDARG` startup regression on Windows 11. A separate
observation/validation stage is necessary before any sound-buffer control.

## Safe test under ProcDump

From PowerShell in the game directory (with ProcDump in `ProcDump`):

```powershell
Remove-Item Env:\SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force .\crash-dumps | Out-Null
& '.\ProcDump\procdump.exe' -accepteula -ma -e -x '.\crash-dumps' '.\SpideyPC.exe'
```

1. Confirm unchanged menu/3D control recovery after Alt+Tab.
2. Pause and resume during a **Bink cinematic**, then check whether video
   and its own audio are now synchronized and the movie can still be skipped.
3. In **3D gameplay**, pause and note **exactly which sounds** continue:
   background music, ambient loop, dialogue, sound effects, or all.
4. Keep the log/INI. If there is a crash, keep the ProcDump memory dump
   private, as process memory can contain sensitive data.
5. If movie behavior regresses, revert to `PauseBinkOnFocusLoss=0` only.
   The previously validated Alt+Tab input behavior remains unchanged.

## Status

- Original DirectInput retry and Any-key wake validated on native Windows 11.
- Native Bink video/audio/music pause: **implemented behind opt-in; UNTESTED**.
- DirectSound secondary 3D voice pause: **implemented as separate opt-in in stage 2, NOT YET VALIDATED**.

---
legluondunet assisted by ChatGPT (OpenAI)

## Stage 2: experimental DirectSound secondary voices (2026-10-21)

**User validation of stage 1:** Bink cinematic streams and Bink-backed music now
pause and resume successfully. In the provided `20261021-054813` log,
14 focus pauses and wake-ups succeeded, Bink handle[0] and handle[1]
show `BinkPause-result=1` / `BinkPause-result=0`, and native
DirectInput error recovery reached keyboard=4, mouse=4. The one audible
channel remaining in 3D is **suspected** to be separate DirectSound audio,
because that test explicitly logged `no sound-buffer changes`.

A more precise offline inspection of the same exact SHA-verified game EXE
reveals **32 game-owned secondary DirectSound voice slots**, which the game
allocates as 12-byte records starting at virtual address `0x006BBD50`.
At `0x005039F0`, initialization clears 0x60 DWORDs = 32 slots × 12 bytes;
at `0x00504110`, it duplicates a source buffer into an indexed voice slot;
at `0x00504230` and `0x005042A0`, it plays/stops these voices using the
original DirectSound interface. The **primary** buffer at `0x006BBF1C`
is excluded.

Add this **new** option to the existing INI, with the other validated options:

```ini
[Debug]
Log=1

[Input]
DirectInputReacquire=1
PauseOnFocusLoss=1
PauseKey=Any
PauseBinkOnFocusLoss=1
PauseDirectSoundOnFocusLoss=1
```

Both media pause options default to **0** in factory configurations.
DirectSound support is an independent opt-in; disabling it leaves the Bink
pause and Alt+Tab input fix working as before.

The experimental code uses **only the verified original game voice table**,
not any COM-vtable/IAT interception. On the established focus Flip pause,
it queries each existing secondary buffer's `GetStatus`, retains the
actually playing buffers via `AddRef`, and `Stop`s only those buffers.
On resume, it checks whether each slot still points to the same buffer,
then `Play`s the stopped voices from the preserved playback cursor and with
their original looping flag. It releases all temporary references. It does
not stop the primary buffer, the Bink buffers, already stopped voices, or
any other application's audio. Static callsite signatures and the original
EXE hash must match before enabling the feature.

Expected additional log lines:

```text
FOCUS-DSOUND: experimental secondary-voice pause enabled; 32 game-owned slots, primary excluded
FOCUS-DSOUND: pause slots=32 occupied=N playing-stopped=M failed=0
FOCUS-DSOUND: resume playing-restored=M slot-replaced=0 failed=0
```

**Essential test**: launch under ProcDump. Pause with an audible remaining
3D effect or ambience, wait 5–10 seconds, and resume. Verify that it
actually stops, resumes naturally without pitch/cursor reset, and that
ordinary new gameplay sound effects still play afterwards. Also check
cinematics and menus for regressions. If no secondary voice is playing
(`playing-stopped=0`) while audible audio continues, **do not increase
hook aggressiveness**: identify the other source using further diagnostics.

**Rollback:** set `PauseDirectSoundOnFocusLoss=0`. Leave the validated
`PauseBinkOnFocusLoss=1`, `PauseKey=Any`, and `DirectInputReacquire=1`
unchanged. This experiment remains a draft PR, not a stable release.

---
legluondunet assisted by ChatGPT (OpenAI)
