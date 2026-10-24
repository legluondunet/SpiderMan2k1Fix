"""Development-default regression checks (source-level, not a Windows runtime test)."""
from pathlib import Path

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
    assert '#define SPIDERMAN2K1FIX_VERSION "0.0.0"' in (ROOT / 'version_info.h').read_text()


def test_input_focus_recovery_not_removed():
    core = (ROOT / 'spiderman2k1fix.c').read_text()
    focus = (ROOT / 'input_focus_trace.c').read_text()
    assert 'SpiderInputFocus_Install(log_line);' in core
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in focus
    assert 'if(!input_debug_logging' in focus


def test_native_windows_reacquire_hook_not_on_by_default():
    ini = (ROOT / 'ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini').read_text()
    generated = (ROOT / 'virtual_spidey_cfg.c').read_text()
    assert 'DirectInputReacquire=0' in ini
    assert '"DirectInputReacquire=0\\r\\n"' in generated
    assert 'ExperimentalAltTabReacquire=' not in ini
    assert '"ExperimentalAltTabReacquire=' not in generated
    assert 'ExperimentalDI8CreateDeviceTrace=' not in ini
    assert '"ExperimentalDI8CreateDeviceTrace=' not in generated
    assert 'PauseOnFocusLoss=0' in ini
    assert '"PauseOnFocusLoss=0\\r\\n"' in generated
    assert 'RestoreCursorOnFocusLoss=1' in ini
    assert '"RestoreCursorOnFocusLoss=1\\r\\n"' in generated
