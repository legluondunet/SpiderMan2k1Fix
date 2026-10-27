"""Native DirectSound WAV tail repair: fail-open integrity and identity guards."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "pkr_audio_index.c").read_text()
HDR = (ROOT / "pkr_audio_index.h").read_text()
MAIN = (ROOT / "spiderman2k1fix.c").read_text()
HOOK = (ROOT / "pkr_audio_play_hook.c").read_text()
INI = (ROOT / "ini_validation.h").read_text()

def test_restore_opt_in_and_no_sdl_runtime():
    assert '"ExperimentalAudioPKRTailRestore"' in INI
    assert '"ExperimentalPKRAudioIndex"' in INI
    assert "ExperimentalSDL3" not in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioPKRTailRestore",0,ini)==1' in MAIN
    assert 'SpiderPKRAudioIndex_Init(log_line,pkr_index||pkr_tail_restore)' in MAIN
    assert 'SpiderPKRAudioIndex_EnableTailRestore(pkr_tail_restore)' in MAIN
    assert 'SpiderPKRAudioPlayHook_Init(log_line,pkr_index||pkr_tail_restore||pkr_play_trace,pkr_play_trace)' in MAIN
    assert 'tail_restore_enabled=(enabled==1 && index_ready)' in SRC
    assert 'if(!tail_restore_enabled||!buf||slot<0||slot>=32)return 0;' in SRC
    assert 'SpiderPKRAudioIndex_TryTailRestore(slot,buffer,looping)' in HOOK
    assert HOOK.index('SpiderPKRAudioIndex_Probe(slot,buffer,looping)') < HOOK.index('SpiderPKRAudioIndex_TryTailRestore(slot,buffer,looping)') < HOOK.index('NATIVE_PLAY(slot,looping);')
    assert "SDL3" not in MAIN

def test_crc_verified_unique_original_source_before_any_pcm_write():
    for required in (
        'crc32_pkraw(raw,e->length)==e->crc',
        'dst->header_bytes=(uint32_t)(pcm-raw)',
        'w->header_bytes!=44u',
        'w->pcm_bytes!=caps.dwBufferBytes',
        'w->rate!=fmt.nSamplesPerSec',
        'if(status&DSBSTATUS_PLAYING)',
        'IDirectSoundBuffer_GetStatus(buf,&status)',
        'IDirectSoundBuffer_Lock(buf,0,caps.dwBufferBytes',
        'if(j!=MIN_MATCH_BYTES)continue;',
        'if(j!=sector_start)',
        'matching_sources>1u',
        'if(matching_sources!=1u)',
        'for(j=MIN_MATCH_BYTES;j<sector_start;j++)',
        'for(j=sector_start;j<caps.dwBufferBytes;j++)',
        'if(first_diff==UINT32_MAX)first_diff=j;',
        'if(first_diff==UINT32_MAX)',
        'IDirectSoundBuffer_Unlock(buf,a,a_len,b,b_len)',
        'PKR-AUDIO: TAIL-RESTORED'
    ):
        assert required in SRC
    boundary=SRC.index('if(matching_sources!=1u)')
    discrepancy=SRC.index('if(first_diff==UINT32_MAX)')
    write=SRC.index('memcpy(a+first_diff,source->pcm+first_diff,a_len-first_diff);')
    assert boundary < discrepancy < write
    assert 'memcpy(a,source->pcm,' not in SRC
    assert 'fopen(path,"rb")' in SRC
    assert 'fopen(path,"wb")' not in SRC
    assert 'return changed;' in SRC
    assert 'MAX_FULL_COMPARE_BYTES' in SRC

def test_sector_math_original_reported_corruption():
    observed=[(60480,59348),(85176,83924),(55444,55252),
              (32632,30676),(10840,10196),(14324,14292),
              (12460,12244),(15596,14292),(69708,69588),
              (4144,4052),(18872,18388),(13664,12244)]
    for n,offset in observed:
        cut=((n+44)//2048)*2048-44
        assert offset==cut
        assert (offset+44)%2048==0
        assert 1024<=offset<n
    assert 'sector_start=((caps.dwBufferBytes+44u)/2048u)*2048u-44u;' in SRC

def test_later_partial_sector_damage_can_be_repaired_without_rewriting_intact_head():
    sector=bytes(range(256))*8
    source=sector+bytes(range(128))
    damaged=bytearray(source)
    first_fault=len(sector)+17
    damaged[first_fault:] = bytes(len(damaged)-first_fault)
    assert damaged[:len(sector)]==source[:len(sector)]
    assert damaged[len(sector):first_fault]==source[len(sector):first_fault]
    assert damaged[first_fault:]!=source[first_fault:]
    damaged[first_fault:]=source[first_fault:]
    assert bytes(damaged)==source
    assert 'memcpy(a+first_diff,source->pcm+first_diff,a_len-first_diff);' in SRC
    assert 'PKR-AUDIO: TAIL-SKIP reason=%s' in SRC

def test_native_play_untouched_and_no_sdl_dependency():
    assert 'NATIVE_PLAY(slot,looping);' in HOOK
    assert 'DXSOUND_Play' in HOOK
    assert 'DXSOUND_Stop' not in HOOK
    assert 'GetModuleHandleA(NULL)!=GAME_BASE' in HOOK
    assert 'memcmp(call,expected_call,sizeof(expected_call))!=0' in HOOK
    assert '0xE8,0xBB,0x19,0x09,0x00' in HOOK
    assert 'FlushInstructionCache' in HOOK
    assert 'VirtualProtect(' in HOOK
    assert 'IDirectSoundBuffer_Play(' not in SRC
    assert 'IDirectSoundBuffer_Stop(' not in SRC
    assert 'SDL3' not in SRC


def test_unmatched_pcm_formats_logged_once_without_changing_native_play():
    """An unknown PCM length/rate should explain a source miss, not get patched."""
    assert 'MAX_SOURCE_MISS_LOGS 96u' in SRC
    assert 'PKR-AUDIO: SOURCE-MISS reason=%s' in SRC
    assert '"nonstandard-original-wav-header"' in SRC
    assert '"no-original-size-rate"' in SRC
    assert 'source_misses[i].bytes==bytes && source_misses[i].rate==rate' in SRC
    assert 'if(source_miss_count>=MAX_SOURCE_MISS_LOGS)return;' in SRC
    assert 'if(sources[i].header_bytes==44u)candidates++;' in SRC
    assert 'log_source_miss(slot,looping,caps.dwBufferBytes,' in SRC
    # Fail-open remains before any full-buffer DirectSound lock or write.
    begin=SRC.index('int SpiderPKRAudioIndex_TryTailRestore(')
    body=SRC[begin:]
    assert body.index('if(!candidates){') < body.index('IDirectSoundBuffer_Lock(')
    assert body.index('log_source_miss(') < body.index('IDirectSoundBuffer_Lock(')
    assert 'NATIVE_PLAY(slot,looping);' in HOOK
