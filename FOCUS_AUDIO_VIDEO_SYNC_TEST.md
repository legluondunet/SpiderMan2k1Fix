# Synchronized focus pause — Windows 11 validation and configuration

This document describes the optional Bink and DirectSound portions of the
existing Alt+Tab focus pause. These changes belong to PR #9, separate from
the merged DirectInput recovery in PR #7.

## Recommended test configuration

Use the **existing** `SpiderMan2k1Fix.ini` alongside the exact supported
`SpideyPC.exe`. Back up the file, close the game, and set:

```ini
[Input]
DirectInputReacquire=1
PauseOnFocusLoss=1
PauseKey=Any
RestoreCursorOnFocusLoss=1
PauseBinkOnFocusLoss=1
PauseDirectSoundOnFocusLoss=1
```

Both new audio options **remain disabled by default** in generated INIs.
The menu/game display and individual [Audio] volume settings do not need
to change. [Debug] `Log=1` is optional when collecting one-session logs.

## How it works

**Focus/input:** after a confirmed external focus loss, the game pauses
inside its DirectDraw `Flip` handler. The Windows message queue continues
to be processed. Returning to the game and releasing a wake key resumes the
game, consuming the wake key. `DirectInputReacquire` enables the separate,
previously verified native game's keyboard and mouse recovery path.

**Bink cinematics and music:** the original game imports
`_BinkPause@8` at `0x0053B210`, called at `0x0050BDE5`.
It has separate handles at `0x00AC0BA4` (video) and
`0x00AC1020` (audio-only Bink music). The fix calls the same
Bink API to pause/resume active handles. It avoids unpausing a replaced
handle or music the game itself considers paused.

**DirectSound game effects:** the original game stores up to 32
secondary voice entries (12 bytes each) from `0x006BBD50`.
At focus pause, the fix queries the 32 slots, retains only playing
secondary buffers, saves whether each is looping, and stops it using
its existing `IDirectSoundBuffer` interface. When waking, it resumes
those same buffers only if they are still installed in their original
slots and not already playing, then releases the temporary COM reference.
The primary buffer and other applications' audio are not touched.

The game EXE's SHA-256 verification and several exact in-memory callsite
checks protect these version-specific operations. No game executable
file is modified. Neither module installs a DirectInput or DirectSound
COM vtable hook.

## Windows 11 test evidence — 2026-10-21

The tester validated Alt+Tab in menus, during Bink cinematics and during
real 3D gameplay, with no audio desynchronization after resume. The
`SpiderMan2k1Fix(20261021-060711).log` session confirms:

- Bink cinematic and music handle pause/resume returned the expected
  `BinkPause-result=1` / `BinkPause-result=0` values.
- Menu/cinematic DirectSound voice scans found no occupied secondary
  buffers; no `Stop` calls were made to those slots.
- In 3D, two focus pauses stopped and later resumed **11** and **9**
  game-owned playing secondary buffers, respectively; each operation
  reported `failed=0` and no slot replacements.
- Previously validated Alt+Tab input recovery remained active.

Intermittent audible crackling was reported **under ProcDump**, including
before the two new audio options existed. It could not be reproduced when
launching normally without ProcDump. Its exact cause is **unconfirmed**;
do not claim ProcDump itself is proven responsible. The tester asked to
defer investigating it while the ordinary-launch experience remains good.

This is a validation on one supported original EXE and the tester's
Windows 11 system. It does not establish compatibility across all sound
drivers, Wine versions, or modified binaries.

## Diagnostics and rollback

Enable `[Debug] Log=1` only when troubleshooting and restart the game.
Look for:

```text
FOCUS-MEDIA: Bink pause handle[...]=... BinkPause-result=1
FOCUS-MEDIA: Bink resume handle[...]=... BinkPause-result=0
FOCUS-DSOUND: pause slots=32 occupied=N playing-stopped=M failed=0
FOCUS-DSOUND: resume playing-restored=M slot-replaced=0 failed=0
```

To isolate which component causes a regression, set one option to `0`
and **restart the game**. Turning off DirectSound voice pause may cause
game effects to continue during focus pause; turning off Bink pause may
allow cinematics or music to continue. Both can be disabled without
disabling the previously merged DirectInput recovery. The overall
`PauseOnFocusLoss=1` option controls whether focus pause occurs at all.

If ordinary-launch crackling, hangs or control loss recur, retain the
single-run log and report the scenario. ProcDump may be used for crashes
but is **not required for everyday play** and its full-memory dump
should not be shared publicly.

The PR must be reviewed and merged only after explicit maintainer
approval.

---
legluondunet assisted by ChatGPT (OpenAI)
