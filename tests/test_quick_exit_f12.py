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
                      "0x0048D880u"):
        assert forbidden not in SRC


def test_foreground_key_detection_and_movie_to_native_quit_handoff():
    assert "GetAsyncKeyState(VK_F12)&0x8000" in SRC
    assert "GetForegroundWindow()==hwnd" in SRC
    assert "previously_down=TRUE;" in SRC
    assert "Sleep(25);" in SRC
    assert "REAL Shell_MainMenu or startup movie not active; F12 safely ignored" in SRC
    assert "intro movie exit requested; native Bink cleanup then top-level engine Quit" in SRC
    assert "F12 queued for REAL Shell_MainMenu" in SRC
    assert "InterlockedExchange(&pending,1);" in SRC
    assert "InterlockedExchange(&pending,0);" in SRC
    assert "native menu frame not reached within 800ms; no exit" in SRC
    assert "SpiderQuickExitF12_AttachWindow(hwnd);" in CORE
    assert "SpiderQuickExitF12_Init(log_line);" in CORE
    assert "SpiderQuickExitF12_ObserveMovieFrame();" in CORE
    assert "SpiderQuickExitF12_Enabled()" in CORE
    # Only game-owned code at the post-intro transition writes this
    # native Quit flag. The F12 polling thread never directly requests
    # engine shutdown from an arbitrary graphics/audio state.
    begin=SRC.index("static DWORD WINAPI f12_input_thread")
    end=SRC.index("void SpiderQuickExitF12_Init",begin)
    assert "*SPIDER_GAME_QUIT_FLAG=" not in SRC[begin:end]
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


def test_intro_exit_latch_survives_four_movies_into_native_top_level_quit_check():
    assert "static volatile LONG intro_quit_pending=0;" in SRC
    assert "InterlockedCompareExchange(&intro_quit_pending,1,0)==0" in SRC
    assert "InterlockedCompareExchange(&intro_quit_pending,0,0)==1" in SRC
    assert "InterlockedCompareExchange(&intro_quit_pending,0,1)==1" not in SRC
    assert "static void __cdecl spider_after_intro_cleanup(int mode)" in SRC
    assert "SPIDER_AFTER_INTRO_CLEANUP(mode);" in SRC
    assert "*SPIDER_GAME_QUIT_FLAG=1u;" in SRC
    assert "engine Quit flag set BEFORE top-level 0x455E50 check" in SRC
    assert "0x00455E50u" in SRC
    assert "0x00455D23u" in SRC
    assert "movl $3, 0x68(%esp)" in SRC
    assert "movl $0x004941D5, 0x24(%esp)" in SRC
    assert "*SPIDER_GAME_BINK_ACTIVE!=0u" in SRC
    assert "movie_time)<750u" in SRC


def test_intro_calls_are_signature_checked_and_install_rollbacks_complete():
    assert "memcmp(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1,5)" in SRC
    assert "memcmp(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2,5)" in SRC
    assert "if(!patch_native_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1," in SRC
    assert "if(!patch_native_call(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2," in SRC
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


def test_intro_transition_runs_original_cleanup_before_setting_native_quit():
    # Original x86:
    # 00455D22 PUSH EBX
    # 00455D23 CALL 0x443AD0
    # 00455D28 ADD ESP,4
    # ... 00455E50 CMP BYTE [0x60CF88],BL; JNE 0x4561F2.
    assert "0xE8,0xA8,0xDD,0xFE,0xFF" in SRC
    assert "0x38,0x1D,0x88,0xCF,0x60,0x00" in SRC
    assert "memcmp(SPIDER_AFTER_INTRO_CLEANUP_CALL,post_intro_cleanup_original,5)" in SRC
    assert "memcmp(SPIDER_TOPLEVEL_NATIVE_QUIT_TEST," in SRC
    begin=SRC.index("static void __cdecl spider_after_intro_cleanup(int mode)")
    next_fn=SRC.index("/* Returns nonzero ONLY",begin)
    checkpoint=SRC[begin:next_fn]
    assert checkpoint.index("SPIDER_AFTER_INTRO_CLEANUP(mode);") < checkpoint.index("*SPIDER_GAME_QUIT_FLAG=1u;")
    assert "if(!install_post_intro_checkpoint())" in SRC
    assert "restore_post_intro_checkpoint();" in SRC
    assert "intro_quit_pending" in checkpoint
    assert "intro_quit_pending,0,1" not in checkpoint
    assert "InterlockedExchange(&intro_native_quit_forwarded,1);" in checkpoint


def test_intro_exit_is_not_consumed_on_early_shell_main_menu_splash():
    cb=SRC[SRC.index("int __cdecl SpiderQuickExitF12_ConsumeRealMenuRequest(void)"):
           SRC.index("/* x86 stack at entry")]
    assert "InterlockedCompareExchange(&intro_quit_pending,0,1)" not in cb
    assert "*SPIDER_GAME_QUIT_FLAG=" not in cb
    assert "InterlockedCompareExchange(&pending,0,1)==1" in cb
    assert "native Shell_MainMenu Quit activated: slot=3 action=18" in cb


def test_engine_phase_tracer_is_passive_throttled_and_gated():
    # No new EXE-byte patch or standalone worker is allowed for stage discovery.
    # The preexisting DirectDraw Flip hook owns these read-only snapshots.
    assert "SpiderQuickExitF12_ObserveEngineState(count,InterlockedCompareExchange(&menu_upscale_active,0,0)==1);" in CORE
    assert "void SpiderQuickExitF12_ObserveEngineState(LONG flip_index, LONG renderer_menu);" in HDR
    assert "0x0060CFA4u" in SRC  # top-level switch read at 0x455F24
    assert "0x006B4C9Cu" in SRC  # native async transition
    assert "0x0054A510u" in SRC  # native mode task
    assert 'if(!SpiderQuickExitF12_Enabled())return;' in SRC
    assert "ENGINE-STATE flip=%ld renderer2d=%ld mode_request=%lu countdown=%lu countdown_active=%ld task=%lu quit=%ld" in SRC
    assert "previous_async_active==async_active" in SRC
    assert "previous_renderer_menu==renderer_menu" in SRC
    assert "previous_pause_context==pause_context" in SRC
    assert "previous_pause_options==pause_options" in SRC
    assert "previous_pause_restart==pause_restart" in SRC
    assert "0x00682950u" in SRC
    assert "0x00682968u" in SRC
    assert "0x00682960u" in SRC
    assert "pause_ctx=%ld pause_options=%ld pause_restart=%ld" in SRC
    assert "(*SPIDER_PAUSE_CONTEXT!=0u)" in SRC
    assert "(*SPIDER_PAUSE_OPTION_MENU!=0u)" in SRC
    assert "(*SPIDER_PAUSE_RESTART_MENU!=0u)" in SRC

    assert "menu_recent=" in SRC and "movie_recent=" in SRC
    assert "(DWORD)(now-last_snapshot)<5000u" in SRC
    observer=SRC[SRC.index("void SpiderQuickExitF12_ObserveEngineState(LONG flip_index,"):
                 SRC.index("/* Native BinkFrame tick wrapper")]
    for forbidden in ("VirtualProtect(", "PostMessageA(", "DestroyWindow(",
                      "*SPIDER_GAME_QUIT_FLAG=", "*SPIDER_ENGINE_MODE=",
                      "*SPIDER_ENGINE_ASYNC_STATE=", "TerminateProcess(",
                      "ExitProcess(", "SetWindowLongPtr("):
        assert forbidden not in observer


def test_three_native_3d_pause_actions_are_documented_but_not_mutated():
    # These native option slots are the literal strings from SpideyPC.exe
    # reachable from the game-owned pause UI, not a synthetic selection.
    # The experiment only reports pointer presence; it never alters
    # selected-item memory, request 7, or the owning native return path.
    assert "0 retry; 1 quit to training; 2 quit to main menu." in SRC
    observer=SRC[SRC.index("void SpiderQuickExitF12_ObserveEngineState(LONG flip_index,"):
                 SRC.index("/* Native BinkFrame tick wrapper")]
    assert "*SPIDER_PAUSE_CONTEXT=" not in observer
    assert "*SPIDER_PAUSE_OPTION_MENU=" not in observer
    assert "*SPIDER_PAUSE_RESTART_MENU=" not in observer
    assert "0x0047BC40u" not in SRC # no unauthorized direct native cleanup call


def test_native_game_pause_handler_probe_calls_original_once_without_mutation():
    # 00455540 MOV EAX,[0x682950]; TEST EAX,EAX; JE 0x45554E
    # 00455549 CALL 0x47BC40 (bytes E8 F2 66 02 00)
    # Game code itself decides whether this handler executes.
    assert "#define SPIDER_NATIVE_PAUSE_CALL ((BYTE *)(uintptr_t)0x00455549u)" in SRC
    assert "#define SPIDER_NATIVE_PAUSE_HANDLER ((void (__cdecl *)(void))(uintptr_t)0x0047BC40u)" in SRC
    assert "native_pause_call_original[5]={0xE8,0xF2,0x66,0x02,0x00}" in SRC
    assert "memcmp(SPIDER_NATIVE_PAUSE_CALL,native_pause_call_original,5)!=0" in SRC
    assert "if(!install_passive_pause_probe())" in SRC
    assert "restore_passive_pause_probe();" in SRC
    start=SRC.index("static void __cdecl spider_observe_native_pause_handler(void)")
    end=SRC.index("/* Native BinkFrame tick wrapper",start)
    observer=SRC[start:end]
    assert observer.count("SPIDER_NATIVE_PAUSE_HANDLER();")==1
    assert "NATIVE-PAUSE owner ENTER" in observer
    assert "NATIVE-PAUSE owner EXIT" in observer
    assert "native request 7 from pause" in observer
    assert "if(after_mode==7u && before_mode!=7u)" in observer
    for forbidden in ("VirtualProtect(", "ExitProcess(", "TerminateProcess(",
                      "PostMessageA(", "DestroyWindow(", "SendInput(",
                      "*SPIDER_PAUSE_CONTEXT=", "*SPIDER_PAUSE_OPTION_MENU=",
                      "*SPIDER_ENGINE_MODE=", "SPIDER_NATIVE_PAUSE_HANDLER(1"):
        assert forbidden not in observer
