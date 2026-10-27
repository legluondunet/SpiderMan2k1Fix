"""Regression tests: retired DirectSound tail fallback; preserved Heli1 declick.

The native PKR loader is the ONLY supported WAV partial-sector repair.
This module retains its historical path to avoid silently losing regression
coverage after the obsolete Play-time restoration was removed.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "pkr_audio_index.c").read_text()
HDR = (ROOT / "pkr_audio_index.h").read_text()
MAIN = (ROOT / "spiderman2k1fix.c").read_text()
HOOK = (ROOT / "pkr_audio_play_hook.c").read_text()
INI = (ROOT / "ini_validation.h").read_text()
LOADER = (ROOT / "pkr_audio_loader.c").read_text()


def test_legacy_tail_repair_is_entirely_disconnected_and_removed():
    forbidden = (
        "ExperimentalAudioPKRTailRestore", "pkr_tail_restore",
        "SpiderPKRAudioIndex_EnableTailRestore",
        "SpiderPKRAudioIndex_TryTailRestore", "tail_restore_enabled",
        "tail_restore_count", "source_matches_runtime_length",
        "PKR-AUDIO: TAIL-SKIP", "PKR-AUDIO: TAIL-RESTORE ARMED",
    )
    for token in forbidden:
        assert all(token not in src for src in (SRC, HDR, MAIN, HOOK, INI)), token
    assert "SpiderPKRAudioLoader_Init(log_line,pkr_loader_tail);" in MAIN
    assert "SpiderPKRAudioIndex_ExpectedLoaderWavTail" in LOADER
    assert "SpiderPKRAudioIndex_Init(log_line,pkr_index||pkr_heli_declick||pkr_loader_tail)" in MAIN
    assert ('SpiderPKRAudioPlayHook_Init(log_line,'
            'pkr_index||pkr_play_trace||pkr_heli_declick,pkr_play_trace)') in MAIN


def test_native_sector_repair_retains_crc_and_fresh_read_safety():
    assert "crc32_pkraw(raw,e->length)==e->crc" in SRC
    assert 'memcmp(wav,"RIFF",4u)!=0' in SRC
    assert "if(pcm_end<=complete_bytes)continue;" in SRC
    assert "memcmp(wav,w->raw,complete_bytes)!=0" in SRC
    assert "memcmp(match->raw+complete_bytes," in SRC
    assert "w->raw+complete_bytes,remainder)!=0)return NULL;" in SRC
    assert "extra=ORIGINAL_SECTOR_READ(handle,1u,scratch);" in LOADER
    assert "memcmp(scratch,expected,remainder)!=0" in LOADER
    assert "memcpy((unsigned char*)dst+complete,scratch,remainder);" in LOADER
    assert LOADER.index("memcmp(scratch,expected,remainder)!=0") < LOADER.index(
        "memcpy((unsigned char*)dst+complete,scratch,remainder);")


def test_original_native_play_and_heli_declick_remain_separate():
    assert "NATIVE_PLAY(slot,looping);" in HOOK
    assert HOOK.count("NATIVE_PLAY(slot,looping);") == 1
    assert "SpiderPKRAudioIndex_Probe(slot,buffer,looping)" in HOOK
    assert "SpiderPKRAudioIndex_TryHeli1Declick(slot,buffer,looping)" in HOOK
    assert HOOK.index("SpiderPKRAudioIndex_TryHeli1Declick(slot,buffer,looping)") < HOOK.index(
        "NATIVE_PLAY(slot,looping);")
    assert '"ExperimentalAudioHeli1Declick"' in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioHeli1Declick",0,ini)==1' in MAIN
    assert "SpiderPKRAudioIndex_EnableHeli1Declick(pkr_heli_declick);" in MAIN
    assert "heli1_declick_enabled=(enabled==1 && index_ready);" in SRC
    assert "if(!heli1_declick_enabled||!buf||slot<0||slot>=32||!looping)return 0;" in SRC
    assert "strcmp(q->name,\"Heli1.wav\")==0 && q->pcm_bytes==60480u" in SRC
    assert "q->rate==12000u && q->header_bytes==44u" in SRC
    assert "static const DWORD positions[2]={12095u,12109u};" in SRC
    assert "static const int16_t original_samples[2]={-9,32765};" in SRC
    assert "static const int16_t replacements[2]={29634,-30655};" in SRC
    assert "(status&DSBSTATUS_PLAYING)" in SRC
    assert "if(got!=expected && got!=(uint16_t)replacements[k])" in SRC
    assert "else if(got!=expected){valid=0;break;}" in SRC
    assert "PKR-AUDIO: HELI1-DECLICK slot=" in SRC


def test_heli_declick_sample_math_still_matches_original_source():
    orig={12094:30604,12095:-9,12096:28663,12108:-30587,12109:32765,12110:-30723}
    assert (orig[12094]+orig[12096]+1)//2 == 29634
    assert (orig[12108]+orig[12110])//2 == -30655
    assert abs(orig[12109]-orig[12110]) == 63488
    assert abs(-30655-orig[12110]) == 68


def test_source_index_diagnostics_remain_read_only():
    assert "SpiderPKRAudioIndex_Probe(int slot" in SRC
    assert "PKR-AUDIO: FULL-PCM IDENTICAL" in SRC
    assert "PKR-AUDIO: FULL-PCM DIFF" in SRC
    assert "IDirectSoundBuffer_Lock(buf,0,winner->pcm_bytes" in SRC
    assert "IDirectSoundBuffer_Unlock(buf,p,a,q,b)" in SRC
    assert "fopen(path,\"rb\")" in SRC
    assert "fopen(path,\"wb\")" not in SRC
    for forbidden in ("IDirectSoundBuffer_Stop(", "IDirectSoundBuffer_Play("):
        assert forbidden not in SRC
