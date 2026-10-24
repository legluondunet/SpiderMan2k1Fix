"""Verify all runtime and build filenames agree after the project rename."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def test_new_dll_names_are_consistent():
    proxy = (ROOT / "winmm_proxy.c").read_text()
    build = (ROOT / "build.sh").read_text()
    ci = (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()
    assert '"SpiderMan2k1Fix.dll"' in proxy
    assert 'SpiderMan2k1Fix.dll' in build
    assert 'SpiderMan2k1Fix.dll' in ci
    assert 'winmm.dll' in build and 'winmm.dll' in ci
    assert 'spiderman2k1fix.c' in build
    assert (ROOT / 'spiderman2k1fix.c').exists()

def test_ini_and_logs_are_consistent():
    for name in ('spiderman2k1fix.c', 'winmm_proxy.c', 'input_focus_trace.c', 'virtual_spidey_cfg.c'):
        data=(ROOT / name).read_text()
        assert 'SpiderMan2k1Fix.ini' in data
    for name in ('spiderman2k1fix.c','winmm_proxy.c'):
        assert 'SpiderMan2k1Fix.log' in (ROOT / name).read_text()
    assert (ROOT / 'SpiderMan2k1Fix.ini').exists()
    assert (ROOT / 'SpiderMan2k1Fix.generated-example.ini').exists()

def test_no_obsolete_name_in_runtime_sources():
    for path in ROOT.iterdir():
        if path.is_file() and path.suffix in ('.c','.h','.sh','.ini'):
            assert 'SpiderManFix' not in path.read_text(), path.name
            assert 'spidermanfix' not in path.read_text(), path.name
