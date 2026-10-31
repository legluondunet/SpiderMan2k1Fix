# DualShock 4 audio routing — Windows FAQ and draft validation

## Why can Spider-Man be silent while Windows audio works?

Some DualShock 4 connections expose a **playback audio endpoint** as well
as DirectInput controls.  On a tested Windows 11 machine, unplugging the
controller immediately restored **all** game audio, including Bink/video
sound and gameplay effects; reconnecting it silenced the game again.
Disabling only the controller's playback endpoint in `mmsys.cpl`
resolved that local issue.  This observation does **not** by itself
establish which Windows audio-routing component changed the destination.

## What does the setting do?

```ini
[Audio]
DualShock4AudioOutput=0
```

- **0 (factory default):** on **native Windows**, before the original
  sound engine starts, enumerate DirectSound playback devices.  If a
  recognized DualShock 4 audio endpoint and a non-controller alternative
  are both present, provide the non-controller device GUID to the
  game's **original DirectSoundCreate8** call.  Use the first physical
  alternative enumerated, which Windows enumerates as the current
  default when available.  If identification or creation fails, retry
  the original game request instead of leaving the game without sound.
- **1:** preserve the original game's device selection, including when
  using a headset plugged into a DualShock 4.
- **Wine:** currently leaves the original device untouched, regardless
  of this setting, until there is separate native-vs-Wine verification.

**This option is game-local.** It does not disable the DualShock 4's
Windows audio playback device, change the default Windows endpoint,
modify volume settings for other applications, or affect controller input.
Changing the INI option requires restarting the game. Older INIs without
this new key behave as `0`; a newly generated INI contains it.

The game uses one DirectSound8 object for native sound effects and
passes it to Bink's DirectSound backend for video and music. This is
why interception at the game's original device creation is preferable
to independently modifying multiple audio playback paths. However,
the proposed behavior is **not yet proven audible on Windows 11**;
per-application Windows audio preferences may supersede a specific
DirectSound device selection.

## What if I still hear nothing?

First check Windows Settings → System → Sound → Volume mixer →
Spider-Man and its application-specific output. Alternatively, run
`mmsys.cpl` → **Playback** and select the intended output. If needed,
disable only the **playback** audio endpoint of the DualShock 4 in
Windows while leaving the controller itself enabled.

With `[Debug] Log=1`, search for `AUDIO-ROUTING:`.
An `armed` line means the hook is installed, **not** that audio is audible.
A `selected` line means the DirectSound device creation succeeded,
**not** that Windows subsequently delivered audible samples.
If the DirectSound import is missing, enumeration is ambiguous or no
non-DS4 output is available, the feature keeps original behavior.

## Release validation matrix (must pass before merge)

- [ ] Windows 11, real DS4 connected before start: all Bink intro audio,
      menu SFX, gameplay effects and music audible on speakers.
- [ ] Real DS4 + headset deliberately connected to its jack:
      `DualShock4AudioOutput=1` restores its original routing.
- [ ] Unplug and reconnect DS4 at runtime: no crashes, no controller
      regressions; changing the sound endpoint mid-session is **not**
      guaranteed without restarting the sound engine.
- [ ] Windows 11 without DS4: original default is preserved.
- [ ] Windows 11 with DS4 but no other playback endpoint: original
      behavior preserved; no forced mute or system changes.
- [ ] Multiple non-controller audio outputs: the output chosen is
      consistent with the first enumerated valid physical endpoint.
- [ ] Confirm Windows per-application output selection does not override
      the explicit DirectSound GUID; otherwise revisit routing strategy.
- [ ] Wine and Proton: no regressions to output device selection,
      effects, music, video or controller input.
- [ ] Review the x86 PE import hook installation, restoration/failure
      semantics, and signed/unsigned/UTF-8 warning cleanliness.
- [ ] GitHub Actions Windows x86 build and source/INI tests green.

The behavior must not be described as verified until the real hardware
tests above pass. No changes to the original `SpideyPC.exe` on disk.

---
legluondunet assisted by ChatGPT (OpenAI)
