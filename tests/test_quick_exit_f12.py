"""F12 native-menu-dispatch prototype regression checks.

Source checks cannot establish that the game exits cleanly on Windows.
The new menu-only native dispatcher must be runtime tested before the feature
can be marked complete or promoted outside a draft PR.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
SRC=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
HDR=(ROOT/"quick_exit_f12.h").read_text(encoding="utf-8")
VAL=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
GEN=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
INI=(ROOT/"SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")


def test_f12_opt_in_and_original_unsafe_default_disabled():
    assert "QuickExitF12=0" in INI
    assert '"[System]\\r\\n"' in GEN
    assert '"QuickExitF12=0\\r\\n"' in GEN
    assert '"Debug","System","Display"' in VAL
    assert 'spider_ini_system[]={"QuickExitF12"}' in VAL
    assert 'GetPrivateProfileStringA("System","QuickExitF12","0"' in SRC
    assert 'if(strcmp(value,"1")!=0)' in SRC
    assert 'suppress_native_f12_close()' in SRC
    assert 'original unsafe F12/WM_CLOSE suppressed' in SRC


def test_exact_native_menu_quit_path_verification():
    # SHA-pinned 2001 EXE, call at 0x4A202F -> 0x493990.
    # Native main-menu action 18 -> table index 7 -> 0x4A238A
    # and call 0x48D880 at 0x4A2391 on the game's thread.
    for address in ("0x004A202Fu", "0x00493990u", "0x004A238Au",
                    "0x004A2391u", "0x00455E50u", "0x00515373u"):
        assert address in SRC
    for signature in ("0xE8,0x5C,0x19,0xFF,0xFF",
                      "0xC6,0x05,0x88,0xCF,0x60,0x00,0x01",
                      "0xE8,0xEA,0xB4,0xFE,0xFF",
                      "0x38,0x1D,0x88,0xCF,0x60,0x00"):
        assert signature in SRC
    assert "#define SPIDER_MENU_QUIT_ID 18" in SRC
    assert "return SPIDER_MENU_QUIT_ID;" in SRC
    assert "static int __cdecl hooked_native_menu_selection(int argument)" in SRC
    assert "int selected=SPIDER_MENU_SELECTOR(argument);" in SRC
    assert "native MENU Quit command 18 delivered on game thread" in SRC
    assert "install_rel32_call(SPIDER_MENU_SELECTOR_CALL" in SRC


def test_f12_input_worker_only_queues_request_without_engine_mutation():
    assert "GetAsyncKeyState(VK_F12)&0x8000" in SRC
    assert "GetForegroundWindow()==hwnd" in SRC
    assert "previously_down=TRUE;" in SRC
    assert "Sleep(25);" in SRC
    assert "InterlockedExchange(&menu_quit_pending,1);" in SRC
    assert "InterlockedExchange(&menu_quit_pending,0);" in SRC
    assert "SPIDER_F12_DISPATCH_TIMEOUT_MS 1200u" in SRC
    assert "menu loop not reached" in SRC
    assert "movie active; no native menu Quit dispatch yet" in SRC
    assert "SPIDER_MENU_SELECTOR(argument)" in SRC
    assert "SpiderQuickExitF12_AttachWindow(hwnd);" in CORE
    assert "SpiderQuickExitF12_Init(log_line);" in CORE
    assert "SpiderQuickExitF12_ObserveMovieFrame();" in CORE
    assert "quick_exit_f12.c" in BUILD
    assert "SpiderQuickExitF12_Enabled()" in CORE
    assert "SpiderQuickExitF12_ObserveMovieFrame(void);" in HDR
    # No speculative worker-side native teardown, quit-flag writes or input.
    for forbidden in ("*SPIDER_GAME_QUIT_FLAG=", "0x0048D880u",
                      "TerminateProcess(", "ExitProcess(", "PostMessageA(",
                      "DestroyWindow(", "SetWindowLongPtr(",
                      "SendInput(", "keybd_event(", "SetWindowsHookEx("):
        assert forbidden not in SRC


def test_installer_rolls_back_partial_hooks():
    assert "memcmp(SPIDER_NATIVE_F12_POST,native_f12_original,6)" in SRC
    assert "memcmp(SPIDER_MENU_SELECTOR_CALL,menu_selector_original,5)" in SRC
    assert "memcmp(SPIDER_MENU_LOOP_READ_CALL,menu_loop_read_original,5)" in SRC
    assert "memcmp(SPIDER_MENU_LOOP_READ_CALL+5,menu_loop_exit_compare_signature,4)" in SRC
    assert "if(!install_menu_loop_read())" in SRC
    assert "restore_menu_loop_read();" in SRC
    assert "if(!install_menu_dispatcher())" in SRC
    assert "if(!suppress_native_f12_close())" in SRC
    assert "restore_menu_dispatcher();" in SRC
    assert "restore_native_f12_close();" in SRC
    assert "FlushInstructionCache(GetCurrentProcess(),site,count);" in SRC
    assert "FlushInstructionCache(GetCurrentProcess(),site,5);" in SRC


def test_external_windows_watcher_is_readonly():
    script=(ROOT/"tools/Verify-F12QuickExit.ps1").read_text(encoding="utf-8")
    assert "Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru" in script
    assert "$watch.WaitForExit(500)" in script
    assert "$watch.ExitCode" in script
    assert 'Get-Process -Name "SpideyPC"' in script
    assert "Get-WinEvent -FilterHashtable" in script
    assert "Stop-Process" not in script
    assert ".Kill(" not in script
    assert "TerminateProcess" not in script


def test_readme_stays_clear_about_partial_menu_only_support():
    readme=(ROOT/"README.md").read_text(encoding="utf-8")
    assert "main-menu" in readme.lower()
    assert "cinematics" in readme.lower()
    assert "not yet" in readme.lower()
    assert "tools/Verify-F12QuickExit.ps1" in readme


def test_native_menu_loop_exit_read_replaces_only_expected_mov():
    # Exact game loop:
    # 004A1A21 CALL 0x505720 (normal frame tick)
    # 004A1A26 MOV EAX,[0x006B4CA0] (5 bytes)
    # 004A1A2B CMP EAX,EDI; JA 0x004A1A36 (normal cleanup)
    assert "#define SPIDER_MENU_LOOP_READ_CALL ((BYTE *)(uintptr_t)0x004A1A26u)" in SRC
    assert "#define SPIDER_MENU_LOOP_READ_PTR ((const volatile DWORD *)(uintptr_t)0x006B4CA0u)" in SRC
    assert "0xA1,0xA0,0x4C,0x6B,0x00" in SRC
    assert "0x3B,0xC7,0x77,0x07" in SRC
    assert "static DWORD __cdecl hooked_menu_loop_exit_read(void)" in SRC
    assert "DWORD original=*SPIDER_MENU_LOOP_READ_PTR;" in SRC
    assert "return 0xFFFFFFFFu;" in SRC
    assert "return original;" in SRC
    assert "native MENU loop exit branch requested on game thread" in SRC
    assert "install_rel32_call(SPIDER_MENU_LOOP_READ_CALL" in SRC
    assert "InterlockedCompareExchange(&menu_quit_pending,0,1)==1" in SRC
    assert "InterlockedExchange(&menu_quit_armed,1);" in SRC
    assert "InterlockedCompareExchange(&menu_quit_armed,0,1)==1" in SRC
    assert "native MENU loop exited; awaiting native Quit dispatch after cleanup" in SRC
    assert "abandoned stale native menu Quit after 15s" in SRC
    assert "MENU Quit command 18 delivered" in SRC
    assert "restore_menu_loop_read();" in SRC
    # The previous build #464 only intercepted 0x4A202F, after the menu
    # loop returned; it had no effect from idle menu.
    assert "menu loop not reached; F12 safely ignored outside menu" in SRC


def test_watcher_rejects_obsolete_flag_only_f12_success():
    script=(ROOT/"tools/Verify-F12QuickExit.ps1").read_text(encoding="utf-8")
    assert "native MENU Quit command 18 delivered on game thread" in script
    assert "game-native quit requested flag=1" not in script
