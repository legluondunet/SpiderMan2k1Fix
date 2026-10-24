"""Current F10 guidance must match supported INI settings and scope."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def test_f10_readme_describes_hidden_debug_opt_in_and_native_cleanup():
    readme=(ROOT/"README.md").read_text(encoding="utf-8")
    assert "[Debug]\nF10Shortcut=1" in readme
    assert "not generated" in readme
    assert "defaults to `0`" in readme
    assert "independent of `Log=1`" in readme
    assert "build #713" in readme
    assert "Bink-to-first-3D Enter/Space release safeguard" in readme
    assert "docs/F10_VALIDATION_MATRIX.md" in readme
    assert "later in-level" in readme
    assert "F10TestMode=1\\nF10Auto3DScripts=1" not in readme

def test_validation_matrix_distinguishes_wine_verified_and_pending_levels():
    matrix=(ROOT/"docs/F10_VALIDATION_MATRIX.md").read_text(encoding="utf-8")
    for evidence in ("CutSceneScript", "native_context=1", "F10VerboseTrace=1",
                     "NATIVE-FINAL-CLEANUP", "Windows 11 native",
                     "Alt+Tab / focus loss", "F10Shortcut=1", "F10Shortcut=0"):
        assert evidence in matrix
    assert "Still to be tested on real gameplay" in matrix
    assert "Validated in Wine #705 and #713" in matrix
    assert "Final Wine validation — build #713" in matrix
