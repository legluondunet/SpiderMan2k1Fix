"""Targeted read-only PCM fingerprint in the native SFX diagnostic branch."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"audio_swing5_fingerprint.c").read_text()
HEADER=(ROOT/"audio_swing5_fingerprint.h").read_text()
NATIVE=(ROOT/"audio_native_lifecycle.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
BUILD=(ROOT/"build.sh").read_text()

def test_exact_candidate_shape_and_expected_digest():
    assert "32632u" in SRC
    assert "16000u" in SRC
    assert "MAX_PROBES 12u" in SRC
    assert "abb6d1b0863ea9c44fc0b718115f7e0821383d90a58f7cd22d9cf6e447fb8639" in SRC
    for constraint in ("fmt.wFormatTag!=WAVE_FORMAT_PCM",
                       "fmt.nChannels!=1u","fmt.wBitsPerSample!=16u",
                       "fmt.nSamplesPerSec!=SUSPECT_HZ",
                       "bytes!=SUSPECT_BYTES","frequency!=SUSPECT_HZ"):
        assert constraint in SRC

def test_fingerprint_is_opt_in_and_preserves_game_audio():
    assert '"ExperimentalAudioSwing5Fingerprint"' in VALID
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioSwing5Fingerprint",0,path)==1' in NATIVE
    assert '"ExperimentalAudioNativeLifecycleTrace",0,path)!=1' in NATIVE
    assert 'SpiderSwing5Fingerprint_Probe(slot,info.buffer,info.bytes,info.hz);' in NATIVE
    assert 'SpiderSwing5Fingerprint_Init(native_log,' in NATIVE
    assert "MAX_PROBES" in SRC
    assert "IDirectSoundBuffer_Lock(" in SRC and "IDirectSoundBuffer_Unlock(" in SRC
    assert SRC.index("IDirectSoundBuffer_Lock(") < SRC.index("IDirectSoundBuffer_Unlock(")
    for unsafe in ("IDirectSoundBuffer_Stop(","IDirectSoundBuffer_Play(",
                   "IDirectSoundBuffer_SetVolume(","IDirectSoundBuffer_SetFrequency(",
                   "IDirectSoundBuffer_Restore(","IDirectSoundBuffer_SetCurrentPosition("):
        assert unsafe not in SRC
    assert "CryptHashData(hash,(const BYTE*)part1,part1_len,0)" in SRC
    assert "CryptHashData(hash,(const BYTE*)part2,part2_len,0)" in SRC
    assert '"MATCH":"DIFFERENT"' in SRC
    assert "audio_swing5_fingerprint.c" in BUILD
    assert "SpiderSwing5Fingerprint_Probe(" in HEADER
