"""The public changelog must not turn the research diary into release claims."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CHANGELOG=(ROOT/"CHANGELOG.md").read_text(encoding="utf-8")
ARCHIVE=(ROOT/"docs/references/CHANGELOG_EXPERIMENT_HISTORY_PRE_20261101.md").read_text(encoding="utf-8")
README=(ROOT/"README.md").read_text(encoding="utf-8")


def test_unreleased_integration_is_explicitly_draft():
    assert CHANGELOG.startswith("# Changelog\n\n## Unreleased")
    head=CHANGELOG.split("## 1.1.0 — October 27, 2026",1)[0]
    assert "draft PR #91" in head
    assert "**Not released or merged.**" in head
    assert "experimental INI opt-ins" in head
    assert "ExperimentalAltLatchRecovery=1" in head
    assert "default-OFF" in head
    assert "20/20 physical" in head
    assert "37/37" in head
    assert "DS3, Steam Deck and arbitrary controllers are not validated" in head
    assert "PAUSED ribbon may be" in head
    assert "MouseEnabled=1" in head
    assert "release is authorized by these results" in head


def test_published_changelog_entries_survive_below_unreleased():
    assert CHANGELOG.count("## 1.1.0 — October 27, 2026")==1
    assert CHANGELOG.count("## 1.0.0 — October 25, 2026")==1
    assert CHANGELOG.index("## 1.1.0")<CHANGELOG.index("## 1.0.0")
    assert "Fixed crackling and distorted sound effects" in CHANGELOG
    assert "First stable release of SpiderMan2k1Fix" in CHANGELOG
    assert "SpiderMan2k1Fix v1.0.0 — GitHub Releases" in CHANGELOG
    assert "docs/references/diagnostics/audio/PKR_NATIVE_AUDIO_REPAIR.md" in CHANGELOG


def test_old_research_diary_preserved_as_explicitly_superseded_archive():
    assert len(ARCHIVE)>52000
    assert "preserved verbatim" in ARCHIVE
    assert "not the authoritative" in ARCHIVE
    assert "## Unreleased — experimental SDL3-selected DirectInput owner (DRAFT PR #80, NOT hardware-validated)" in ARCHIVE
    assert "MouseEnabled=1" in ARCHIVE  # historical obsolete policy allowed here, NOT promoted
    assert "## 1.1.0 — October 27, 2026" in ARCHIVE
    assert "## 1.0.0 — October 25, 2026" in ARCHIVE
    assert "CHANGELOG_EXPERIMENT_HISTORY_PRE_20261101.md" in CHANGELOG
    assert "## Frequently Asked Questions" in README
