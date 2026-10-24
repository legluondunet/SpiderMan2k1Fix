"""Keep the GitHub homepage useful for players rather than a developer log."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
README = (ROOT / "README.md").read_text(encoding="utf-8")


def test_homepage_has_one_reader_focused_installation_path():
    headings = (
        "## What does the patch do?",
        "## Download and install",
        "### Installing on Windows",
        "### Installing on Linux with Wine",
        "## Configuration",
        "## Troubleshooting",
        "## For developers",
        "## Credits and acknowledgements",
        "## License and third-party rights",
    )
    assert list(map(README.index, headings)) == sorted(map(README.index, headings))
    assert len(README) < 10_000
    assert README.count("## Download and install") == 1
    assert README.count("No stable release is available") == 1
    assert README.count("No files from the original game are included") == 1
    assert "### F10" not in README
    assert "## Adaptive widescreen HUD" not in README


def test_download_requirements_and_linux_override_remain_explicit():
    assert "SpiderMan2k1Fix-Windows-x86" in README
    assert "Actions" in README and "Artifacts" in README
    assert "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea" in README
    assert "winmm.dll" in README and "SpiderMan2k1Fix.dll" in README
    assert 'WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe' in README
    assert 'WINEDLLOVERRIDES="winmm=n,b" %command%' in README
    assert "No stable release is available" in README


def test_readme_links_to_documentation_instead_of_repeating_it():
    for path in (
        "docs/README.md",
        "docs/references/CONFIGURATION.md",
        "docs/references/F9_PNG_CAPTURE.md",
        "docs/DEVELOPMENT.md#7-display-and-rendering-research",
        "ChatGPT/README.md",
    ):
        assert path in README
    config = (ROOT / "docs/references/CONFIGURATION.md").read_text(encoding="utf-8")
    assert "## Upgrading from older builds" in config
    assert "SpiderManFix.ini" in config
    assert "SpiderManFix.dll" in config
    assert "keep your existing INI" in config


def test_readme_preserves_required_disclaimers_and_credit():
    assert "GPL-3.0-only" in README
    assert "No files from the original game are included" in README
    assert "own legally obtained copy" in README
    assert "not affiliated with, sponsored by, or endorsed by Activision" in README
    assert "[Krystalgamer](https://github.com/krystalgamer)" in README
