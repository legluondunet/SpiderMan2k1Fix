"""Current F10 guidance must match supported INI settings and scope."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def test_f10_details_live_in_technical_docs_instead_of_homepage():
    readme=(ROOT/"README.md").read_text(encoding="utf-8")
    matrix=(ROOT/"docs/references/F10_VALIDATION_MATRIX.md").read_text(encoding="utf-8")
    index=(ROOT/"docs/README.md").read_text(encoding="utf-8")
    assert "### F10 development shortcut" not in readme
    assert "F10Shortcut=1" not in readme
    assert "references/F10_VALIDATION_MATRIX.md" in index
    for text in ("[Debug]", "Log=1", "F10Shortcut=1", "not generated",
                 "defaults to `0`", "independent of `Log=1`",
                 "build #713", "Bink-to-first-3D Enter/Space release safeguard",
                 "later in-level"):
        assert text in matrix
    assert "F10TestMode=1\\nF10Auto3DScripts=1" not in readme


def test_validation_matrix_distinguishes_wine_verified_and_pending_levels():
    matrix=(ROOT/"docs/references/F10_VALIDATION_MATRIX.md").read_text(encoding="utf-8")
    for evidence in ("CutSceneScript", "native_context=1", "F10VerboseTrace=1",
                     "NATIVE-FINAL-CLEANUP", "Windows 11 native",
                     "Alt+Tab / focus loss", "F10Shortcut=1", "F10Shortcut=0"):
        assert evidence in matrix
    assert "Still to be tested on real gameplay" in matrix
    assert "Validated in Wine #705 and #713" in matrix
    assert "Final Wine validation — build #713" in matrix
