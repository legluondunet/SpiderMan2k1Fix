"""F12 REAL Shell_MainMenu native Quit path source checks.

Passing tests cannot prove a clean process exit on Windows. The user's
previous build #468 test ruled out 0x4A1A26: that was not the main menu.
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


def test_default_off_and_ini_section_validated():
    assert "QuickExitF12=0" in INI
    assert '"[System]\\r\\n"' in GEN
    assert '"QuickExitF12=0\\r\\n"' in GEN
    assert '"Debug","System","Display"' in VAL
    assert 'spider_ini_system[]={"QuickExitF12"}' in VAL
    assert 'GetPrivateProfileStringA("System","QuickExitF12","0"' in SRC
    assert 'if(strcmp(val,"1")!=0)' in SRC
    assert 'OFF; unsafe original F12 WM_CLOSE suppressed' in SRC


def test_verified_real_shell_menu_tick_and_quit_selection():
    assert "0x00493990" in SRC  # REAL Shell_MainMenu
    assert "0x00493DD6u" in SRC
    assert "0x00505720u" in SRC
    assert "0x004941D5u" in SRC
    assert "0x00552AA8u" in SRC
    assert "0xE8,0x45,0x19,0x07,0x00" in SRC
    assert "0x6A,0x00,0x68,0x00,0x20" in SRC
    assert "SPIDER_SHELL_MENU_ITEMS+3*40+12" in SRC
    assert "!=0x12u" in SRC
    assert 'real Shell_MainMenu code/table signature mismatch' in SRC
    assert 'REAL Shell_MainMenu frame callback observed' in SRC
    # This is NOT the previously mistaken secondary-menu 0x4A1A26 hook:
    assert "0x004A1A26" in SRC  # only in historical explanation
    assert "#define SPIDER_MENU_LOOP_READ_CALL" not in SRC


def test_f12_game_thread_uses_native_menu_cleanup_without_calls_from_worker():
    assert "native Shell_MainMenu Quit activated: slot=3 action=18" in SRC
    assert "InterlockedCompareExchange(&pending,0,1)==1" in SRC
    assert "movl $3, 0x68(%esp)" in SRC
    assert "movl $0x004941D5, 0x24(%esp)" in SRC
    assert "pushfl" in SRC and "pushal" in SRC
    assert "popal" in SRC and "popfl" in SRC
    assert "call _SpiderQuickExitF12_ConsumeRealMenuRequest" in SRC
    assert '__attribute__((naked,used))' in SRC
    assert "movl $0x00505720, %eax" in SRC
    assert "call *%eax" in SRC
    # No direct invocation of the native cleanup or Win32 close functions.
    for forbidden in ("TerminateProcess(", "ExitProcess(", "PostMessageA(",
                      "DestroyWindow(", "SetWindowLongPtr(", "SendInput(",
                      "keybd_event(", "SetWindowsHookEx(",
                      "0x0048D880u", "*SPIDER_GAME_QUIT_FLAG="):
        assert forbidden not in SRC


def test_foreground_key_detection_and_movie_to_native_quit_handoff():
    assert "GetAsyncKeyState(VK_F12)&0x8000" in SRC
    assert "GetForegroundWindow()==hwnd" in SRC
    assert "previously_down=TRUE;" in SRC
    assert "Sleep(25);" in SRC
    assert "REAL Shell_MainMenu or startup movie not active; F12 safely ignored" in SRC
    assert "intro movie exit requested; native Bink cleanup then real Shell_MainMenu Quit" in SRC
    assert "F12 queued for REAL Shell_MainMenu" in SRC
    assert "InterlockedExchange(&pending,1);" in SRC
    assert "InterlockedExchange(&pending,0);" in SRC
    assert "native menu frame not reached within 800ms; no exit" in SRC
    assert "SpiderQuickExitF12_AttachWindow(hwnd);" in CORE
    assert "SpiderQuickExitF12_Init(log_line);" in CORE
    assert "SpiderQuickExitF12_ObserveMovieFrame();" in CORE
    assert "SpiderQuickExitF12_Enabled()" in CORE
    assert "quick_exit_f12.c" in BUILD
    assert "SpiderQuickExitF12_ObserveMovieFrame(void);" in HDR


def test_native_site_patch_reverts_if_f12_suppression_fails():
    assert "memcmp(SPIDER_NATIVE_F12_POST,native_f12_original,6)" in SRC
    assert "memcmp(SPIDER_SHELL_MENU_TICK,shell_menu_tick_original,5)" in SRC
    assert "memcmp(SPIDER_SHELL_MENU_QUIT_RETURN" in SRC
    assert "if(!install_real_menu_hook())" in SRC
    assert "if(!suppress_unsafe_native_f12())" in SRC
    assert "restore_original_menu_tick();" in SRC
    assert "FlushInstructionCache(GetCurrentProcess(),address,count);" in SRC
    assert "FlushInstructionCache(GetCurrentProcess(),SPIDER_SHELL_MENU_TICK,5);" in SRC


def test_windows_process_watch_has_no_kill_side_effects():
    script=(ROOT/"tools/Verify-F12QuickExit.ps1").read_text(encoding="utf-8")
    assert "Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru" in script
    assert "$watch.ExitCode" in script
    assert "Get-WinEvent -FilterHashtable" in script
    assert "Stop-Process" not in script and ".Kill(" not in script


def test_readme_describes_menu_validated_and_movies_experimental():
    readme=(ROOT/"README.md").read_text(encoding="utf-8")
    assert "Shell_MainMenu" in readme
    assert "0x00493DD6" in readme
    assert "main menu" in readme.lower()
    assert "cinematics" in readme.lower()
    assert "not yet" in readme.lower()
    assert "tools/Verify-F12QuickExit.ps1" in readme


def test_four_intro_movies_run_original_bink_cleanup_on_game_thread():
    # Four sequential 0x470750 game calls (intro slates/cinematics) share
    # two exact original BinkFrame tick sites, whose false-result path
    # branches to the native 0x470871 BinkClose cleanup continuation.
    assert "#define SPIDER_INTRO_TICK_CALL_1 ((BYTE *)(uintptr_t)0x0047081Du)" in SRC
    assert "#define SPIDER_INTRO_TICK_CALL_2 ((BYTE *)(uintptr_t)0x0047085Fu)" in SRC
    assert "0x0050B5A0u" in SRC
    assert "0x006151F8u" in SRC
    assert "0xE8,0x7E,0xAD,0x09,0x00" in SRC
    assert "0xE8,0x3C,0xAD,0x09,0x00" in SRC
    assert "static BYTE __cdecl spider_f12_native_intro_tick(void)" in SRC
    assert "if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1)" in SRC
    assert "return 0;" in SRC
    assert "return SPIDER_NATIVE_BINK_FRAME_TICK();" in SRC
    assert "native Bink tick returned finished; original game video cleanup will run" in SRC
    assert "BinkClose(" not in SRC
    assert "PostMessageA(" not in SRC
    assert "TerminateProcess(" not in SRC


def test_intro_exit_latch_survives_all_movies_then_consumed_by_validated_menu():
    assert "static volatile LONG intro_quit_pending=0;" in SRC
    assert "InterlockedCompareExchange(&intro_quit_pending,1,0)==0" in SRC
    assert "InterlockedCompareExchange(&intro_quit_pending,0,1)==1" in SRC
    assert SRC.index("InterlockedCompareExchange(&intro_quit_pending,0,1)==1") < SRC.index("if(InterlockedCompareExchange(&pending,0,1)==1)")
    assert "intro Bink cleanup completed; native Shell_MainMenu Quit activated" in SRC
    assert "movl $3, 0x68(%esp)" in SRC
    assert "movl $0x004941D5, 0x24(%esp)" in SRC
    assert "*SPIDER_GAME_BINK_ACTIVE!=0u" in SRC
    assert "movie_time)<750u" in SRC


def test_intro_calls_are_signature_checked_and_install_rollbacks_complete():
    assert "memcmp(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1,5)" in SRC
    assert "memcmp(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2,5)" in SRC
    assert "if(!patch_intro_tick_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1))" in SRC
    assert "if(!patch_intro_tick_call(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2))" in SRC
    assert "restore_intro_tick_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1);" in SRC
    assert "if(!install_intro_tick_hooks())" in SRC
    assert "restore_intro_tick_hooks();" in SRC
    assert "FlushInstructionCache(GetCurrentProcess(),site,5);" in SRC


def test_only_initial_four_movies_can_trigger_deferred_quit():
    assert "static volatile LONG real_main_menu_seen=0;" in SRC
    assert "InterlockedExchange(&real_main_menu_seen,1);" in SRC
    assert "InterlockedCompareExchange(&real_main_menu_seen,0,0)==0" in SRC
    assert "if(menu_time && (DWORD)(now-(DWORD)menu_time)<=400u)" in SRC
    assert SRC.index("if(menu_time && (DWORD)(now-(DWORD)menu_time)<=400u)") < SRC.index("InterlockedCompareExchange(&real_main_menu_seen,0,0)==0")
