# Native audio ordering hypothesis: read-only helicopter loop evidence

krystalgamer reports an inverted audio playback-order instruction in the
original Spider-Man PC port. The exact RVA and meaning of "order" have not
been confirmed, and must not be guessed. In the current decompilation,
DXSOUND_Play and DXSOUND_Open largely match the original binary, while
DXSOUND_CreateDSBuffer, loadWAV and ParseWavHeader remain TODO stubs.

In our tests: the Swing5 buffer has a corrupted 1956-byte tail at PCM
offset 30676. With ExperimentalAudioStopLoopMask=5, the background sounds
better but two 60480-byte/12000Hz looping buffers, repeating every 2520ms
with ~1180ms phase separation, match two F8-marked helicopter noises.
These are observations, NOT an identified file or proven cause.

### Targeted test with the NEXT diagnostic build

Set under [Debug]:

    Log=1
    ExperimentalAudioNativeLifecycleTrace=1
    ExperimentalAudioLoopPCMProbe=1
    ExperimentalAudioLoopPCMCapture=1
    ExperimentalAudioStopLoopMask=5

Keep ExperimentalAudioSwing5TailMute=1 if it was already enabled. Do NOT
change the mask to 13 for this capture: we need the helicopter loops
playing in order to correlate listening and data.

The probe only sees native voice Play calls. It filters to a looping
60,480-byte buffer at 12,000 Hz with PCM16 mono format, and captures at
most two buffers. Log messages AUDIO-LOOP-PCM include SHA-256, the
sample-to-sample jump across the loop seam, the largest intra-buffer
jump and its sample offset. An optional worker saves the two WAVs under
the user's TEMP folder as SpiderMan2k1Fix-Loop8-1.wav and
SpiderMan2k1Fix-Loop8-2.wav. Files are never overwritten.

These are copied buffers, NOT modified gameplay sounds. Lock/Unlock may
perturb scheduling briefly. Disable both probe and capture after testing.

Compare the SHA-256 and PCM of each capture with its original
PKR-extracted WAV, inspecting whether a corruption occurs at the WAV
loader's transfer boundaries. If the data are pristine, focus on
loop joins / correct scheduler order; do not patch an unknown x86
conditional merely because it sounds like the quoted bug.

Upstream: https://github.com/krystalgamer/spidey-decomp

## 26 Oct 2026 – Combat sound-tail automatic PCM investigation

The user hears a *brief unrelated sound, almost a voice fragment* **at the
end of each punch**, separate from the former 2.52-second helicopter
loop tick. The output recording shows heavy clipping during combat,
but clipping alone does NOT demonstrate the origin of the foreign tail.

New **diagnostic-only** opt-in; no modification of one-shot audio:
[Debug] `ExperimentalAudioNativeLifecycleTrace=1`,
`ExperimentalAudioCombatPCMCapture=1`, and optional
`ExperimentalAudioCombatPCMStartMs=22000` (0..300000, ms since
native 3D initial Flip; increase to target a later fight). Up to
24 *distinct* PCM16 mono one-shot buffers, size 4096..32768 bytes,
sample rate 8k..48k, spaced >=250ms, copied via Lock/Unlock.
The probe is invoked after game Play but **before** any optional
Swing5 tail repair. It writes one valid PCM RIFF file for each unique
capture in %TEMP% as `SpiderMan2k1Fix-CombatPCM-01.wav` etc.
Existing same-name files are never overwritten. File writing,
RIFF construction, and boundary scoring are performed in a worker
thread after DirectSound Unlock; no game audio content or source
assets are changed. The probe has a fixed cap and can potentially
alter timing via read-only Lock/Unlock, so keep OFF outside the
investigation.

The log records source-native time, slot, byte size, sample rate,
bytewise duplicate filter (FNV64 + memcmp), and a **heuristic** for
mean absolute adjacent PCM differences before and after the *last
interior* conventional WAV sector boundary `2048*N` including an
assumed 44-byte RIFF header. Not all WAV headers are 44 bytes; a
large jump does not prove corruption, and no jumps does not
exclude audio scheduling bugs. **Do not auto-repair** based on these
scores. Listen to individual exported sounds and compare exact
original .PKR/WAV source data to decide whether the tail exists
before DirectSound playback. Users can zip the captured WAV files
and send them alongside their native log without pressing F8.

This diagnostic is distinct from the exact-SHA Loop8 and Swing5
workarounds. PR must remain DRAFT until validated.

## 26 Oct 2026 – F8 manual trigger for combat PCM capture

Problem: auto elapsed time `ExperimentalAudioCombatPCMStartMs=22000`
starts before the user can reach the fight, so the 24 unique captures
fill up on unrelated sound effects.

**New opt-in** `[Debug] ExperimentalAudioCombatPCMManualF8=1`:
F8 rising edge in 3D starts the *configured delay timer* exactly once.
`ExperimentalAudioCombatPCMStartMs=0` means capturing starts immediately
on F8. Set 22000 to begin 22 seconds after manually pressing F8,
if desired. No captures before F8, no timer reset on subsequent F8,
and the previous F8 AFTER-CLICK history dump is **suppressed only while
this manual mode is enabled**, to avoid false click correlations. F8
remains available to the original game; only observer behavior changes.
The event log includes `AUDIO-COMBAT-PCM: F8-TRIGGER` and later
`CAPTURE-WINDOW-OPEN`. Exiting/reentering 3D re-arms for the next
scene without overwriting existing capture WAVs; globally max 24
distinct WAVs per process. Existing automatic start mode is unchanged
when ManualF8=0, and all diagnostic options remain OFF by default.

Recommended configuration for punch glitch:
```ini
[Debug]
Log=1
ExperimentalAudioNativeLifecycleTrace=1
ExperimentalAudioCombatPCMCapture=1
ExperimentalAudioCombatPCMManualF8=1
ExperimentalAudioCombatPCMStartMs=0
ExperimentalAudioStopLoopMask=5
ExperimentalAudioLoopTailRepair=1
ExperimentalAudioSwing5Fingerprint=1
ExperimentalAudioSwing5TailMute=1
```
