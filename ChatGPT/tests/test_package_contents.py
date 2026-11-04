"""Single directly extractable Windows x86 artifact regression tests.

CI compiles two Windows x86 DLLs. The 1.0.0 release was published
manually, not by CI.
The game creates its own user INI after first successful launch.
"""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CI_PATH = ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml"


def test_ci_release_package_contains_required_dlls_and_public_docs():
    workflow = CI_PATH.read_text(encoding="utf-8")
    artifact_section = workflow.split(
        "      - name: Prepare game installation files", 1
    )[1].split("      - name: Build release-ready ZIP and SHA256", 1)[0]

    assert "cp winmm.dll dist/package/" in artifact_section
    assert "cp SpiderMan2k1Fix.dll README.md CHANGELOG.md dist/package/SpiderMan2k1Fix/" in artifact_section
    assert "path: dist/package/" in workflow
    upload=workflow.split("      - name: Upload installation files as one ZIP (no nested ZIP)",1)[1]
    assert "path: dist/package/" in upload
    assert "dist/release/" not in upload
    assert workflow.count("uses: actions/upload-artifact@v4") == 1
    assert "name: SpiderMan2k1Fix-Windows-x86" in upload
    assert "name: SpiderMan2k1Fix-Release-ZIP" not in workflow
    # Pre-packaged release archives are retained for future publishing,
    # but are not uploaded into the outer GitHub Actions artifact ZIP.
    assert "Build release-ready ZIP and SHA256 (do not publish)" in workflow
    assert "cd dist/release" in workflow
    assert "SpiderMan2k1Fix.ini" not in artifact_section
    assert "ChatGPT/" not in artifact_section
    assert "docs/" not in artifact_section


def test_workflow_only_builds_and_does_not_publish_releases():
    workflows = ROOT / ".github/workflows"
    assert list(workflows.glob("publish-*.yml")) == []
    assert sorted(p.name for p in workflows.glob("*release*.yml")) == ["spiderman2k1fix-v1.1.0-release.yml"]
    assert sorted(p.name for p in ROOT.glob("RELEASE_NOTES_*.md")) == ["RELEASE_NOTES_1.0.0.md", "RELEASE_NOTES_1.1.0.md"]
    new_notes = (ROOT / "RELEASE_NOTES_1.1.0.md").read_text(encoding="utf-8")
    assert "# SpiderMan2k1Fix v1.1.0" in new_notes
    assert "Linux/Wine" in new_notes
    assert "Windows" in new_notes
    notes=(ROOT/"RELEASE_NOTES_1.0.0.md").read_text(encoding="utf-8")
    assert "# SpiderMan2k1Fix v1.0.0" in notes
    assert "## Known Issues" in notes
    assert "Not published" not in notes
    assert "release candidate" not in notes.lower()
    for path in workflows.glob("*.yml"):
        src = path.read_text(encoding="utf-8")
        if path.name == "spiderman2k1fix-v1.1.0-release.yml":
            assert "startsWith(github.event.head_commit.message, 'release: publish SpiderMan2k1Fix v1.1.0')" in src
            assert "github.repository == 'legluondunet/SpiderMan2k1Fix'" in src
            assert "python3 -m pytest -q ChatGPT/tests" in src
            assert "bash ./build-native-msvcrt.sh" in src
            assert "gh release create v1.1.0" in src
            assert "RELEASE_NOTES_1.1.0.md" in src
        else:
            assert "gh release create" not in src
            assert "gh release upload" not in src


def test_ci_runs_developer_regression_tests():
    workflow = CI_PATH.read_text(encoding="utf-8")
    assert "python3 -m pytest -q ChatGPT/tests" in workflow
    assert "bash ./build.sh" in workflow or "bash ./build-native-msvcrt.sh" in workflow


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


def test_1_0_stable_readme_links_to_releases_without_candidate_warning():
    version_header = (ROOT / "version_info.h").read_text(encoding="utf-8")
    game = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert '#define SPIDERMAN2K1FIX_VERSION "1.1.0"' in version_header
    assert 'log_line("START: " SPIDERMAN2K1FIX_VERSION_LABEL);' in game
    assert "[GitHub Releases](https://github.com/legluondunet/SpiderMan2k1Fix/releases)" in readme
    assert "No stable release is available" not in readme
    assert "1.0.0 release candidate" not in readme
