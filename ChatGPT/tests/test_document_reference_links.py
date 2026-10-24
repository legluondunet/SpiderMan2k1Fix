"""The explanatory Markdown references are grouped under docs/references.

Check source-document navigation, report inventory and historical path cleanup.
The Windows debugger's executable WinDbg prototype remains under ChatGPT.
"""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REFERENCES = (
    "F10_VALIDATION_MATRIX.md",
    "F12_DEBUG_TEST_MODE.md",
    "F12_NATIVE_SHUTDOWN.md",
    "NATIVE_PAUSE_MENU_43.md",
    "diagnostics/directinput/DIRECTINPUT_EXTERNAL_DIAG.md",
    "diagnostics/directinput/DIRECTINPUT_INI_FINAL_TEST.md",
    "diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md",
    "diagnostics/focus/CURSOR_FOCUS_TEST.md",
    "diagnostics/focus/FOCUS_AUDIO_VIDEO_SYNC_TEST.md",
    "diagnostics/focus/PAUSE_ON_FOCUS_LOSS_TEST.md",
    "diagnostics/focus/UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md",
    "diagnostics/graphics/WINDOWS_PRIMARY_REUSE_TEST.md",
    "diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md",
    "tests/MENU_VERSION_TEST.md",
    "tests/TASKBAR_ICON_TEST.md"
)


def test_reference_inventory_is_consolidated():
    docs = ROOT / "docs"
    reference_dir = docs / "references"
    assert reference_dir.is_dir()

    # Both indexes remain at the top of docs; all 15 detailed reports do not.
    assert sorted(p.name for p in docs.glob("*.md")) == [
        "DEVELOPMENT.md", "README.md"
    ]

    for relative in REFERENCES:
        ref = reference_dir / relative
        assert ref.is_file(), relative
        assert not (docs / relative).exists(), relative
        # Old developer-report directories are no longer their canonical home.
        if relative.startswith("diagnostics/"):
            old = ROOT / "ChatGPT" / relative
        elif relative.startswith("tests/"):
            old = ROOT / "ChatGPT/tests/reports" / Path(relative).name
        else:
            old = docs / Path(relative).name
        assert not old.exists(), str(old)

    assert (ROOT / "ChatGPT/diagnostics/WINDBG_DI8_TRACE.md").is_file()


def test_document_indexes_resolve_all_local_links():
    files = [
        "README.md",
        "docs/README.md",
        "docs/DEVELOPMENT.md",
        "ChatGPT/README.md",
        "docs/references/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md",
    ]
    for rel in files:
        page = ROOT / rel
        for raw in re.findall(r"\]\(([^)]+)\)", page.read_text(encoding="utf-8")):
            if raw.startswith(("http:", "https:", "mailto:", "#")):
                continue
            target = raw.split("#", 1)[0].split("?", 1)[0]
            if not target:
                continue
            resolved = (page.parent / target).resolve()
            assert resolved.exists(), f"{rel}: broken local Markdown link {raw}"


def test_docs_index_lists_every_reference():
    index = (ROOT / "docs/README.md").read_text(encoding="utf-8")
    for rel in REFERENCES:
        assert f"references/{rel}" in index, rel
