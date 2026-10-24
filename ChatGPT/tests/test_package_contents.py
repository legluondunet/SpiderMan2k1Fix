"""Development artifact regression tests.

CI compiles two Windows x86 DLLs. Version 1.0.0 is a candidate;
publication is manual and not performed by CI.
The game creates its own user INI after first successful launch.
"""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CI_PATH = ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml"


def test_ci_artifact_contains_only_two_dlls_and_public_docs():
    workflow = CI_PATH.read_text(encoding="utf-8")
    artifact_section = workflow.split(
        "      - name: Prepare game installation files", 1
    )[1].split("      - name: Upload Windows DLL artifact", 1)[0]

    assert "cp SpiderMan2k1Fix.dll winmm.dll README.md CHANGELOG.md dist/package/" in artifact_section
    assert "path: dist/package/" in workflow
    assert "SpiderMan2k1Fix.ini" not in artifact_section
    assert "ChatGPT/" not in artifact_section
    assert "docs/" not in artifact_section


def test_workflow_only_builds_and_does_not_publish_releases():
    workflows = ROOT / ".github/workflows"
    assert list(workflows.glob("publish-*.yml")) == []
    assert list(workflows.glob("*release*.yml")) == []
    assert [p.name for p in ROOT.glob("RELEASE_NOTES_*.md")] == ["RELEASE_NOTES_1.0.0.md"]
    notes=(ROOT/"RELEASE_NOTES_1.0.0.md").read_text(encoding="utf-8")
    assert "Not published" in notes
    assert "manual; intentionally not automated" in notes
    for path in workflows.glob("*.yml"):
        src = path.read_text(encoding="utf-8")
        assert "gh release create" not in src
        assert "gh release upload" not in src


def test_ci_runs_developer_regression_tests():
    workflow = CI_PATH.read_text(encoding="utf-8")
    assert "python3 -m pytest -q ChatGPT/tests" in workflow
    assert "bash ./build.sh" in workflow


def test_developer_directories_are_excluded_from_future_source_archives():
    attrs = (ROOT / ".gitattributes").read_text(encoding="utf-8")
    assert re.search(r"(?m)^/docs/\s+export-ignore\s*$", attrs)
    assert re.search(r"(?m)^/ChatGPT/\s+export-ignore\s*$", attrs)


def test_user_ini_is_generated_at_runtime_not_committed_to_repo_root():
    game = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
    config = (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
    assert "SpiderCfg_EnsureDefaultIni(log_line);" in game
    assert "CREATE_NEW" in config
    assert (ROOT / "ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini").is_file()
    assert not (ROOT / "SpiderMan2k1Fix.ini").exists()
    assert not (ROOT / "SpiderMan2k1Fix.generated-example.ini").exists()


def test_1_0_release_candidate_version_does_not_claim_published_release():
    version_header = (ROOT / "version_info.h").read_text(encoding="utf-8")
    game = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert '#define SPIDERMAN2K1FIX_VERSION "1.0.0"' in version_header
    assert 'log_line("START: " SPIDERMAN2K1FIX_VERSION_LABEL);' in game
    assert "/releases/tag/" not in readme
    assert "No stable release is available" in readme
    assert "1.0.0 release candidate" in readme
