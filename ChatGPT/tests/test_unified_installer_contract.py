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
    assert '1109 RCDATA "dist/unified/package/game/SpideyPC.exe.manifest"' in build
    assert 'L"game\\\\SpideyPC.exe.manifest"' in src
    assert "RES_DPI_MANIFEST" in src
    assert "OWNED_COUNT 8" in src

def test_fixture_hash_is_exact_and_injected_failure_is_test_only():
    generator=(ROOT/"scripts/build_unified_test_fixture.py").read_text()
    assert "assert text.count(retail)==1" in generator
    assert "SPIDER_UNIFIED_TEST_FAIL" in generator
    assert "output.write_text" in generator
    installer=(ROOT/"installer/spiderman2k1fix_installer.c").read_text()
    assert "SPIDER_UNIFIED_TEST_FAIL" not in installer



def test_unified_fix_auto_repair_is_transactional_and_does_not_keep_backup():
    src=(ROOT/"installer/spiderman2k1fix_installer.c").read_text()
    fixture=(ROOT/"scripts/build_unified_test_fixture.py").read_text()
    assert "static int replace_fix_without_backup(unsigned i)" in src
    assert "return i==0u;" in src
    assert "const wchar_t *rollback_slot=replace_fix_without_backup(i)?previous:backup;" in src
    assert "MoveFileExW(replace_fix_without_backup(j)?previous:backup," in src
    assert 'L"previous-bak-0"' in src
    assert "DeleteFileW(previous)" in src
    assert 'game\\\\fix.dll differs from embedded version; replacing automatically without .bak' in src
    assert 'resource_verify(RES_FIX,game_dir,L"fix.dll")' in src
    assert 'L"state\\\\installed-game.sha256"' in src
    assert "SPIDER_UNIFIED_TEST_UPGRADE_FAIL" in fixture
    assert "SPIDER_UNIFIED_TEST_UPGRADE_FAIL" not in src


def test_unified_dpi_shim_is_child_scoped_and_before_createprocess():
    src=(ROOT/"installer/spiderman2k1fix_installer.c").read_text()
    core=(ROOT/"spiderman2k1fix.c").read_text()
    start=src.split("static int launch_unified_game(int verify_exe) {",1)[1].split(
        "static int launch_game(",1)[0]
    assert 'GetEnvironmentVariableW(L"__COMPAT_LAYER",old_compat,' in start
    assert 'spider_compat_has_token(old_compat,L"DPIUNAWARE")' in start
    assert 'spider_compat_has_token(old_compat,L"GDIDPISCALING")' in start
    assert 'spider_compat_has_token(old_compat,L"HIGHDPIAWARE")' in start
    assert 'SetEnvironmentVariableW(L"__COMPAT_LAYER",child_compat)' in start
    assert start.index('SetEnvironmentVariableW(L"__COMPAT_LAYER",child_compat)') < start.index(
        "CreateProcessW(path,cmd"
    )
    assert start.count('SetEnvironmentVariableW(L"__COMPAT_LAYER",') >= 3
    assert "DPI-LAUNCH: requested HIGHDPIAWARE for child only" in start
    assert "spider_log_runtime_dpi_context(hwnd);" in core
    assert "GetWindowDpiAwarenessContext" in core
    assert "GetProcessDpiAwareness" in core
    assert "RegSetValue" not in start
    assert "SetProcessDPIAware" not in start
