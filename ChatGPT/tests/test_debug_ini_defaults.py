"""Source-level checks for the debug switch and permanent virtual hooks."""
from pathlib import Path
import configparser

ROOT = Path(__file__).resolve().parents[1]
example = (ROOT / 'SpiderMan2k1Fix.generated-example.ini').read_text()
virtual = (ROOT / 'virtual_spidey_cfg.c').read_text()
core = (ROOT / 'spiderman2k1fix.c').read_text()
proxy = (ROOT / 'winmm_proxy.c').read_text()
source_ini = (ROOT / 'SpiderMan2k1Fix.ini').read_text()


def test_debug_example_and_exact_display_help():
    parser = configparser.ConfigParser(interpolation=None)
    parser.read_string(example)
    assert parser['Debug']['Log'] == '0'
    assert '; Auto = desktop monitor resolution\nResolution=Auto\n' in example
    assert '; Auto or 4:3 for a native gaming experience\nAspectRatio=Auto\n' in example
    assert '[Debug]\\r\\n' in virtual
    assert 'Log=0\\r\\n' in virtual
    assert 'VirtualSpideyCfg=' not in example
    assert 'BootstrapGameConfig=' not in example
    assert 'VirtualSpideyCfg=' not in source_ini
    assert 'BootstrapGameConfig=' not in source_ini


def test_logging_is_gated_in_both_dlls():
    assert 'GetPrivateProfileIntA("Debug","Log",0,path)==1' in core
    assert 'if (!debug_log_enabled) return;' in core
    assert 'GetPrivateProfileIntA("Debug","Log",0,path)!=1' in proxy
    assert 'SpiderMan2k1Fix-proxy.log' not in proxy
    assert 'SpiderMan2k1Fix.log' in proxy
    assert 'debug_log_load_setting();' in core


def test_virtual_cfg_and_bootstrap_always_attempted_but_guarded():
    assert 'SpiderCfg_Install(ini_path,log_line);' in core
    assert 'GetPrivateProfileIntA("GameConfig","VirtualSpideyCfg"' not in core
    assert 'GetPrivateProfileIntA("GameConfig","BootstrapGameConfig"' not in virtual
    assert 'bootstrap_enabled' not in virtual
    assert 'if(!ready || !image ||' in virtual
    assert 'memcmp(image+' in virtual
