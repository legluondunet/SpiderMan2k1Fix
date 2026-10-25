"""README badges, platform support guidance and community acknowledgements."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
README = (ROOT / "README.md").read_text(encoding="utf-8")


def test_windows_badge_links_to_real_build_workflow():
    workflow = "actions/workflows/spiderman2k1fix-windows-x86.yml"
    assert f"{workflow}/badge.svg" in README
    assert "[![Windows x86 build](" in README
    assert (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").is_file()


def test_linux_wine_proton_badge_is_informational_and_has_instructions():
    assert "[![Linux / Wine / Proton compatible](https://img.shields.io/badge/" in README
    assert "](#installation-on-windows-and-linux-wine)" in README
    assert 'WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe' in README
    tested = "Successfully tested on Windows 11 and Linux/Wine."
    assert tested in README
    assert f"**{tested}**" not in README
    assert README.index("## Main Features") < README.index(tested) < README.index("## Bugs Resolved")


def test_krystalgamer_acknowledged_with_links_to_original_research():
    assert "## Credits" in README
    assert "[Krystalgamer](https://krystalgamer.github.io/index.html)" in README
    assert "reverse-engineering work on the original PC game" in README
