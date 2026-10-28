"""Verify README screenshot and animated comparison assets.

These are user-supplied gameplay captures used for documentation only,
not replacement game assets or part of the distributable DLL artifact.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
README = (ROOT / "README.md").read_text(encoding="utf-8")
SHOTS = (
    "assets/screenshots/main-menu.webp",
    "assets/screenshots/aspect-ratio-comparison.gif",
)


def test_readme_gallery_appears_below_intro_and_above_features():
    start = README.index("## Screenshots")
    assert README.index("[![Windows x86 build](") < start
    assert README.index("[![Linux / Wine / Proton compatible](") < start
    assert README.index("SpiderMan2k1Fix is an **unofficial") < start
    assert start < README.index("## Main Features")
    gallery = README[start:README.index("## Main Features")]
    assert "<table>" in gallery and "</table>" in gallery
    assert gallery.count('<td width="50%" align="center">') == 2
    assert gallery.count('width="100%"') == 2
    for relative in SHOTS:
        assert f'href="{relative}"' in gallery
        assert f'src="{relative}"' in gallery


def test_readme_image_blobs_exist_and_have_valid_headers():
    for relative in SHOTS:
        data = (ROOT / relative).read_bytes()
        if relative.endswith(".webp"):
            assert data.startswith(b"RIFF"), relative
            assert data[8:12] == b"WEBP", relative
            assert 4000 < len(data) < 100_000
        else:
            assert relative.endswith(".gif")
            assert data.startswith((b"GIF87a", b"GIF89a")), relative
            assert 100_000 < len(data) < 3_000_000


def test_ci_dll_artifact_does_not_ship_documentation_screenshots():
    workflow = (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").read_text(encoding="utf-8")
    assert "cp winmm.dll dist/package/" in workflow
    assert "cp SpiderMan2k1Fix.dll README.md CHANGELOG.md dist/package/SpiderMan2k1Fix/" in workflow
    assert "assets/screenshots/" not in workflow
