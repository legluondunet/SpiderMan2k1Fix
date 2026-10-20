"""Source-level safety checks for one diagnostic log per game launch.

The proxy owns early startup logging. It must truncate once and ONLY once
before writing the first line. The main DLL must keep append mode to avoid
erasing the early loader/proxy diagnostics.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROXY = (ROOT / "winmm_proxy.c").read_text()
CORE = (ROOT / "spiderman2k1fix.c").read_text()
INI = (ROOT / "SpiderMan2k1Fix.ini").read_text()
GENERATED = (ROOT / "virtual_spidey_cfg.c").read_text()
README = (ROOT / "README.md").read_text()


def test_proxy_truncates_once_before_first_enabled_message():
    assert "static INIT_ONCE log_session_once = INIT_ONCE_STATIC_INIT;" in PROXY
    assert "static BOOL CALLBACK begin_log_session(" in PROXY
    assert 'f=fopen(path,"w");' in PROXY
    assert 'InitOnceExecuteOnce(&log_session_once,begin_log_session,path,NULL)' in PROXY
    log_start = PROXY.index("static void log_proxy(")
    log_end = PROXY.index("static BOOL CALLBACK load_real(", log_start)
    logging = PROXY[log_start:log_end]
    assert 'GetPrivateProfileIntA("Debug","Log",0,path)!=1' in logging
    assert logging.index('GetPrivateProfileIntA("Debug","Log",0,path)') < logging.index(
        'InitOnceExecuteOnce(&log_session_once')
    assert logging.index('InitOnceExecuteOnce(&log_session_once') < logging.index(
        'f=fopen(path,"a")')
    assert PROXY.count('f=fopen(path,"w");') == 1


def test_core_dll_only_appends_and_keeps_early_proxy_messages():
    assert 'f=fopen(path,"a");' in CORE
    assert 'f=fopen(path,"w");' not in CORE
    assert "debug_log_load_setting();" in CORE
    assert CORE.index('static DWORD WINAPI initialize(') < CORE.index('debug_log_load_setting();', CORE.index('static DWORD WINAPI initialize('))
    # Proxy performs all file I/O from the init thread or forwarded calls,
    # not the loader-lock DllMain.
    dllmain = PROXY[PROXY.index("BOOL WINAPI DllMain("):]
    assert "begin_log_session(" not in dllmain
    assert "log_proxy(" not in dllmain
    assert "init_fix" in dllmain


def test_docs_and_defaults_are_consistent():
    assert "Log=0" in INI
    assert '"Log=0\\r\\n"' in GENERATED
    assert "replace SpiderMan2k1Fix.log at each launch" in INI
    assert "replace SpiderMan2k1Fix.log at each launch" in GENERATED
    assert "Each launch now **replaces**" in README
    assert "existing log is left untouched" in README
