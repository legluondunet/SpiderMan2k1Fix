"""Guardrails for experimental original-game helicopter loop PCM capture."""
from pathlib import Path
R=Path(__file__).resolve().parents[2]
S=(R/"audio_loop_pcm.c").read_text()
H=(R/"audio_loop_pcm.h").read_text()
N=(R/"audio_native_lifecycle.c").read_text()
I=(R/"ini_validation.h").read_text()
B=(R/"build.sh").read_text()

def test_strict_opt_in_shape():
    assert "#define LOOP_BYTES 60480u" in S
    assert "#define LOOP_HZ 12000u" in S
    assert "#define MAX_LOOPS 2u" in S
    assert "!loop_flag" in S
    assert "fmt.wFormatTag!=WAVE_FORMAT_PCM" in S
    assert "fmt.nChannels!=1u" in S
    assert "fmt.wBitsPerSample!=16u" in S
    assert "fmt.nSamplesPerSec!=LOOP_HZ" in S
    assert "fmt.nBlockAlign!=2u" in S
    assert '"ExperimentalAudioLoopPCMProbe"' in I
    assert '"ExperimentalAudioLoopPCMCapture"' in I
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioLoopPCMProbe",0,path)==1' in N
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioLoopPCMCapture",0,path)==1' in N
    assert N.count("SpiderLoopPCM_Probe(slot,info.buffer,info.bytes,info.hz,looping);")==2
    assert "audio_loop_pcm.c" in B
    assert "SpiderLoopPCM_Probe" in H

def test_game_sound_is_never_mutated():
    assert "IDirectSoundBuffer_Lock(" in S
    assert "IDirectSoundBuffer_Unlock(" in S
    assert S.index("IDirectSoundBuffer_Lock(")<S.index("IDirectSoundBuffer_Unlock(")
    assert "memcpy(pcm[index],part1,a)" in S
    for prohibited in ("IDirectSoundBuffer_Stop(","IDirectSoundBuffer_Play(",
        "IDirectSoundBuffer_SetVolume(","IDirectSoundBuffer_SetPan(",
        "IDirectSoundBuffer_SetFrequency(","IDirectSoundBuffer_Restore(",
        "IDirectSoundBuffer_SetCurrentPosition("):
        assert prohibited not in S

def test_two_wav_copies_and_pcm_checks():
    assert "CryptHashData(hash,pcm[index],LOOP_BYTES,0)" in S
    assert "seam_jump=" in S
    assert "max_internal_jump=" in S
    assert "jumps_ge_12000=" in S
    assert "GetTempPathA(MAX_PATH,filename)" in S
    assert "CREATE_NEW" in S
    assert "SpiderMan2k1Fix-Loop8-%u.wav" in S
    assert "BYTE header[44]" in S
    assert "DeleteFileA(filename)" in S
    assert S.index("IDirectSoundBuffer_Unlock(")<S.index("CreateThread(NULL,0,save_pcm")
