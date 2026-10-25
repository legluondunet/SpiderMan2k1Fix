"""Keep the GitHub homepage useful for players rather than a developer log."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
README = (ROOT / "README.md").read_text(encoding="utf-8")


def test_homepage_has_one_reader_focused_installation_path():
    headings = (
        "## Screenshots",
        "## Main Features",
        "## Bugs Resolved",
        "## Installation on Windows and Linux (Wine)",
        "## Configuration",
        "## Troubleshooting",
        "## Building from Source",
        "## Documentation",
        "## Credits",
        "## License and Disclaimer",
    )
    assert list(map(README.index, headings)) == sorted(map(README.index, headings))
    assert len(README) < 20_000
    assert README.count("## Installation on Windows and Linux (Wine)") == 1
    assert "No stable release is available" not in README
    assert README.count("No original game files are included in this project.") == 1
    assert "### F10" not in README
    assert "## Adaptive widescreen HUD" not in README


def test_download_requirements_and_linux_override_remain_explicit():
    assert "[GitHub Releases](https://github.com/legluondunet/SpiderMan2k1Fix/releases)" in README
    assert "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea" in README
    assert "winmm.dll" in README and "SpiderMan2k1Fix.dll" in README
    assert 'WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe' in README
    assert "Other versions or modified copies of the executable are not supported." in README


def test_readme_links_to_documentation_instead_of_repeating_it():
    for path in (
        "docs/README.md",
        "docs/references/CONFIGURATION.md",
        "docs/references/F9_PNG_CAPTURE.md",
        "docs/DEVELOPMENT.md",
    ):
        assert path in README
    config = (ROOT / "docs/references/CONFIGURATION.md").read_text(encoding="utf-8")
    assert "## Upgrading from older builds" in config
    assert "SpiderManFix.ini" in config
    assert "SpiderManFix.dll" in config
    assert "keep your existing INI" in config


def test_readme_preserves_required_disclaimers_and_credit():
    assert "GPL-3.0-only" in README
    assert "No original game files are included in this project." in README
    assert "own legally obtained copy" in README
    assert "not affiliated with, sponsored by, or endorsed by Activision" in README
    assert "[Krystalgamer](https://krystalgamer.github.io/index.html)" in README
