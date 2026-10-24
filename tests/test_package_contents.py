"""Packaging regression: ship only the two DLLs and public release notes.

The historical research documentation remains tracked in Git, but must not
appear in Windows binary artifacts or GitHub-generated source code archives.
"""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def test_actions_artifact_does_not_include_user_configuration():
    workflow = (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()
    assert "cp SpiderMan2k1Fix.dll winmm.dll README.md CHANGELOG.md dist/package/" in workflow
    assert "path: dist/package/" in workflow
    assert "SpiderMan2k1Fix.ini" not in workflow


def test_actions_artifact_excludes_development_docs():
    workflow = (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()
    # Only these four files are copied to the uploaded artifact directory.
    preparation = workflow.split("      - name: Prepare game installation files", 1)[1].split(
        "      - name: Upload Windows DLL artifact", 1
    )[0]
    assert "cp SpiderMan2k1Fix.dll winmm.dll README.md CHANGELOG.md dist/package/" in preparation
    assert "docs/" not in preparation
    assert "path: dist/package/" in workflow


def test_release_zip_does_not_include_user_configuration():
    workflow = (ROOT / ".github/workflows/publish-v1.0.0.yml").read_text()
    assert "winmm.dll SpiderMan2k1Fix.dll README.md CHANGELOG.md" in workflow
    assert "SpiderMan2k1Fix.ini" not in workflow


def test_binary_release_zip_excludes_development_docs():
    workflow = (ROOT / ".github/workflows/publish-v1.0.0.yml").read_text()
    packaging = workflow.split("      - name: Package release and checksum", 1)[1].split(
        "      - name: Publish official v1.0.0 release", 1
    )[0]
    assert "zip -j -9 dist/SpiderMan2k1Fix-1.0.0-Windows-x86.zip" in packaging
    assert "winmm.dll SpiderMan2k1Fix.dll README.md CHANGELOG.md" in packaging
    assert "docs/" not in packaging


def test_future_github_source_archives_exclude_development_docs():
    # GitHub auto-generated Source code (zip/tar.gz) releases use git archive,
    # which honors export-ignore from .gitattributes at the tagged commit.
    attrs = (ROOT / ".gitattributes").read_text()
    assert re.search(r"(?m)^/docs/\\s+export-ignore\\s*$", attrs)


def test_default_ini_is_still_generated_on_first_launch():
    game = (ROOT / "spiderman2k1fix.c").read_text()
    config = (ROOT / "virtual_spidey_cfg.c").read_text()
    assert "SpiderCfg_EnsureDefaultIni(log_line);" in game
    assert "CREATE_NEW" in config
    assert (ROOT / "SpiderMan2k1Fix.ini").is_file()
