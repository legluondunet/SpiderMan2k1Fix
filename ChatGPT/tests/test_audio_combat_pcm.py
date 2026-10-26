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

def test_optional_manual_f8_arming_with_zero_delay():
    assert "ExperimentalAudioCombatPCMManualF8" in I
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioCombatPCMManualF8",0,path)==1' in N
    assert 'SpiderCombatPCM_Init(' in N and 'SpiderCombatPCM_IsManualF8()' in N
    assert 'SpiderCombatPCM_OnF8(now)' in N
    assert 'SpiderCombatPCM_On3DStart();' in N
    assert 'SpiderCombatPCM_OnFlip(GetTickCount()-native_epoch);' in N

    assert '#define MANUAL_SESSIONS 2u' in S
    assert '#define MANUAL_SESSION_LIMIT 12u' in S
    assert '#define F8_DEBOUNCE_MS 1200u' in S
    assert 'InterlockedIncrement(&f8_uses)' in S
    assert 'session_counts[session-1]++' in S
    assert 'samples[idx].session=manual_mode?(unsigned)session:0u' in S
    assert 'SpiderMan2k1Fix-CombatPCM-R%u-%02u.wav' in S
    assert 'F8-IGNORED maximum two sessions per game' in S
    assert 'InterlockedExchange(&capture_from_ms,(LONG)(native_ms+start_delay_ms))' in S
    assert 'if(manual_mode && (session<1 || session>MANUAL_SESSIONS))return;' in S
    assert 'session!=InterlockedCompareExchange(&f8_triggered,0,0)' in S
    assert 'used=manual_mode?session_counts[session-1]:count' in S
    assert 'SpiderCombatPCM_IsManualF8' in H
    assert 'SpiderCombatPCM_OnF8' in H
    assert 'SpiderCombatPCM_On3DStart' in H
    assert 'SpiderCombatPCM_OnFlip' in H
    assert N.index('SpiderCombatPCM_IsManualF8()') < N.index('native_history_dump(now)')
    assert 'CAPTURE-WINDOW-OPEN' in S
