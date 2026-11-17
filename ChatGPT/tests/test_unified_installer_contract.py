"""Experimental unified installer source contracts (Windows tests complement)."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_unified_installer_is_opt_in_and_installs_root_receipts():
    src=(ROOT/"installer/spiderman2k1fix_installer.c").read_text()
    build=(ROOT/"scripts/build_unified_installer.sh").read_text()
    for required in ("#ifdef SPIDER_UNIFIED_INSTALL","CREATE_SUSPENDED",
                     "SpiderUnified_AliasSuspendedDDraw(pi.hProcess)",
                     "SpiderUnified_PatchSuspendedNoCD(pi.hProcess)",
                     "TerminateProcess(pi.hProcess",
                     "ResumeThread(pi.hThread)",
                     'L"state\\\\installed-game.sha256"',
                     'L"state\\\\installed-resources.sha256"',
                     'L"third_party\\\\SDL3\\\\SDL3.dll"',
                     'L"docs\\\\LICENSE"','L"linux\\\\spiderman-focus-helper"',
                     "original_installed_path(","write_game_receipt(stage)",
                     "verify_install_receipt(stage)","rolling back"):
        assert required in src,required
    assert "SPIDER_UNIFIED_INSTALL" in build
    assert "installer/unified_import_alias.c" in build
    assert '1102 RCDATA "dist/unified/package/game/fix.dll"' in build
    assert '1108 RCDATA "dist/unified/package/docs/LICENSE"' in build

def test_fixture_hash_is_exact_and_injected_failure_is_test_only():
    generator=(ROOT/"scripts/build_unified_test_fixture.py").read_text()
    assert "assert text.count(retail)==1" in generator
    assert "SPIDER_UNIFIED_TEST_FAIL" in generator
    assert "output.write_text" in generator
    installer=(ROOT/"installer/spiderman2k1fix_installer.c").read_text()
    assert "SPIDER_UNIFIED_TEST_FAIL" not in installer
