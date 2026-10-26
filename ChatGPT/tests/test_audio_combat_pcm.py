"""Opt-in short one-shot PCM capture must not rewrite game DirectSound buffers."""
from pathlib import Path
R=Path(__file__).resolve().parents[2]
S=(R/"audio_combat_pcm.c").read_text()
N=(R/"audio_native_lifecycle.c").read_text()
I=(R/"ini_validation.h").read_text()
B=(R/"build.sh").read_text()
H=(R/"audio_combat_pcm.h").read_text()

def test_feature_is_guarded_and_diagnostic_only():
    assert "ExperimentalAudioCombatPCMCapture" in I
    assert "ExperimentalAudioCombatPCMStartMs" in I
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioCombatPCMCapture",0,path)==1' in N
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioCombatPCMStartMs",22000,path)' in N
    assert "SpiderCombatPCM_Init(" in N and "SpiderCombatPCM_Probe(" in N
    assert "SpiderCombatPCM_Probe(" in H
    assert "audio_combat_pcm.c" in B
    assert "#define LIMIT 24u" in S
    assert "#define PCM_MAX 32768u" in S
    assert "if(!on || !buf || looping" in S
    assert "format.wFormatTag!=WAVE_FORMAT_PCM" in S
    assert "format.nChannels!=1u" in S
    assert "format.wBitsPerSample!=16u" in S
    assert "caps.dwBufferBytes!=size" in S
    assert 'memcmp(samples[i].data,copy,size)==0' in S
    for forbidden in ("IDirectSoundBuffer_Play(","IDirectSoundBuffer_Stop(",
        "IDirectSoundBuffer_SetVolume(","IDirectSoundBuffer_SetPan(",
        "IDirectSoundBuffer_SetFrequency(","IDirectSoundBuffer_SetCurrentPosition(",
        "IDirectSoundBuffer_Restore("):
        assert forbidden not in S

def test_only_snapshots_written_outside_directsound_lock():
    assert S.index("IDirectSoundBuffer_Lock(")<S.index("IDirectSoundBuffer_Unlock(")
    assert 'memcpy(copy,p,len1)' in S
    assert 'memcpy(samples[idx].data,copy,size)' in S
    assert S.index("IDirectSoundBuffer_Unlock(buf,p,len1,q,len2)") < S.index("CreateThread(")
    assert "CREATE_NEW" in S
    assert "SpiderMan2k1Fix-CombatPCM-%02u.wav" in S
    assert 'memcpy(header,"RIFF",4)' in S
    assert 'memcpy(header+36,"data",4)' in S
    assert "DeleteFileA(dir)" in S

def test_captured_before_tail_muting():
    c=N.index("SpiderCombatPCM_Probe(slot,info.buffer,info.bytes")
    m=N.index("SpiderSwing5Fingerprint_Probe(slot,info.buffer")
    assert c<m
