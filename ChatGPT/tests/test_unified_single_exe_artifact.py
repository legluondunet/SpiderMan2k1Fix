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
    assert workflow.count("name: SpiderMan2k1Fix\n          path:") == 1


def test_fixtures_and_native_helpers_never_ship_with_player_launcher():
    workflow = UNIFIED.read_text(encoding="utf-8")
    public = workflow.split("      - name: Upload SpiderMan2k1Fix.exe only", 1)[1].split(
        "      - name: Upload internal Windows test harness", 1
    )[0]
    assert "dist/unified/SpiderMan2k1Fix.exe" in public
    assert "FIXTURE" not in public and "test.exe" not in public
    assert "path: |" not in public, "player download must be one file"
    assert "name: SpiderMan2k1Fix-CI-Internal" in workflow
    assert "retention-days: 1" in workflow
    assert "name: SpiderMan2k1Fix-CI-Internal\n          path: dist/unified" in workflow
    assert "name: SpiderMan2k1Fix\n          path: dist/unified" in workflow
    assert workflow.count("uses: actions/upload-artifact@v4") == 2


def test_legacy_installer_does_not_publish_another_build_on_main():
    legacy = LEGACY.read_text(encoding="utf-8")
    assert "on:\n  workflow_dispatch:\n" in legacy
    assert "  push:" not in legacy
    assert "  pull_request:" not in legacy
    assert "SpiderMan2k1Fix-Windows-x86-Standalone" in legacy
