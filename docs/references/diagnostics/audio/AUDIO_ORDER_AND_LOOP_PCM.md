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
