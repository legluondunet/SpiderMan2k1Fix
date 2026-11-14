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


def test_ci_builds_without_retaining_the_completed_one_shot_release_workflow():
    workflows = ROOT / ".github/workflows"
    # v1.1.0 was published on 2026-10-27. Its one-shot publisher must not
    # continue running (even as a skipped job) after the tag exists.
    # The experimental, separately permission-scoped installer workflow is
    # deliberately NOT an automatic release publisher; the historical
    # one-shot v1.1 release workflow must still be absent.
    assert sorted(p.name for p in workflows.glob("*.yml")) == [
        "spiderman2k1fix-standalone-windows-x86.yml",
        "spiderman2k1fix-windows-x86.yml",
    ]
    standalone=(workflows/"spiderman2k1fix-standalone-windows-x86.yml").read_text(encoding="utf-8")
    assert "NOT a release" in standalone
    assert "SpiderMan2k1Fix-Windows-x86-Standalone" in standalone
    assert "gh release create" not in standalone and "gh release upload" not in standalone
    assert sorted(p.name for p in ROOT.glob("RELEASE_NOTES_*.md")) == [
        "RELEASE_NOTES_1.0.0.md",
        "RELEASE_NOTES_1.1.0.md",
        "RELEASE_NOTES_2.0.0.md",
    ]
    stable_notes = (ROOT / "RELEASE_NOTES_2.0.0.md").read_text(encoding="utf-8")
    assert "# SpiderMan2k1Fix v2.0.0" in stable_notes
    assert "Windows 11" in stable_notes and "Wine" in stable_notes
    assert "SDL3" in stable_notes and "Game Over" in stable_notes
    assert "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea" in stable_notes
    new_notes = (ROOT / "RELEASE_NOTES_1.1.0.md").read_text(encoding="utf-8")
    assert "# SpiderMan2k1Fix v1.1.0" in new_notes
    assert "Linux/Wine" in new_notes
    assert "Windows" in new_notes
    notes = (ROOT / "RELEASE_NOTES_1.0.0.md").read_text(encoding="utf-8")
    assert "# SpiderMan2k1Fix v1.0.0" in notes
    assert "## Known Issues" in notes
    assert "Not published" not in notes
    assert "release candidate" not in notes.lower()

    ci = CI_PATH.read_text(encoding="utf-8")
    assert "permissions:\n  contents: read\n" in ci
    assert "python3 -m pytest -q ChatGPT/tests" in ci
    assert "bash ./build-native-msvcrt.sh" in ci
    assert "gh release create" not in ci
    assert "gh release upload" not in ci



def test_windows_dll_source_inventory_is_explicit_and_unambiguous():
    build = (ROOT / "build.sh").read_text(encoding="utf-8")
    start = build.index("fix_sources=(\n") + len("fix_sources=(\n")
    end = build.index("\n)", start)
    inventory = build[start:end]
    units = [
        part
        for line in inventory.splitlines()
        if line.strip() and not line.lstrip().startswith("#")
        for part in line.split()
    ]
    assert len(units) == 41
    assert len(units) == len(set(units)), "Duplicate DLL compilation unit"
    assert all(path.endswith(".c") and (ROOT / path).is_file() for path in units)
    assert units[0] == "spiderman2k1fix.c"
    assert units[-1] == "third_party/miniz/miniz_tinfl.c"
    assert "sdl3_universal_gameplay.c" in units
    assert "input_notacquired_retry.c" in units
    assert "pkr_audio_loader.c" in units
    assert "winmm_proxy.c" not in units  # separate proxy build
    assert '"${fix_sources[@]}"' in build

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
    assert re.search(r'^#define SPIDERMAN2K1FIX_VERSION "2\.0\.0(?:-rc\.\d+)?"$', version_header, re.M)
    assert 'log_line("START: " SPIDERMAN2K1FIX_VERSION_LABEL);' in game
    assert "[GitHub Releases](https://github.com/legluondunet/SpiderMan2k1Fix/releases)" in readme
    assert "No stable release is available" not in readme
    assert "1.0.0 release candidate" not in readme
