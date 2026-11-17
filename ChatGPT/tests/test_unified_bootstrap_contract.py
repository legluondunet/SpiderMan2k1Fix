"""Static invariants for the experimental unified DirectDraw bridge.
These tests inspect source only; they are not Windows/Wine executable tests.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_unified_bridge_exports_only_directdraw_factory():
    source = (ROOT / "unified_bootstrap.c").read_text()
    exports = (ROOT / "unified_directdraw.def").read_text()
    assert "SpiderUnified_DirectDrawCreateEx" in source
    assert "DirectDrawCreateEx = SpiderUnified_DirectDrawCreateEx@16" in exports
    assert "SpiderMan2k1Fix_DirectDrawGate(" in source
    assert "GetSystemDirectoryW(path,MAX_PATH)" in source
    assert "LoadLibraryW(path)" in source
    assert "InitOnceExecuteOnce(" in source
    assert "DDERR_UNSUPPORTED" in source
    assert "DDSCL_EXCLUSIVE" not in source
    assert "SetCooperativeLevel" not in source
    assert "MessageBox" not in source
    assert "DllMain(" not in source


def test_architecture_preserves_separate_dsound_diagnostic_and_state_receipts():
    design = (ROOT / "docs/references/UNIFIED_ARCHITECTURE_20261117.md").read_text()
    for path in ("state/", "installed-game.sha256", "installed-resources.sha256",
                 "third_party/", "SDL3/", "linux/", "docs/", "game/", "fix.dll"):
        assert path in design
    assert "draft PR #161" in design
    assert "No automatic migration" in design
    assert "not a playable candidate" in design
