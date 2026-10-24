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
    assert "](#installing-on-linux-with-wine)" in README
    assert 'WINEDLLOVERRIDES="winmm=n,b" %command%' in README
    assert "informational, not the result of a Linux/Proton gameplay CI test" in README


def test_krystalgamer_acknowledged_with_links_to_original_research():
    assert "## Credits and acknowledgements" in README
    assert "[Krystalgamer](https://github.com/krystalgamer)" in README
    assert "https://github.com/krystalgamer/spidey-decomp" in README
    assert "https://krystalgamer.github.io/spidey-breaking/index.html" in README
