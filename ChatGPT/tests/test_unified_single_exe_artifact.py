"""Keep one user-facing executable across GitHub Actions workflows.

Synthetic PE32 test programs are intentionally kept internal for native
Windows regression coverage and must never contaminate the player download.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
UNIFIED = ROOT / ".github/workflows/spiderman2k1fix-unified-x86.yml"
LEGACY = ROOT / ".github/workflows/spiderman2k1fix-standalone-windows-x86.yml"
BUILD = ROOT / "scripts/build_unified_installer.sh"


def test_single_user_facing_unified_exe_has_classic_name():
    build = BUILD.read_text(encoding="utf-8")
    workflow = UNIFIED.read_text(encoding="utf-8")
    assert "output=dist/unified/SpiderMan2k1Fix.exe" in build
    assert "SpiderMan2k1Fix-UNIFIED-TEST.exe" not in workflow
    assert "SpiderMan2k1Fix-UNIFIED-TEST.exe" not in build
    assert "test -s dist/unified/SpiderMan2k1Fix.exe" in workflow
    assert "name: SpiderMan2k1Fix\n          path: dist/unified/SpiderMan2k1Fix.exe" in workflow
    upload_section = workflow.split("      - name: Upload SpiderMan2k1Fix.exe only", 1)[1].split(
        "      - name: Cache internal Windows test harness", 1
    )[0]
    assert upload_section.count("name: SpiderMan2k1Fix\n          path:") == 1


def test_fixtures_and_native_helpers_never_ship_with_player_launcher():
    workflow = UNIFIED.read_text(encoding="utf-8")
    public = workflow.split("      - name: Upload SpiderMan2k1Fix.exe only", 1)[1].split(
        "      - name: Cache internal Windows test harness", 1
    )[0]
    assert "dist/unified/SpiderMan2k1Fix.exe" in public
    assert "FIXTURE" not in public and "test.exe" not in public
    assert "path: |" not in public, "player download must be one file"
    assert "name: SpiderMan2k1Fix-CI-Internal" not in workflow
    assert workflow.count("uses: actions/upload-artifact@v4") == 1
    assert workflow.count("uses: actions/download-artifact@v4") == 1
    assert "name: SpiderMan2k1Fix\\n          path: dist/unified" in workflow

    # Pass the native PE32 test helpers between Linux and Windows without
    # uploading a second GitHub Actions Artifacts package.
    assert "uses: actions/cache/save@v4" in workflow
    assert "uses: actions/cache/restore@v4" in workflow
    assert workflow.count("enableCrossOsArchive: true") == 2
    assert "fail-on-cache-miss: true" in workflow
    key = "key: spider-unified-internal-${{ github.run_id }}-${{ github.run_attempt }}"
    assert workflow.count(key) == 2
    for exe in (
        "SpiderMan2k1Fix-UNIFIED-FIXTURE-TEST-ONLY.exe",
        "suspended-pe32-test.exe",
        "first-run-log-policy-test.exe",
        "dpi-manifest-test.exe",
    ):
        assert workflow.count("dist/unified/" + exe) >= 2


def test_legacy_installer_does_not_publish_another_build_on_main():
    legacy = LEGACY.read_text(encoding="utf-8")
    assert "on:\n  workflow_dispatch:\n" in legacy
    assert "  push:" not in legacy
    assert "  pull_request:" not in legacy
    assert "SpiderMan2k1Fix-Windows-x86-Standalone" in legacy
