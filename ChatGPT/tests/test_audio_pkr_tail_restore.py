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
    assert 'SpiderPKRAudioIndex_Init(log_line,pkr_index||pkr_tail_restore||pkr_heli_dump)' in MAIN
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
    assert 'if(source_miss_count>=MAX_SOURCE_MISS_LOGS)return 0;' in SRC
    assert 'if(sources[i].header_bytes==44u)candidates++;' in SRC
    assert 'log_source_miss(slot,looping,caps.dwBufferBytes,' in SRC
    # Fail-open remains before any full-buffer DirectSound lock or write.
    begin=SRC.index('int SpiderPKRAudioIndex_TryTailRestore(')
    body=SRC[begin:]
    assert body.index('if(!candidates){') < body.index('IDirectSoundBuffer_Lock(')
    assert body.index('log_source_miss(') < body.index('IDirectSoundBuffer_Lock(')
    assert 'NATIVE_PLAY(slot,looping);' in HOOK


def test_unmatched_source_prefix_probe_is_bounded_read_only_and_never_fixes_unknown_wavs():
    """The next R2-07 probe searches by PCM prefix, but must never patch."""
    assert 'static void probe_source_miss_prefix(' in SRC
    assert 'PKR-AUDIO: SOURCE-PREFIX slot=%d' in SRC
    assert 'matches=0 (READ-ONLY; no repair)' in SRC
    assert 'prefix_matches++' in SRC
    assert 'if(w->rate==rate)same_rate++;' in SRC
    assert 'if(a_len && memcmp(w->pcm,a,a_len)!=0)continue;' in SRC
    assert 'if(b_len && memcmp(w->pcm+a_len,b,b_len)!=0)continue;' in SRC
    assert '(status&DSBSTATUS_PLAYING))return;' in SRC
    assert 'IDirectSoundBuffer_Lock(buf,0,MIN_MATCH_BYTES,' in SRC
    assert 'IDirectSoundBuffer_Unlock(buf,a,a_len,b,b_len)' in SRC
    assert 'if(log_source_miss(slot,looping,caps.dwBufferBytes,' in SRC
    # Only one probe per unique length/rate, subject to the 96-entry cap.
    assert 'source_misses[i].bytes==bytes && source_misses[i].rate==rate)return 0;' in SRC
    assert 'if(source_miss_count>=MAX_SOURCE_MISS_LOGS)return 0;' in SRC
    probe=SRC.split('static void probe_source_miss_prefix(',1)[1].split('int SpiderPKRAudioIndex_TryTailRestore(',1)[0]
    assert 'memcpy(' not in probe
    assert 'IDirectSoundBuffer_Play(' not in probe
    assert 'IDirectSoundBuffer_SetCurrentPosition(' not in probe


def test_four_byte_alignment_tail_repair_safety():
    """R2-07 / ThugHit2: original 15138 bytes, game buffer 15140."""
    source_bytes = 15138
    native_bytes = 15140
    assert source_bytes % 4 == 2
    assert native_bytes == source_bytes + 2
    sector_start = ((native_bytes + 44) // 2048) * 2048 - 44
    assert sector_start == 14292
    assert source_bytes - sector_start == 846
    assert native_bytes - sector_start == 848  # 26.5 ms at 16 kHz
    # Our repair must preserve the entire source prefix and restore
    # even a partially intact final WAV sector, without out-of-bounds read.
    source = bytes((i * 13 + 7) % 256 for i in range(source_bytes))
    damaged = bytearray(source + b"\x90\xA1")
    damaged[sector_start:] = b"\xDA" * (native_bytes - sector_start)
    assert damaged[:sector_start] == source[:sector_start]
    first_diff = next(i for i in range(sector_start, native_bytes)
                      if damaged[i] != (source[i] if i < source_bytes else 0))
    assert first_diff >= sector_start
    damaged[first_diff:] = source[first_diff:] + b"\0\0"
    assert damaged == source + b"\0\0"
    assert 'source_matches_runtime_length(' in SRC
    assert '(w->pcm_bytes&3u)==2u' in SRC
    assert 'n==w->pcm_bytes+2u && (n&3u)==0u' in SRC
    assert '(w->pcm_bytes+44u)/2048u==(n+44u)/2048u' in SRC
    assert 'uint8_t expected=j<source->pcm_bytes?source->pcm[j]:0u;' in SRC
    assert 'if(source->pcm_bytes==caps.dwBufferBytes)' in SRC
    assert 'pad_bytes=%lu' in SRC
    assert 'return changed;' in SRC


def test_alignment_repair_still_rejects_large_or_wrongly_aligned_buffers():
    def eligible(original, native):
        return original == native or (
            original % 4 == 2
            and native == original + 2
            and native % 4 == 0
            and (original + 44) // 2048 == (native + 44) // 2048
        )

    assert eligible(15138, 15140)   # ThugHit2 WAV -> padded native DS
    assert eligible(10232, 10232)   # existing exact-sized repairs
    assert eligible(43746, 43748)   # PickConf WAV
    assert not eligible(15138, 15142)
    assert not eligible(15138, 15138 - 2)
    assert not eligible(255480, 79308)  # truncated traffic loop
    assert not eligible(2002, 2004)  # +2 would cross file-sector boundary
    assert not eligible(10232, 10234)  # source was already DWORD aligned
    assert 'matching_sources>1u' in SRC
    assert 'if(matching_sources!=1u)' in SRC
    assert 'if(status&DSBSTATUS_PLAYING)' in SRC
    assert 'w->header_bytes!=44u' in SRC
    assert 'if(j!=sector_start)' in SRC


def test_heli1_source_discontinuity_probe_is_read_only_and_matches_recording_period():
    """The user's 30s capture repeats at 2.520s, exactly Heli1's PCM duration."""
    assert (60480 // 2) / 12000 == 2.52
    assert 'static void log_heli1_source_discontinuities(void)' in SRC
    assert 'if(decoded>0 && crc_fail==0)log_heli1_source_discontinuities();' in SRC
    assert 'strcmp(w->name,"Heli1.wav")!=0' in SRC
    assert 'w->rate!=12000u' in SRC
    assert 'w->pcm_bytes!=60480u' in SRC
    assert 'w->header_bytes!=44u' in SRC
    assert 'PKR-AUDIO: HELI1-SOURCE original=' in SRC
    assert 'PKR-AUDIO: HELI1-SOURCE-JUMP rank=' in SRC
    probe=SRC.split('static void log_heli1_source_discontinuities(void){',1)[1].split('static int load_audio_index(',1)[0]
    assert 'le16(w->pcm' in probe
    assert 'diag(line)' in probe
    for forbidden in ('fopen(', 'fwrite(', 'memcpy(', 'IDirectSoundBuffer_', 'NATIVE_PLAY('):
        assert forbidden not in probe


def test_heli1_source_export_is_opt_in_non_destructive_and_temp_only():
    assert '"ExperimentalAudioHeli1SourceDump"' in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioHeli1SourceDump",0,ini)==1' in MAIN
    assert 'SpiderPKRAudioIndex_SetHeli1SourceDump(pkr_heli_dump);' in MAIN
    assert 'void SpiderPKRAudioIndex_SetHeli1SourceDump(int enabled);' in HDR
    assert 'heli1_source_dump_enabled=(enabled==1);' in SRC
    assert 'if(!heli1_source_dump_enabled)return;' in SRC
    assert 'if(decoded>0 && crc_fail==0)' in SRC
    assert 'dump_heli1_source_optin();' in SRC
    assert 'w->pcm_bytes==60480u && w->rate==12000u' in SRC
    assert 'sources[i].header_bytes==44u && sources[i].pcm' in SRC
    assert 'GetTempPathA(' in SRC
    assert 'CREATE_NEW' in SRC
    assert 'PKR-AUDIO: HELI1-DUMP CREATED' in SRC
    assert 'w->pcm_bytes' in SRC
    assert 'WriteFile(out,w->pcm,w->pcm_bytes' in SRC
    probe=SRC.split('static void dump_heli1_source_optin(void){',1)[1].split('static void log_heli1_source_discontinuities(void){',1)[0]
    assert 'IDirectSoundBuffer_' not in probe
    assert 'SetCurrentPosition' not in probe
    assert 'NATIVE_PLAY' not in probe
    assert 'data.pkr' not in probe.replace('never data.pkr', '')
    assert 'FILE_SHARE_READ,NULL,CREATE_NEW' in probe
    assert 'DeleteFileA(path);' in probe  # delete an incomplete new dump only
    assert 44+60480 == 60524
