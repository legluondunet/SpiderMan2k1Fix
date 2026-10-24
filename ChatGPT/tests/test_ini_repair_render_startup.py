"""Startup invariant: malformed INI must not block or bypass display setup.

Static and host-side regressions complement real Wine/Windows DirectDraw tests.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
PROXY=(ROOT/"winmm_proxy.c").read_text()
MAIN=(ROOT/"spiderman2k1fix.c").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()
BUILD=(ROOT/"build.sh").read_text()


def between(text,begin,end):
    return text.split(begin,1)[1].split(end,1)[0]


def test_ini_repair_is_always_silent_even_if_unsuccessful():
    validator=between(CFG,
        "void SpiderCfg_ValidateCurrentIni(void (*logger)(const char *)) {",
        "/* Obsolete report files")
    assert "SpiderCfg_RepairIni" in validator
    assert "SpiderIni_Validate" in validator
    assert "ini_writes_permitted=0;" in validator
    assert 'if(logger)logger(summary);' in validator
    assert "MessageBoxA" not in validator
    assert "MessageBoxW" not in validator
    assert "MB_TOPMOST" not in validator
    assert "if(!SpiderCfg_RepairIni(" in validator
    assert "source unchanged" in validator


def test_proxy_installs_directdraw_gate_under_exact_native_signature():
    boot=between(PROXY,"static void install_early_ddraw_gate(void) {",
                 "static INIT_ONCE once")
    assert "early_patch_status!=1" in boot
    assert "0x13b010" in boot
    assert "IMAGE_NT_OPTIONAL_HDR32_MAGIC" in boot
    assert "IMAGE_DIRECTORY_ENTRY_IMPORT" in boot
    assert 'lstrcmpiA(library,"ddraw.dll")' in boot
    assert 'strcmp(name,"DirectDrawCreateEx")' in boot
    assert "PAGE_READWRITE" in boot
    assert "*slot=(void*)proxy_first_directdraw;" in boot
    dllmain=between(PROXY,"BOOL WINAPI DllMain(", "\n}") if False else PROXY.split("BOOL WINAPI DllMain(",1)[1]
    assert dllmain.index("patch_early();") < dllmain.index("install_early_ddraw_gate();")
    assert dllmain.index("install_early_ddraw_gate();") < dllmain.index("CreateThread(")
    assert "install_early_ddraw_gate();" not in PROXY.split("static DWORD WINAPI init_fix(",1)[1].split("BOOL WINAPI DllMain(",1)[0]


def test_no_loader_lock_wait_or_library_load_in_early_iat_installer():
    early=between(PROXY,"static void install_early_ddraw_gate(void) {",
                  "static INIT_ONCE once")
    assert "LoadLibrary" not in early
    assert "WaitForSingleObject" not in early
    assert "MessageBox" not in early
    assert "GetPrivateProfile" not in early
    call=between(PROXY,"static HRESULT WINAPI proxy_first_directdraw(", 
                 "static void install_early_ddraw_gate(")
    assert "proxy_load_fix_module()" in call
    assert 'GetProcAddress(core,"SpiderMan2k1Fix_DirectDrawGate")' in call
    assert "return E_FAIL;" in call
    assert "real_ddraw_create(guid" not in call  # no unsafe fallback


def test_game_first_directdraw_waits_for_core_once_initialized():
    core=between(MAIN,"static BOOL CALLBACK core_initialize_once(",
                 "BOOL WINAPI DllMain(")
    assert "initialize_body(NULL);" in core
    assert "InitOnceExecuteOnce(&core_start_once," in core
    assert "SpiderMan2k1Fix_DirectDrawGate(" in core
    assert "InterlockedCompareExchange(&core_start_success,0,0)!=1" in core
    assert "return DDERR_UNSUPPORTED;" in core
    assert "original_create=(CreateExFn)real_create;" in core
    assert "return hooked_create(guid,out,iid,outer);" in core
    initialize=between(MAIN,"static DWORD initialize_body(", 
                       "static BOOL CALLBACK core_initialize_once(")
    assert initialize.index("supported_exe()") < initialize.index("SpiderCfg_ValidateCurrentIni(log_line)")
    assert initialize.index("SpiderCfg_ValidateCurrentIni(log_line)") < initialize.index("install_ddraw_hook();")
    assert initialize.index("install_ddraw_hook();") < initialize.index("InterlockedExchange(&core_start_success,1);")


def test_native_monitor_selection_is_performed_before_original_game_cfg_read():
    creation=between(MAIN,"static HRESULT WINAPI hooked_create(",
                     "static void install_ddraw_hook(")
    assert creation.index("probe_desktop_mode(early);") < creation.index("SpiderCfg_BootstrapGameRead();")
    assert 'hr=original_coop(self,hwnd,DDSCL_NORMAL)' in MAIN
    assert "return DDERR_UNSUPPORTED;" in MAIN
    assert "hr=DD_OK; /* borderless window: never change monitor resolution */" in MAIN


def test_build_refuses_unpaired_proxy_without_startup_export():
    assert "SpiderMan2k1Fix_DirectDrawGate" in BUILD
    assert '"$OBJDUMP" -p "$probe_dir/SpiderMan2k1Fix.dll"' in BUILD
    assert "ERROR: missing SpiderMan2k1Fix_DirectDrawGate export" in BUILD
