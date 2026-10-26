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
    assert "730cca4057c9225c80ba1ac09946a38ac9331b1a7b491f437d0bdcd00e3d1700" in SRC
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


def test_exact_one_shot_wav_capture_opt_in():
    assert '"ExperimentalAudioSwing5Capture"' in VALID
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioSwing5Capture",0,path)==1' in NATIVE
    assert "int capture_wav" in SRC and "int capture_wav" in HEADER
    assert "capture_enabled=probe_enabled && capture_wav==1;" in SRC
    assert "InterlockedCompareExchange(&capture_state,1,0)==0" in SRC
    assert "memcpy(captured_pcm,p1,part1_len)" in SRC
    assert "if(part2_len)memcpy(captured_pcm+part1_len,p2,part2_len)" in SRC
    assert SRC.index("IDirectSoundBuffer_Unlock(")<SRC.index("CreateThread(NULL,0,write_captured_wav")
    assert "CREATE_NEW" in SRC and "WriteFile(file,captured_pcm,SUSPECT_BYTES" in SRC
    assert "SpiderMan2k1Fix-AudioCandidate.wav" in SRC
    assert "36u+SUSPECT_BYTES" in SRC and "BYTE header[44]" in SRC
    assert "DeleteFileA(path)" in SRC
    assert "IDirectSoundBuffer_Unlock(" in SRC
    for unsafe in ("IDirectSoundBuffer_SetVolume(","IDirectSoundBuffer_Stop(","IDirectSoundBuffer_Play("):
        assert unsafe not in SRC


def test_capture_is_written_to_user_temp_without_admin():
    assert "GetTempPathA(MAX_PATH,path)" in SRC
    assert "strcpy(path+n,\"SpiderMan2k1Fix-AudioCandidate.wav\")" in SRC
    assert "GetModuleFileNameA(NULL" not in SRC


def test_known_bad_tail_zero_is_opt_in_and_exact_hash_guarded():
    assert '"ExperimentalAudioSwing5TailMute"' in VALID
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioSwing5TailMute",0,path)==1' in NATIVE
    assert "tail_mute_enabled=probe_enabled && tail_mute==1;" in SRC
    assert "5b562348077ddb8cb77edabcd522e1b1789a74c6b7852c86344c1e887f6209ad" in SRC
    assert "#define CORRUPT_TAIL_START 30676u" in SRC
    assert "SUSPECT_BYTES-CORRUPT_TAIL_START" in SRC
    assert 'strcmp(actual,corrupt_pcm_sha256)==0' in SRC
    assert "memset((BYTE*)part1+CORRUPT_TAIL_START" in SRC
    assert "memset((BYTE*)part2+(CORRUPT_TAIL_START-part1_len)" in SRC
    assert "AUDIO-SWING5-TAIL: ZEROED" in SRC
    assert "int capture_wav,int tail_mute" in SRC
    assert "int capture_wav,int tail_mute" in HEADER
    assert SRC.index("memcpy(captured_pcm,p1,part1_len)") < SRC.index('strcmp(actual,corrupt_pcm_sha256)==0')
    assert SRC.index('strcmp(actual,corrupt_pcm_sha256)==0') < SRC.index("IDirectSoundBuffer_Unlock(")
