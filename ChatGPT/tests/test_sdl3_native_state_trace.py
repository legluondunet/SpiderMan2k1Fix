"""Native-state trace: explicit opt-in and read-only x86 signature guards.

This validates instrumentation boundaries, not a physical DS3 or Deck.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_trace_wired_only_to_existing_game_flip_and_opt_in():
    p=(ROOT/"sdl3_gameplay_plan_probe.c").read_text(encoding="utf-8")
    h=(ROOT/"sdl3_native_latch_trace.h").read_text(encoding="utf-8")
    ini=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    assert '"SDL3NativeStateTrace"' in ini
    assert '#include "sdl3_native_latch_trace.h"' in p
    assert 'spider_latch_trace_init(ini,logger);' in p
    assert 'spider_latch_trace_flip(hwnd,menu2d,gameplay3d,bink,pause);' in p
    assert p.index('SpiderSDL3GameplayLive_OnFlip(hwnd,') < p.index(
        'spider_latch_trace_flip(hwnd,')
    assert 'SpiderNativeReconnect_' not in p
    assert 'spider_latch_trace_flip(hwnd,menu2d,gameplay3d,bink,pause);' in p
    assert 'GetPrivateProfileIntA("Debug","SDL3NativeStateTrace",0,ini)==1' in h
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1' in h
    assert 'spider_latch_trace_binary_ok()' in h
    assert 'GetForegroundWindow()!=hwnd' in h
    assert 'bink || paused' in h
    assert 'spider_native_latch_observer_changed(' in h
    assert 'spider_latch_trace_observer.emitted>=64u' in h
    assert 'spider_latch_trace_observer.have_previous=0u;' in h
    assert 'spider_latch_trace_observer.emitted=0u;' not in h
    assert '[READ ONLY, SOURCE UNKNOWN]' in h

def test_trace_never_issues_original_input_polls_or_native_writes():
    h=(ROOT/"sdl3_native_latch_trace.h").read_text(encoding="utf-8")
    assert 'const volatile uint8_t *record=' in h
    assert 'record[0]' in h and 'record[1]' in h and 'record[2]' in h
    assert 'spider_original_action_latch_address(i)' in h
    for excluded in ('WriteProcessMemory','VirtualProtect','SendInput',
                     'IDirectInputDevice8_', '0x00505720u',
                     'CreateThread(', '0x0050A190u'):
        assert excluded not in h
    assert 'spider_latch_trace_preflight_ok' in h
    assert 'memcmp((const void *)(uintptr_t)0x0050A19Fu' in h
    assert 'memcmp((const void *)(uintptr_t)0x0050A29Eu' in h
    assert 'memcmp((const void *)(uintptr_t)0x00479540u' in h
    assert 'spider_latch_trace_log(message);' in h

def test_trace_is_disabled_in_generated_default_config():
    for path in ROOT.glob('*.ini'):
        content=path.read_text(encoding="utf-8",errors="replace")
        assert 'SDL3NativeStateTrace=1' not in content
