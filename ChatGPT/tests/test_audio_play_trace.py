"""Optional DirectSound voice trace: bounded, read-only, no audio behavior changes."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HOOK = (ROOT / "pkr_audio_play_hook.c").read_text()
HEAD = (ROOT / "pkr_audio_play_hook.h").read_text()
MAIN = (ROOT / "spiderman2k1fix.c").read_text()
INI = (ROOT / "ini_validation.h").read_text()

def test_trace_is_explicitly_opt_in_and_ini_validator_knows_it():
    assert '"ExperimentalPKRAudioPlayTrace"' in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalPKRAudioPlayTrace",0,ini)==1' in MAIN
    assert 'SpiderPKRAudioPlayHook_Init(log_line,pkr_index||pkr_tail_restore||pkr_play_trace||pkr_heli_declick,pkr_play_trace)' in MAIN
    assert 'SpiderPKRAudioPlayHook_Init(void (*logger)(const char*),int enabled,int trace)' in HEAD
    assert 'trace_enabled=(trace==1);' in HOOK

def test_trace_uses_exact_native_play_and_read_only_queries():
    assert 'MAX_PLAY_TRACE 256u' in HOOK
    assert 'trace_count<MAX_PLAY_TRACE' in HOOK
    assert 'trace_voice(n,"before"' in HOOK
    assert 'trace_voice(n,"after"' in HOOK
    assert 'slot_buffer(slot)' in HOOK
    assert 'repaired=SpiderPKRAudioIndex_TryTailRestore(slot,buffer,looping);' in HOOK
    assert HOOK.count('NATIVE_PLAY(slot,looping);') == 1
    assert HOOK.index('trace_voice(n,"before"') < HOOK.index('NATIVE_PLAY(slot,looping);') < HOOK.index('trace_voice(n,"after"')
    for read_api in ('GetCaps(', 'GetFormat(', 'GetStatus(', 'GetCurrentPosition('):
        assert 'IDirectSoundBuffer_'+read_api in HOOK
    for mutator in ('IDirectSoundBuffer_Stop(', 'IDirectSoundBuffer_Play(',
                    'IDirectSoundBuffer_Lock(', 'IDirectSoundBuffer_SetCurrentPosition(',
                    'IDirectSoundBuffer_SetVolume('):
        assert mutator not in HOOK
    assert 'PKR-AUDIO: PLAY-TRACE n=' in HOOK
    assert 'PLAY-TRACE ARMED read-only' in HOOK
