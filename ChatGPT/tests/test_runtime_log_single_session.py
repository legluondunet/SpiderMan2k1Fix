"""Only one game-process runtime log; early proxy/launcher diagnostics survive.

Standalone main still relies on WinMM's first-line reset. This small
shared-core change makes the same logger independent in unified builds.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
POLICY=(ROOT/"first_run_log_policy.h").read_text(encoding="utf-8")

def test_core_uses_a_single_session_aware_file_open():
    assert "static INIT_ONCE game_log_session_once=INIT_ONCE_STATIC_INIT;" in CORE
    proc=CORE.split("static BOOL CALLBACK game_log_prepare_session(",1)[1].split(
        "/* Windows creates a class=",1)[0]
    assert 'spider_path_log_a(game_log_session_path,' in proc
    assert "if(!first_run_log_enabled){" in proc
    assert 'f=fopen(game_log_session_path,"w");' in proc
    assert 'f=fopen(game_log_session_path,"a");' in proc
    assert "InitOnceExecuteOnce(&game_log_session_once," in proc
    assert proc.index('f=fopen(game_log_session_path,"w");') < proc.index(
        'f=fopen(game_log_session_path,"a");'
    )
    assert proc.index("if(!debug_log_enabled)return;") < proc.index(
        "InitOnceExecuteOnce(&game_log_session_once,"
    )

def test_legacy_proxy_keeps_ownership_of_early_log_reset():
    proc=CORE.split("static BOOL CALLBACK game_log_prepare_session(",1)[1].split(
        "static void log_line(",1)[0]
    assert "#ifdef SPIDER_UNIFIED_FIX" in proc
    assert "Legacy: winmm_proxy.c already truncates" in proc
    assert "return TRUE;" in proc.split("#else",1)[1]

def test_unified_first_launch_retains_early_launcher_preamble():
    proc=CORE.split("static BOOL CALLBACK game_log_prepare_session(",1)[1].split(
        "static void log_line(",1)[0]
    assert "if(!first_run_log_enabled){" in proc
    assert "if(SpiderFirstRunLog_IniMissingA(path))first_run_log_enabled=1;" in CORE
    assert "if(first_run_log_enabled)debug_log_enabled=1;" in CORE
    assert 'FIRST-RUN-LOG: automatic initial startup diagnostics' in CORE
    assert "GetFileAttributesA(ini)" in POLICY
    assert "GetFileAttributesW(ini)" in POLICY
    assert "WritePrivateProfileString" not in POLICY
    assert "DeleteFile" not in POLICY
