"""Verify historical findings survive branch pruning without overclaiming scope."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs/references"


def test_native_function_catalog_marks_call_sites_and_uncertainty():
    doc = (DOCS / "ORIGINAL_EXE_FUNCTION_CATALOG.md").read_text(encoding="utf-8")
    for entry in (
        "0x00470750", "0x00443AD0", "0x004A1930",
        "0x004A17A0", "0x00493990", "0x004941D5",
        "0x00427910", "0x004279FB", "0x004BC1A0",
        "0x004BC3E0", "0x00440EF0", "0x0047BC40",
        "0x00455BF3", "0x00456185", "0x0045620C",
        "0x005163E0", "0x00501BC3", "0x00501D02",
    ):
        assert entry in doc, entry
    assert "not** a complete decompilation" in doc
    assert "not a continuously updated scene ID" in doc
    assert "not** a unique player-HUD routine" in doc


def test_historical_f12_results_not_presented_as_active_feature():
    history = (DOCS / "F12_EXIT_ARCHIVE_SUMMARY.md").read_text(encoding="utf-8")
    assert "Not an active shortcut" in history
    assert "build #605" in history
    assert "build #589" in history
    assert "build #520" in history
    assert "yellow window" in history
    assert "RETURN-TO-CONTROLLER" not in history or "return to controller" in history.lower()
    assert (DOCS / "F12_NATIVE_SHUTDOWN.md").is_file()
    assert (DOCS / "F12_DEBUG_TEST_MODE.md").is_file()


def test_old_wine_branch_method_preserved_without_claiming_ci():
    protocol = (DOCS / "WINE_REGRESSION_CHECKLIST.md").read_text(encoding="utf-8")
    assert "test/wine-compatibility-regression" in protocol
    assert "smoke" in protocol.lower()
    assert "v1.0.0" in protocol  # Only called out as withdrawn/obsolete.
    assert "not proof" in protocol.lower() or "not establish" in protocol.lower()
    assert "tests/wine-game-regression.sh" in protocol


def test_branch_cleanup_guidance_protects_open_prs_and_unmerged_code():
    audit = (DOCS / "BRANCH_RESEARCH_PRESERVATION.md").read_text(encoding="utf-8")
    assert "No branches were deleted" in audit
    assert "docs/remove-redundant-esc-section" in audit
    assert "test/wine-compatibility-regression" in audit
    assert "experiment/esc-menu-draw-diagnostics-20261021" in audit
    assert "experiment/f9-readback-windows-20261022" in audit
    assert "their executable patch implementations" in audit
