"""Development-default regression checks (source-level, not a Windows runtime test)."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def test_default_logging_is_off_by_default():
    ini = (ROOT / 'ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini').read_text()
    generated = (ROOT / 'virtual_spidey_cfg.c').read_text()
    assert '\n[Debug]\n' in ini
    assert '\nLog=0\n' in ini
    assert '"Log=0\\r\\n"' in generated
    assert '"Log=1\\r\\n"' not in generated


def test_obsolete_probes_removed():
    core = (ROOT / 'spiderman2k1fix.c').read_text()
    for symbol in ('fps_trace_flip_rate', 'fps_startup_watchdog',
                   'verify_render_callsites', 'maybe_correct_comic'):
        assert symbol not in core
    assert 'comic_aspect.c' not in (ROOT / 'build.sh').read_text()
    assert 'log_line("START: " SPIDERMAN2K1FIX_VERSION_LABEL);' in core
    proxy=(ROOT / 'winmm_proxy.c').read_text()
    assert '#include "version_info.h"' in proxy
    assert 'SPIDERMAN2K1FIX_VERSION_LABEL' in proxy
    assert re.search(r'#define SPIDERMAN2K1FIX_VERSION "2\\.0\\.0(?:-rc\\.\\d+)?"', (ROOT / 'version_info.h').read_text())


def test_input_focus_recovery_not_removed():
    core = (ROOT / 'spiderman2k1fix.c').read_text()
    focus = (ROOT / 'input_focus_trace.c').read_text()
    assert 'SpiderInputFocus_Install(log_line);' in core
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in focus
    assert 'if(!input_debug_logging' in focus


def test_native_reacquire_and_pause_are_builtin():
    generated = (ROOT / 'virtual_spidey_cfg.c').read_text()
    retry = (ROOT / 'input_notacquired_retry.c').read_text()
    core = (ROOT / 'spiderman2k1fix.c').read_text()
    for retired in ("DirectInputReacquire", "PauseOnFocusLoss",
                    "RestoreCursorOnFocusLoss", "PauseBinkOnFocusLoss"):
        assert f'"{retired}=' not in generated
    assert "InterlockedExchange(&requested,1);" in retry
    assert "InterlockedExchange(&pause_on_focus_loss,1);" in core
    assert "SpiderFocusMedia_Pause();" in core
    assert "SpiderFocusDSound_Pause();" in core

def test_unused_legacy_state_is_removed_but_native_patch_lifetime_is_kept():
    cfg = (ROOT / 'virtual_spidey_cfg.c').read_text(encoding='utf-8')
    retry = (ROOT / 'input_notacquired_retry.c').read_text(encoding='utf-8')
    menu_c = (ROOT / 'ps1_joystick_readonly_menu.c').read_text(encoding='utf-8')
    menu_h = (ROOT / 'ps1_joystick_readonly_menu.h').read_text(encoding='utf-8')
    escape = (ROOT / 'sdl3_physical_escape.h').read_text(encoding='utf-8')

    # These were write-only globals, confirmed by compiler warnings.
    assert 'stdio_opens' not in cfg
    assert 'readonly_xbox_rt_alias' not in menu_c
    assert 'SpiderXbox360_SetRTAlias' not in menu_c + menu_h
    assert 'retained_executable_page' not in retry

    # The executable trampoline is still retained on installed branches,
    # and freed only on safe installation failure / successful rollback.
    assert 'VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)' in retry
    assert 'if(restore_original_jne((BYTE *)(uintptr_t)SPIDER_KEY_JNE))' in retry
    assert 'VirtualFree(page,0,MEM_RELEASE);' in retry
    assert 'InterlockedExchange(&installed,1);' in retry

    # Logical grouping is explicit; preserve the original acceptance rule.
    assert '(got!=(UINT)sizeof(packet) &&' in escape
    assert 'got<(UINT)(sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD))))' in escape
