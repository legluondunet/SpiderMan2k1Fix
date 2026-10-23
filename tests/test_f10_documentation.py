"""Current F10 guidance must match supported INI settings and scope."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def test_f10_readme_lists_compact_and_native_exit_as_separate_opt_ins():
    readme=(ROOT/"README.md").read_text(encoding="utf-8")
    assert "[Debug]\nLog=1\nF10TestMode=1\nF10Auto3DScripts=1\nF10GameplayExit=1\nF10VerboseTrace=0" in readme
    assert "PR #26 remains a draft" in readme
    assert "docs/F10_VALIDATION_MATRIX.md" in readme
    assert "not yet in arbitrary later in-level cutscenes" in readme
    assert "F10TestMode=1\\nF10Auto3DScripts=1" not in readme

def test_validation_matrix_distinguishes_wine_verified_and_pending_levels():
    matrix=(ROOT/"docs/F10_VALIDATION_MATRIX.md").read_text(encoding="utf-8")
    for evidence in ("CutSceneScript", "native_context=1", "F10VerboseTrace=1",
                     "NATIVE-FINAL-CLEANUP", "Windows 11 native",
                     "Alt+Tab / focus loss", "F10GameplayExit=0"):
        assert evidence in matrix
    assert "Still to be tested on real gameplay" in matrix
    assert "validated for this F10 consolidated build" in matrix
