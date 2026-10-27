"""Safety gates for optional source-verified native DirectSound WAV sector-tail repair."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"pkr_audio_index.c").read_text()
HDR=(ROOT/"pkr_audio_index.h").read_text()
MAIN=(ROOT/"spiderman2k1fix.c").read_text()
HOOK=(ROOT/"sdl3_swing5_redirect.c").read_text()
INI=(ROOT/"ini_validation.h").read_text()

def test_restoration_off_by_default_and_source_index_required():
    assert '"ExperimentalAudioPKRTailRestore"' in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioPKRTailRestore",0,ini)==1' in MAIN
    assert 'SpiderPKRAudioIndex_Init(log_line,pkr_readonly||pkr_tail_restore)' in MAIN
    assert 'SpiderPKRAudioIndex_EnableTailRestore(pkr_tail_restore)' in MAIN
    assert 'SpiderSDL3Swing5_Init(log_line,swing||pkr_readonly||pkr_tail_restore)' in MAIN
    assert 'tail_restore_enabled=(enabled==1 && index_ready)' in SRC
    assert 'if(!tail_restore_enabled||!buf||slot<0||slot>=32)return 0;' in SRC
    assert 'SpiderPKRAudioIndex_TryTailRestore(slot,buf,looping)' in HOOK
    assert HOOK.index('SpiderSDL3Probe_PlaySwing5(slot,buf,volume,pan,hz)') < HOOK.index('SpiderPKRAudioIndex_TryTailRestore(slot,buf,looping)') < HOOK.index('GAME_PLAY(slot,looping); /* Fail OPEN')

def test_never_write_before_source_and_boundary_verification():
    assert 'src' not in []  # no dynamic tests depend on an actual game archive
    for required in (
        'e->crc', 'crc32_pkraw(raw,e->length)==e->crc',
        'dst->header_bytes=(uint32_t)(pcm-raw)',
        'w->header_bytes!=44u',
        'w->pcm_bytes!=caps.dwBufferBytes',
        'w->rate!=fmt.nSamplesPerSec',
        '(status&DSBSTATUS_PLAYING)',
        'IDirectSoundBuffer_GetStatus(buf,&status)',
        'IDirectSoundBuffer_Lock(buf,0,caps.dwBufferBytes',
        'if(source){source=NULL;goto unlock;}',
        'for(j=0;j<MIN_MATCH_BYTES;j++)',
        'for(j=0;j<cut;j++)',
        'if(ds_locked_byte(a,b,a_len,j)!=source->pcm[j])goto unlock;',
        'if(ds_locked_byte(a,b,a_len,cut)==source->pcm[cut])goto unlock;',
        'IDirectSoundBuffer_Unlock(buf,a,a_len,b,b_len)',
        'PKR-AUDIO: TAIL-RESTORED'
    ):
        assert required in SRC
    decision=SRC.index('if(ds_locked_byte(a,b,a_len,cut)==source->pcm[cut])goto unlock;')
    write=SRC.index('memcpy(a+cut,source->pcm+cut,a_len-cut);')
    assert decision<write
    assert SRC.index('source=w;')<decision
    assert 'memcpy(a,source->pcm,' not in SRC
    assert 'memcpy(packed,' not in SRC
    assert 'fopen(path,"rb")' in SRC
    assert 'fopen(path,"wb")' not in SRC
    assert 'return changed;' in SRC
    assert 'MAX_FULL_COMPARE_BYTES' in SRC

def test_every_observed_corruption_begins_after_full_2048_byte_wav_sector():
    # This is the shared signature from the actual user-provided October 27
    # diagnostic log; exact sample names/hashes are NOT encoded into the fix.
    cases=[(60480,59348),(85176,83924),(55444,55252),
           (32632,30676),(10840,10196),(14324,14292),
           (12460,12244),(18008,16340),(69708,69588),
           (4144,4052),(18868,18388),(9080,8148)]
    for n,offset in cases:
        predicted=((n+44)//2048)*2048-44
        assert offset==predicted
        assert 1024<=offset<n
        assert (offset+44)%2048==0
    assert 'cut=((caps.dwBufferBytes+source->header_bytes)/2048u)*2048u-' in SRC
    assert 'source->header_bytes;' in SRC

def test_no_native_game_audio_engine_replacement():
    assert 'GAME_PLAY(slot,looping); /* Fail OPEN' in HOOK
    assert 'IDirectSoundBuffer_Play' not in SRC
    assert 'SpiderSDL3Probe_PlaySwing5' not in SRC
    assert 'IDirectSoundBuffer_SetVolume' not in SRC
    assert 'IDirectSoundBuffer_Stop' not in SRC
