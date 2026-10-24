"""Ensure each game run replaces the preceding log without losing startup lines."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROXY = (ROOT / "winmm_proxy.c").read_text(encoding="utf-8")
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")


def test_proxy_starts_new_log_once_before_first_append():
    assert "static INIT_ONCE log_reset_once = INIT_ONCE_STATIC_INIT;" in PROXY
    assert 'f=fopen((const char*)arg,"w");' in PROXY
    reset_call = "InitOnceExecuteOnce(&log_reset_once,reset_log_on_first_write,path,NULL);"
    assert reset_call in PROXY
    proxy_log = PROXY[PROXY.index("static void log_proxy("):PROXY.index("static BOOL CALLBACK load_real(")]
    assert proxy_log.index('GetPrivateProfileIntA("Debug","Log",0,path)') < proxy_log.index(reset_call)
    assert proxy_log.index(reset_call) < proxy_log.index('f=fopen(path,"a")')
    assert proxy_log.count(reset_call) == 1


def test_no_second_reset_when_main_fix_dll_starts():
    # The proxy is first to log: its DPI-BOOT line precedes loading the main DLL.
    init_fix = PROXY[PROXY.index("static DWORD WINAPI init_fix("):]
    assert init_fix.index("enable_native_window_pixels();") < init_fix.index("LoadLibraryA(path)")
    main_log = CORE[CORE.index("static void log_line("):CORE.index("static void log_line(") + 800]
    assert 'fopen(path,"a")' in main_log
    assert 'fopen(path,"w")' not in main_log


def test_proxy_dllmain_does_not_perform_file_io():
    loader = PROXY[PROXY.index("BOOL WINAPI DllMain("):]
    assert "patch_early();" in loader
    assert "CreateThread(" in loader
    assert "fopen(" not in loader
