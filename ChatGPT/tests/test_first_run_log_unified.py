"""Unified first-launch diagnostics: effective Log=1 once, persistent Log=0.

The detailed Windows Win32 policy harness also exercises missing / existing
user INI behavior; this file guards actual core / launcher integration.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
INSTALLER = (ROOT / "installer/spiderman2k1fix_installer.c").read_text(encoding="utf-8")
CFG = (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
POLICY = (ROOT / "first_run_log_policy.h").read_text(encoding="utf-8")


def test_first_session_force_only_in_unified_core_before_ini_creation():
    assert '#ifdef SPIDER_UNIFIED_FIX\n#include "first_run_log_policy.h"' in CORE
    log = CORE.split("static void debug_log_load_setting(void) {", 1)[1].split(
        "static void log_line(", 1
    )[0]
    assert 'debug_log_enabled=(GetPrivateProfileIntA("Debug","Log",0,path)==1);' in log
    assert "SpiderFirstRunLog_IniMissingA(path)" in log
    assert "debug_log_enabled=1;" in log
    assert "first_run_log_enabled=1;" in log
    assert '#ifdef SPIDER_UNIFIED_FIX' in log
    assert CORE.index("debug_log_load_setting();") < CORE.index(
        "SpiderCfg_EnsureDefaultIni(log_line);"
    )
    assert 'FIRST-RUN-LOG: automatic initial startup diagnostics' in CORE


def test_generated_ini_is_permanently_zero_unless_user_changes_it():
    assert '"Log=0\\r\\n"' in CFG
    assert 'WritePrivateProfileStringA("Debug","Log","0"' not in CORE
    assert 'WritePrivateProfileStringA("Debug","Log","0"' not in INSTALLER
    assert '"Log=1\\r\\n"' not in CFG


def test_launcher_writes_early_first_session_diagnostic_without_extra_state():
    launch = INSTALLER.split("static int launch_unified_game(", 1)[1].split(
        "static int launch_game(", 1
    )[0]
    assert '#ifdef SPIDER_UNIFIED_INSTALL' in INSTALLER
    assert "SpiderFirstRunLog_IniMissingW(first_ini)" in launch
    assert 'L"SpiderMan2k1Fix.ini"' in launch
    assert 'L"SpiderMan2k1Fix.log"' in launch
    assert '_wfopen(runtime_log,L"ab")' in launch
    assert "FIRST-RUN-LOG: initial launcher startup" in launch
    assert launch.index("SpiderFirstRunLog_IniMissingW(first_ini)") < launch.index(
        "CreateProcessW("
    )
    assert 'L"first-run-logged"' not in INSTALLER
    assert "state\\\\installed-game.sha256" in INSTALLER
    assert "state\\\\installed-resources.sha256" in INSTALLER


def test_missing_ini_only_file_not_found_and_existing_choice_never_overwritten():
    assert "GetFileAttributesA(ini)" in POLICY
    assert "GetFileAttributesW(ini)" in POLICY
    assert POLICY.count(
        "attr==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND"
    ) == 2
    assert "WritePrivateProfileString" not in POLICY
    assert "CreateFile" not in POLICY
    assert "DeleteFile" not in POLICY

def test_first_session_log_is_latched_after_factory_ini_generation():
    load=CORE.split("static void debug_log_load_setting(void) {",1)[1].split(
        "static void log_line(",1
    )[0]
    assert "if(SpiderFirstRunLog_IniMissingA(path))first_run_log_enabled=1;" in load
    assert "if(first_run_log_enabled)debug_log_enabled=1;" in load
    assert "first_run_log_enabled=0;" not in load
    assert CORE.count("debug_log_load_setting();") >= 2
