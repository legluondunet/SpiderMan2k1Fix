"""Documentation relocation regression: historical reports are developer-only.

This verifies the stable project documentation links to the relocated report
files and catches accidental resurrection of the deprecated docs subtrees.
"""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

REPORTS = (
    "diagnostics/directinput/DIRECTINPUT_EXTERNAL_DIAG.md",
    "diagnostics/directinput/DIRECTINPUT_INI_FINAL_TEST.md",
    "diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md",
    "diagnostics/focus/CURSOR_FOCUS_TEST.md",
    "diagnostics/focus/FOCUS_AUDIO_VIDEO_SYNC_TEST.md",
    "diagnostics/focus/PAUSE_ON_FOCUS_LOSS_TEST.md",
    "diagnostics/focus/UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md",
    "diagnostics/graphics/WINDOWS_PRIMARY_REUSE_TEST.md",
    "diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md",
    "tests/reports/MENU_VERSION_TEST.md",
    "tests/reports/TASKBAR_ICON_TEST.md",
)


def test_all_historical_reports_are_archived_not_in_main_docs():
    for relative in REPORTS:
        assert (ROOT / "ChatGPT" / relative).is_file(), relative
        assert not (ROOT / "docs" / relative).exists(), relative
    assert not (ROOT / "docs/diagnostics").exists()
    assert not (ROOT / "docs/tests").exists()


def test_maintained_docs_link_to_existing_report_files():
    for rel in ("README.md", "docs/README.md", "docs/DEVELOPMENT.md", "ChatGPT/README.md"):
        markdown = ROOT / rel
        content = markdown.read_text(encoding="utf-8")
        for raw in re.findall(r"\]\(([^)]+)\)", content):
            if raw.startswith(("http:", "https:", "mailto:", "#")):
                continue
            target = raw.split("#", 1)[0].split("?", 1)[0]
            if not target:
                continue
            resolved = (markdown.parent / target).resolve()
            assert resolved.exists(), f"{rel}: broken local Markdown link {raw}"
