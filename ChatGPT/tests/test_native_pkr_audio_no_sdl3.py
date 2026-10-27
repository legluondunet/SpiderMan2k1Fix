"""The experimental audio fix runs with stock DirectSound and only two DLLs."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAIN = (ROOT/"spiderman2k1fix.c").read_text()
HOOK = (ROOT/"pkr_audio_play_hook.c").read_text()
HEAD = (ROOT/"pkr_audio_play_hook.h").read_text()
INDEX = (ROOT/"pkr_audio_index.c").read_text()
INI = (ROOT/"ini_validation.h").read_text()
BUILD = (ROOT/"build.sh").read_text()
CI = (ROOT/".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()

def test_native_only_src_and_no_sdl3_runtime_packaging():
    for path in ("sdl3_audio_probe.c", "sdl3_audio_probe.h",
                 "sdl3_swing5_redirect.c", "sdl3_swing5_redirect.h",
                 "ChatGPT/tests/test_sdl3_audio_probe.py",
                 "ChatGPT/tests/test_sdl3_swing5_redirect.py",
                 "docs/references/diagnostics/audio/SDL3_AUDIO_PROBE.md"):
        assert not (ROOT/path).exists()
    assert (ROOT/"docs/references/diagnostics/audio/PKR_NATIVE_AUDIO_REPAIR.md").exists()
    for content in (BUILD,CI,MAIN,HOOK,INDEX,HEAD,INI):
        assert 'SDL3.dll' not in content
        assert 'sdl3_audio_probe' not in content
        assert 'sdl3_swing5_redirect' not in content
    assert 'pkr_audio_play_hook.c pkr_audio_index.c' in BUILD
    assert 'pkr_audio_play_hook.c' not in CI  # compilation remains in build.sh
    assert 'Bundle pinned SDL3 x86 DLL' not in CI
    assert 'cp SpiderMan2k1Fix.dll winmm.dll README.md CHANGELOG.md dist/package/' in CI

def test_verified_original_callsite_and_preserved_playback_semantics():
    assert '0x00472870u' in HOOK
    assert '0x00504230u' in HOOK
    assert '0x006BBD50u' in HOOK
    assert '0xE8,0xBB,0x19,0x09,0x00' in HOOK
    assert 'GetModuleHandleA(NULL)!=GAME_BASE' in HOOK
    assert 'memcmp(call,expected_call,sizeof(expected_call))!=0' in HOOK
    assert 'VirtualProtect(call,sizeof(expected_call),PAGE_EXECUTE_READWRITE,&old)' in HOOK
    assert 'FlushInstructionCache(GetCurrentProcess(),call,sizeof(expected_call))' in HOOK
    assert 'memcpy(call,expected_call,sizeof(expected_call))' in HOOK
    assert 'NATIVE_PLAY(slot,looping);' in HOOK
    assert HOOK.count('NATIVE_PLAY(slot,looping);')==1
    assert 'SpiderPKRAudioIndex_Probe(slot,buffer,looping)' in HOOK
    assert 'SpiderPKRAudioIndex_TryTailRestore(slot,buffer,looping)' not in HOOK
    assert 'SpiderPKRAudioIndex_TryHeli1Declick(slot,buffer,looping)' in HOOK
    assert 'SpiderPKRAudioPlayHook_Init(log_line,pkr_index||pkr_play_trace||pkr_heli_declick,pkr_play_trace)' in MAIN
    assert 'SpiderPKRAudioIndex_EnableTailRestore' not in MAIN
    assert 'SpiderSDL3' not in MAIN
    assert 'DXSOUND_Stop' not in HOOK
    assert 'IDirectSoundBuffer_Play(' not in HOOK

def test_native_settings_default_off_and_obsolete_keys_removed():
    assert '"ExperimentalPKRAudioIndex"' in INI
    assert '"ExperimentalAudioPKRTailRestore"' not in INI
    assert '"ExperimentalSDL3' not in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalPKRAudioIndex",0,ini)==1' in MAIN
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioLoaderTail",0,ini)==1' in MAIN
    assert 'SpiderPKRAudioIndex_Init(log_line,pkr_index||pkr_heli_declick||pkr_loader_tail)' in MAIN
    assert 'tail_restore_enabled' not in INDEX
    assert 'PKR-AUDIO: PLAY-HOOK INSTALLED' in HOOK
    assert 'PKR-AUDIO: TAIL-SKIP reason=%s' not in INDEX

def test_loader_keeps_crc_verified_exact_partial_sector_guards():
    assert 'SpiderPKRAudioIndex_ExpectedLoaderWavTail(' in INDEX
    assert 'if(pcm_end<=complete_bytes)continue;' in INDEX
    assert 'memcmp(wav,w->raw,complete_bytes)!=0' in INDEX
    assert 'memcmp(match->raw+complete_bytes,' in INDEX
    assert 'w->raw+complete_bytes,remainder)!=0)return NULL;' in INDEX
    assert 'fopen(path,"rb")' in INDEX
    assert 'fopen(path,"wb")' not in INDEX
    assert 'PKR-AUDIO: HELI1-DECLICK' in INDEX
    assert 'PKR-AUDIO: TAIL-RESTORED' not in INDEX
