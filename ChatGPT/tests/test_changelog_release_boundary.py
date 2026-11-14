"""The public changelog must describe live policy, not superseded test plans."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CHANGELOG=(ROOT/"CHANGELOG.md").read_text(encoding="utf-8")
ARCHIVE=(ROOT/"docs/references/CHANGELOG_EXPERIMENT_HISTORY_PRE_20261101.md").read_text(encoding="utf-8")
README=(ROOT/"README.md").read_text(encoding="utf-8")
RC_PLAN=(ROOT/"docs/references/RELEASE_2_0_RC_TEST_PLAN.md").read_text(encoding="utf-8")


def test_stable_2_0_explains_current_sdl3_policy_and_preserves_release_gate():
    # This version is prepared but no official v2.0.0 release/tag exists yet.\n    assert CHANGELOG.startswith("# Changelog\n\n## 2.0.0 — Unreleased")
    head=CHANGELOG.split("## 1.1.0 — October 27, 2026",1)[0]
    for phrase in ("only supported controller mapping backend", "first safe foreground",
                   "neutral-first", "DirectInput", "QuickExit F10/F12", "RawHex",
                   "Windows 11", "Wine/Proton", "Steam Deck", "final v2.0.0",
                   "original veil", "SDL3 3.4.18", "CI passed on merged main"):
        assert phrase in head, phrase
    assert "ExperimentalSDL3UniversalGameplay=0" not in head
    assert "Native Windows hardware validation remains deferred" not in head
    assert "Not released or merged" not in head
    assert "README" in RC_PLAN and "Windows" in RC_PLAN and "Wine" in RC_PLAN
    assert "historical RC2 acceptance record" in RC_PLAN
    assert "0d5ae792" in RC_PLAN


def test_historical_release_entries_remain_intact():
    assert CHANGELOG.count("## 1.1.0 — October 27, 2026")==1
    assert CHANGELOG.count("## 1.0.0 — October 25, 2026")==1
    assert CHANGELOG.index("## 1.1.0")<CHANGELOG.index("## 1.0.0")
    assert "Fixed crackling and distorted sound effects" in CHANGELOG
    assert "First stable release of SpiderMan2k1Fix" in CHANGELOG
    assert "SpiderMan2k1Fix v1.0.0 — GitHub Releases" in CHANGELOG
    assert "docs/references/diagnostics/audio/PKR_NATIVE_AUDIO_REPAIR.md" in CHANGELOG


def test_research_archive_is_retained_not_release_policy():
    assert len(ARCHIVE)>52000
    assert "preserved verbatim" in ARCHIVE
    assert "not the authoritative" in ARCHIVE
    assert "## Unreleased — experimental SDL3-selected DirectInput owner" in ARCHIVE
    assert "MouseEnabled=1" in ARCHIVE
    assert "## 1.1.0 — October 27, 2026" in ARCHIVE
    assert "## 1.0.0 — October 25, 2026" in ARCHIVE
    assert "CHANGELOG_EXPERIMENT_HISTORY_PRE_20261101.md" in CHANGELOG
    assert "## Frequently Asked Questions" in README
