"""Regression guards for the separate Linux/Wine smoke and native-game plan."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = (ROOT / ".github/workflows/spiderman2k1fix-wine-smoke.yml").read_text()
HOST = (ROOT / "tests/wine_proxy_smoke.c").read_text()
COLLECTOR = (ROOT / "tests/wine-game-regression.sh").read_text()
DOCUMENT = (ROOT / "WINE_REGRESSION_TEST.md").read_text()


def test_wine_ci_builds_real_dll_pair_and_uses_actual_32_bit_wine():
    assert "runs-on: ubuntu-24.04" in WORKFLOW
    assert "WINEARCH: win32" in WORKFLOW
    assert "wine32:i386" in WORKFLOW
    assert "bash ./build.sh" in WORKFLOW
    assert "winmm=n,b" in WORKFLOW
    assert "xvfb-run -a wine" in WORKFLOW
    assert "spiderman2k1fix-windows-x86" not in WORKFLOW


def test_wine_smoke_checks_proxy_exports_and_loads_fix_safely():
    assert '#include <mmsystem.h>' in HOST
    assert "timeGetTime()" in HOST
    assert 'GetModuleHandleA("winmm.dll")' in HOST
    assert 'GetModuleHandleA("SpiderMan2k1Fix.dll")' in HOST
    assert "local_winmm_is_loaded()" in HOST
    assert "Sleep(1500)" in HOST
    assert "ERROR: unsupported EXE SHA-256" in WORKFLOW


def test_manual_collector_never_mutates_game_files_or_prefix():
    for name in ("SpideyPC.exe", "winmm.dll", "SpiderMan2k1Fix.dll",
                 "SpiderMan2k1Fix.ini"):
        assert name in COLLECTOR
    assert "sha256sum" in COLLECTOR
    assert "WINEDLLOVERRIDES" in COLLECTOR
    assert "WINEPREFIX" in COLLECTOR
    assert "Log=1" in COLLECTOR
    assert 'tail -c "+$((start_bytes+1))"' in COLLECTOR
    assert "tar -czf" in COLLECTOR
    assert "cp --" in COLLECTOR
    assert "sed -i" not in COLLECTOR
    assert "rm -rf" not in COLLECTOR
    assert "WINEDLLOVERRIDES=\"$overrides\"" in COLLECTOR


def test_documentation_preserves_manual_game_validation_requirements():
    assert "The real" in DOCUMENT or "real" in DOCUMENT
    assert "Cinematics" in DOCUMENT or "cinematics" in DOCUMENT
    assert "PauseOnFocusLoss=0" in DOCUMENT
    assert "PauseOnFocusLoss=1" in DOCUMENT
    assert "RestoreCursorOnFocusLoss=1" in DOCUMENT
    assert "Linux" in DOCUMENT
    assert "Wine" in DOCUMENT
