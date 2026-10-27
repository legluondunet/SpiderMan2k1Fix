"""Exact-SHA256 Swing5-only redirection must fail open to original game."""
from pathlib import Path

R=Path(__file__).resolve().parents[2]
REDIRECT=(R/"sdl3_swing5_redirect.c").read_text()
ENGINE=(R/"sdl3_audio_probe.c").read_text()
INTERFACE=(R/"sdl3_audio_probe.h").read_text()
MAIN=(R/"spiderman2k1fix.c").read_text()
BUILD=(R/"build.sh").read_text()
INI=(R/"ini_validation.h").read_text()

def test_fail_open_and_bounded_hook():
    assert 'ExperimentalSDL3Swing5Replace' in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalSDL3Swing5Replace",0,ini)==1' in MAIN
    assert 'SpiderSDL3Probe_Init(log_line,test,swing)' in MAIN
    assert 'SpiderSDL3Swing5_Init(log_line,swing||pkr_readonly||pkr_tail_restore)' in MAIN
    assert 'SpiderSDL3Probe_PlaySwing5' in INTERFACE
    assert 'sdl3_swing5_redirect.c' in BUILD
    assert '#define ONE_SHOT_CALL ((BYTE*)(uintptr_t)0x00472870u)' in REDIRECT
    assert '0xE8,0xBB,0x19,0x09,0x00' in REDIRECT
    assert 'memcmp(ONE_SHOT_CALL,expected_call,sizeof(expected_call))!=0' in REDIRECT
    assert 'GetModuleHandleA(NULL)!=GAME_BASE' in REDIRECT
    assert 'FlushInstructionCache' in REDIRECT
    assert 'VirtualProtect(page,4096u,old,&ignored)' in REDIRECT
    assert 'GAME_PLAY(slot,looping); /* Fail OPEN' in REDIRECT
    assert 'if(!installed || slot<0 || slot>=32)' in REDIRECT
    assert 'if(!looping && buf && exact_corrupt_buffer(buf))' in REDIRECT
    assert 'caps.dwBufferBytes!=SWING_BYTES' in REDIRECT
    assert 'CryptGetHashParam' in REDIRECT
    assert 'memcmp(digest,corrupt_swing_sha,sizeof(digest))==0' in REDIRECT
    assert 'unlock_ok && ok' in REDIRECT
    assert 'entry[8]=0;' in REDIRECT and 'entry[9]=0;' in REDIRECT
    assert REDIRECT.index('SpiderSDL3Probe_PlaySwing5(slot,buf,volume,pan,hz)') < REDIRECT.index('entry[8]=0;')

def test_clean_source_verified_before_native_play_suppressed():
    assert 'clean_pcm_digest(decoded,bytes)' in ENGINE
    assert 'memcmp(digest,clean_swing_sha,sizeof(digest))==0' in ENGINE
    assert 'spec.freq==16000 && spec.channels==1 && spec.format==0x8010u' in ENGINE
    assert 'bytes==sizeof(swing_source_pcm)' in ENGINE
    assert 'swing_source_attempted' in ENGINE
    assert 'if(!load_verified_swing_source())return 0;' in ENGINE
    assert 'if(!put(new_stream,stereo,(int)(frame_count*4u)) || !resume(new_stream))' in ENGINE
    assert 'SDL3-SWING5: REPLACED' in ENGINE
    assert 'swing_voices[32]' in ENGINE
    assert 'ds_volume' in ENGINE and 'ds_pan' in ENGINE
    assert 'spec.freq=(int)actual_hz;' in ENGINE
    assert 'GetProcAddress' in ENGINE
    assert 'LoadLibraryA(path)' in ENGINE
    assert 'SDL3-SWING5: FALLBACK' in REDIRECT
    assert 'if(!on)return;' in ENGINE

def test_only_swing5_replacement_and_no_generic_gameplay_mutation():
    assert 'ExperimentalSDL3Swing5Replace' in INI
    assert 'ExperimentalSDL3AudioProbe' in INI
    assert 'IDirectSoundBuffer_Play' not in ENGINE
    assert 'IDirectSoundBuffer_SetVolume' not in REDIRECT
    assert 'IDirectSoundBuffer_Stop' not in REDIRECT
    assert 'PKR_ReadFile' not in ENGINE
    assert 'PKR_ReadFile' not in REDIRECT
    assert 'WriteFile(' not in ENGINE
    assert 'CreateFileA(' not in ENGINE
    assert 'VirtualProtect(' not in ENGINE
    assert 'voice still' not in ENGINE

def test_native_voice_lifecycle_and_fail_open():
    assert 'static const BYTE expected_stop[NATIVE_STOP_PATCH_BYTES]' in REDIRECT
    assert '0x8B,0x44,0x24,0x04,0x56,0x8D,0x04,0x40' in REDIRECT
    assert 'memcmp(GAME_STOP_ADDRESS,expected_stop,sizeof(expected_stop))!=0' in REDIRECT
    assert 'SpiderSDL3Probe_StopSlot(slot,"native-DXSOUND_Stop")' in REDIRECT
    assert 'SpiderSDL3Probe_StopSlot(slot,"native-voice-replay")' in REDIRECT
    assert 'original_stop(slot)' in REDIRECT
    assert 'stop_trampoline[NATIVE_STOP_PATCH_BYTES]=0xE9' in REDIRECT
    assert 'memcpy(GAME_STOP_ADDRESS,patch,sizeof(patch))' in REDIRECT
    assert 'FlushInstructionCache' in REDIRECT
    assert 'SpiderSDL3Probe_PauseAll(1)' in MAIN
    assert 'SpiderSDL3Probe_PauseAll(0)' in MAIN
    assert 'SpiderSDL3Probe_OnVoiceFlip(InterlockedCompareExchange(&menu_upscale_active,0,0)==1)' in MAIN
    assert 'IDirectSoundBuffer_AddRef(original)' in ENGINE
    assert 'IDirectSoundBuffer_Release(v->original)' in ENGINE
    assert 'SDL3-VOICE: STOP' in ENGINE
    assert 'SDL3-VOICE: PAUSE' not in ENGINE  # logged by formatted PAUSE/RESUME line
    assert 'SDL_PauseAudioStreamDevice' in ENGINE
    assert 'close_voice(i,"return-to-menu")' in ENGINE
    assert 'close_voice(i,"voice-reused-or-released")' in ENGINE
    assert 'close_voice(i,"one-shot-finished")' in ENGINE
    assert 'refresh_parameters(v,vol,pan,hz)' in ENGINE
    assert 'position_milli_frames' in ENGINE
