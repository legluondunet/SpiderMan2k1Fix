"""Licensing and game-ownership notices are explicit and consistent."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_full_gpl3_license_is_committed():
    license_text = (ROOT / "LICENSE").read_text(encoding="utf-8")
    assert "GNU GENERAL PUBLIC LICENSE" in license_text
    assert "Version 3, 29 June 2007" in license_text
    assert "TERMS AND CONDITIONS" in license_text
    assert "END OF TERMS AND CONDITIONS" in license_text
    assert len(license_text) > 30000


def test_project_homepage_has_license_and_game_disclaimer():
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert "[GNU General Public License v3.0](LICENSE)" in readme
    assert "GPL-3.0-only" in readme
    assert "No original game files are included in this project." in readme
    assert "own legally obtained copy" in readme
    assert "not affiliated with, sponsored by, or endorsed by Activision" in readme


def test_both_documentation_entry_points_have_notices():
    for name in ("docs/README.md", "docs/DEVELOPMENT.md"):
        doc = (ROOT / name).read_text(encoding="utf-8")
        assert "../LICENSE" in doc, name
        assert "GPL-3.0-only" in doc, name
        assert "Activision" in doc, name
        assert "original" in doc.lower(), name
