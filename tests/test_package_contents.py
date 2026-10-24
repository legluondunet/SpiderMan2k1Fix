"""Packaging regression: user-generated INI must not ship with the DLLs."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_actions_artifact_does_not_include_user_configuration():
    workflow = (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()
    assert "cp SpiderMan2k1Fix.dll winmm.dll README.md CHANGELOG.md dist/package/" in workflow
    assert "path: dist/package/" in workflow
    assert "SpiderMan2k1Fix.ini" not in workflow


def test_release_zip_does_not_include_user_configuration():
    workflow = (ROOT / ".github/workflows/publish-v1.0.0.yml").read_text()
    assert "winmm.dll SpiderMan2k1Fix.dll README.md CHANGELOG.md" in workflow
    assert "SpiderMan2k1Fix.ini" not in workflow


def test_default_ini_is_still_generated_on_first_launch():
    game = (ROOT / "spiderman2k1fix.c").read_text()
    config = (ROOT / "virtual_spidey_cfg.c").read_text()
    assert "SpiderCfg_EnsureDefaultIni(log_line);" in game
    assert "CREATE_NEW" in config
    assert (ROOT / "SpiderMan2k1Fix.ini").is_file()
