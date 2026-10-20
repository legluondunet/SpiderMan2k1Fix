"""Package/install contract: user INI must never be shipped by release archives.

SpiderMan2k1Fix.ini remains tracked as the *source* of factory defaults,
and it is generated at runtime if missing. This does not authorize shipping
that file in the binary ZIP where extraction might overwrite local settings.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CI = (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()
PUBLISH = (ROOT / ".github/workflows/publish-v1.0.0.yml").read_text()
README = (ROOT / "README.md").read_text()
CORE = (ROOT / "virtual_spidey_cfg.c").read_text()
INI = (ROOT / "SpiderMan2k1Fix.ini").read_text()


def test_ci_artifact_contains_only_dlls_and_docs():
    assert "cp SpiderMan2k1Fix.dll winmm.dll README.md CHANGELOG.md dist/package/" in CI
    assert "SpiderMan2k1Fix.dll winmm.dll README.md CHANGELOG.md)" in CI
    assert "SpiderMan2k1Fix.dll winmm.dll SpiderMan2k1Fix.ini" not in CI
    assert "unzip -Z1" in CI
    assert "grep -Fxq SpiderMan2k1Fix.ini" in CI


def test_official_release_does_not_package_user_ini():
    assert "winmm.dll SpiderMan2k1Fix.dll README.md CHANGELOG.md" in PUBLISH
    assert "winmm.dll SpiderMan2k1Fix.dll SpiderMan2k1Fix.ini" not in PUBLISH
    assert "unzip -Z1" in PUBLISH
    assert "grep -Fxq SpiderMan2k1Fix.ini" in PUBLISH


def test_runtime_generation_remains_create_only():
    assert "SpiderCfg_EnsureDefaultIni" in CORE
    assert "CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL" in CORE
    assert "if((error==ERROR_FILE_EXISTS || error==ERROR_ALREADY_EXISTS) && allow_existing) return 1;" in CORE
    assert "[Debug]" in INI and "Log=0" in INI
    assert "When upgrading from an older archive" in README
    assert "extract **only**" in README
