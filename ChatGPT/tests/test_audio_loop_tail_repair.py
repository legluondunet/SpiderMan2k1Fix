"""Guard the exact-source helicopter tail workaround and both WAV writers."""
from pathlib import Path
R=Path(__file__).resolve().parents[2]
S=(R/"audio_loop_tail_repair.c").read_text()
N=(R/"audio_native_lifecycle.c").read_text()
B=(R/"build.sh").read_text()
I=(R/"ini_validation.h").read_text()

def test_optin_hash_only():
    assert "ExperimentalAudioLoopTailRepair" in I
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioLoopTailRepair",0,path)==1' in N
    assert N.count("SpiderLoopTailRepair_Probe(slot,info.buffer,info.bytes,info.hz,looping);")==2
    assert "audio_loop_tail_repair.c" in B
    assert "#define BAD_OFFSET (29u*2048u-44u)" in S
    assert "#define FADE_SAMPLES 240u" in S
    assert "1a4fdeaf4ba4a60fb43c4bd35ebd2946561cb17c41767e72c8e7559559979cdc" in S
    assert "strcmp(digesthex,bad_sha256)==0" in S
    assert S.index("strcmp(digesthex,bad_sha256)==0")<S.index("for(i=BAD_OFFSET;i<LOOP_BYTES;i++)")
    assert "fmt.wFormatTag!=WAVE_FORMAT_PCM" in S
    assert "fmt.nChannels!=1u" in S
    assert "fmt.wBitsPerSample!=16u" in S
    assert "caps.dwBufferBytes!=LOOP_BYTES" in S
    for bad in ("IDirectSoundBuffer_Play(","IDirectSoundBuffer_Stop(",
        "IDirectSoundBuffer_SetVolume(","IDirectSoundBuffer_SetCurrentPosition("):
        assert bad not in S

def test_pcm_only_and_audio_math():
    assert "CryptHashData(hash,(const BYTE*)a,na,0)" in S
    assert "CryptHashData(hash,(const BYTE*)b,nb,0)" in S
    assert "(v*gain)/(int)(FADE_SAMPLES+1u)" in S
    assert "for(i=BAD_OFFSET;i<LOOP_BYTES;i++)" in S
    assert "IDirectSoundBuffer_Unlock(" in S

def test_riff_header_positions():
    for name in ("audio_loop_pcm.c","audio_swing5_fingerprint.c"):
        s=(R/name).read_text()
        for needle in ('memcpy(header,"RIFF",4u)','memcpy(header+8,"WAVEfmt ",8u)',
                       'memcpy(header+16,&fmtbytes,4u)',
                       'memcpy(header+20,&format,2u)',
                       'memcpy(header+22,&channels,2u)',
                       'memcpy(header+32,&alignbytes,2u)',
                       'memcpy(header+34,&bits,2u)',
                       'memcpy(header+36,"data",4u)'):
            assert needle in s
