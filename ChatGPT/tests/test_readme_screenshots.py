"""Verify that README screenshots are embedded and ship as valid WebP.

The images are screenshots of the user's game, not replacement game assets.
They are development/documentation resources, not part of Windows DLL artifacts.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
README = (ROOT / "README.md").read_text(encoding="utf-8")
SHOTS = (
    "assets/screenshots/main-menu.webp",
    "assets/screenshots/gameplay.webp",
)


def test_readme_gallery_appears_below_badges_and_above_description():
    start = README.index("### Screenshots")
    assert README.index("[![Windows x86 build](") < start
    assert README.index("[![Linux / Wine / Proton compatible](") < start
    assert start < README.index("SpiderMan2k1Fix is an **unofficial")
    assert 'width="49%"' in README
    for relative in SHOTS:
        assert f'<a href="{relative}"><img src="{relative}"' in README


def test_screenshot_blobs_exist_and_have_webp_headers():
    for relative in SHOTS:
        data = (ROOT / relative).read_bytes()
        assert data.startswith(b"RIFF"), relative
        assert data[8:12] == b"WEBP", relative
        assert 4000 < len(data) < 100_000


def test_ci_dll_artifact_does_not_ship_documentation_screenshots():
    workflow = (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").read_text(encoding="utf-8")
    assert "cp SpiderMan2k1Fix.dll winmm.dll README.md CHANGELOG.md dist/package/" in workflow
    assert "assets/screenshots/" not in workflow
