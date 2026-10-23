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



def test_comic_render_candidate_observation_is_passive_and_owner_agnostic():
    # A 512x256 native comic-like quad is a rendering marker, NOT the
    # owner of the underlying game scene or permission to quit.
    assert "void SpiderQuickExitF12_ObserveComicRender(" in HDR
    assert "void SpiderQuickExitF12_ObserveComicRender(" in SRC
    start=SRC.index("void SpiderQuickExitF12_ObserveComicRender(")
    end=SRC.index("/* Read-only stage discovery",start)
    cb=SRC[start:end]
    assert "SpiderQuickExitF12_Enabled()" in cb
    assert "COMIC-RENDER-CANDIDATE" in cb
    assert "[RENDER-ONLY; comic owner UNKNOWN]" in cb
    assert "native_3d_owner_calls" in cb
    assert "InterlockedCompareExchange(&input_native_invocations,0,0)" in cb
    assert "n>8 && n%120!=0" in cb
    for forbidden in ("*SPIDER_ENGINE_MODE=", "*SPIDER_GAME_QUIT_FLAG=",
                      "game3d_exit_state,1,0", "TerminateProcess(",
                      "ExitProcess(", "DestroyWindow(", "PostMessageA(",
                      "SPIDER_NATIVE_GAME_CLEANUP();", "BinkClose("):
        assert forbidden not in cb
    start=CORE.index("static const void *maybe_correct_native_comic")
    end=CORE.index("static const void *maybe_correct_horplus",start)
    drawing=CORE[start:end]
    assert "SpiderComicNative_IsCandidate" in drawing
    assert "SpiderQuickExitF12_ObserveComicRender(" in drawing
    assert drawing.index("SpiderComicNative_IsCandidate") < drawing.index(
        "SpiderQuickExitF12_ObserveComicRender(")
    assert drawing.index("SpiderQuickExitF12_ObserveComicRender(") < drawing.index(
        "SpiderComicNative_Transform")
    assert drawing.count("SpiderQuickExitF12_ObserveComicRender(")==1
    assert "original_draw_primitive" not in drawing



def test_post_new_game_media_diagnostics_are_passive_and_bink_ownership_unproven():
    # Reading the game's known handle slots is not equivalent to playback
    # detection: the video could have another owner/presentation path.
    assert "#define SPIDER_BINK_VIDEO_HANDLE ((const volatile DWORD *)(uintptr_t)0x00AC0BA4u)" in SRC
    assert "#define SPIDER_BINK_AUDIO_HANDLE ((const volatile DWORD *)(uintptr_t)0x00AC1020u)" in SRC
    assert "static volatile LONG last_comic_candidate_ms=0;" in SRC
    comic=SRC[SRC.index("void SpiderQuickExitF12_ObserveComicRender("):
              SRC.index("/* Read-only stage discovery")]
    assert "InterlockedExchange(&last_comic_candidate_ms,(LONG)now)" in comic

    media=SRC[SRC.index("void SpiderQuickExitF12_ObserveMovieFrame(void)"):
              SRC.index("/* Passive comic-quad observation")]
    assert "POST-SHELL-MEDIA-BLT" in media
    assert "[PRESENTATION-ONLY]" in media
    assert "n<=4 || n%120==0" in media
    assert "InterlockedExchange(&last_movie_frame_ms,(LONG)now)" in media
    assert "SpiderQuickExitF12_Enabled()" in media
    assert "real_main_menu_seen,0,0)!=1" in media

    stage=SRC[SRC.index("void SpiderQuickExitF12_ObserveEngineState("):
              SRC.index("/* Diagnostic wrapper for the EXACT ORIGINAL pause-owner call")]
    assert "previous_bink_flag==bink_flag" in stage
    assert "previous_video_handle==video_handle" in stage
    assert "previous_native_owner_seen==native_owner_seen" in stage
    assert "bink_flag=%ld video_handle_present=%ld" in stage
    assert "native_3d_owner_seen=%ld" in stage
    assert "(*SPIDER_BINK_VIDEO_HANDLE!=0u)" in stage
    assert "(*SPIDER_BINK_AUDIO_HANDLE!=0u)" in stage
    assert "(*SPIDER_GAME_BINK_ACTIVE!=0u)" in stage

    tick=SRC[SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"):
             SRC.index("/* This wrapper executes ON THE ORIGINAL GAME THREAD")]
    assert "POST-SHELL-BINK-TICK-CALLSITE" in tick
    assert "post_shell=SpiderQuickExitF12_Enabled() &&" in tick
    assert "real_main_menu_seen,0,0)==1" in tick
    assert "n=InterlockedIncrement(&after_shell_ticks);" in tick
    assert "if(n==1)" in tick
    assert tick.count("SPIDER_NATIVE_BINK_FRAME_TICK();")==1
    assert "native_result=SPIDER_NATIVE_BINK_FRAME_TICK();" in tick
    assert "return native_result;" in tick
    assert tick.index("POST-SHELL-BINK-TICK-CALLSITE") < tick.index("native_result=SPIDER_NATIVE_BINK_FRAME_TICK();")
    # Never reinterpret the diagnostics as permission to quit comics/movies.
    for observer in (media,stage,tick):
        for forbidden in ("TerminateProcess(", "ExitProcess(", "DestroyWindow(",
                          "PostMessageA(", "BinkClose(", "*SPIDER_ENGINE_MODE=",
                          "*SPIDER_BINK_VIDEO_HANDLE=", "*SPIDER_BINK_AUDIO_HANDLE="):
            assert forbidden not in observer



def test_post_shell_space_skip_probe_preserves_original_bink_result():
    tick=SRC[SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"):
             SRC.index("/* This wrapper executes ON THE ORIGINAL GAME THREAD")]
    assert tick.count("native_result=SPIDER_NATIVE_BINK_FRAME_TICK();")==1
    assert "POST-SHELL-BINK-TICK-RESULT" in tick
    assert "space_down_sampled=%ld" in tick
    assert "[READ-ONLY; SKIP-REASON UNKNOWN]" in tick
    assert "native_result!=(BYTE)old_result" in tick
    assert "(space_down && !old_space)" in tick
    assert "n<=4 || n%128==0 || native_result!=(BYTE)old_result" in tick
    assert "(DWORD)(now-after_shell_last_log_ms)>=1000u" in tick
    assert "static DWORD after_shell_last_log_ms=0;" in tick
    assert "n%120==0" not in tick
    assert "GetAsyncKeyState(VK_SPACE)&0x8000" in tick
    assert "return native_result;" in tick
    assert "intro_quit_pending,0,0)==1" in tick
    assert tick.index("intro_quit_pending,0,0)==1") < tick.index(
        "native_result=SPIDER_NATIVE_BINK_FRAME_TICK();")
    for forbidden in ("SPIDER_NATIVE_BINK_FRAME_TICK(1)", "BinkClose(",
                      "TerminateProcess(", "ExitProcess(", "PostMessageA(",
                      "*SPIDER_GAME_QUIT_FLAG=", "*SPIDER_ENGINE_MODE=",
                      "*SPIDER_BINK_VIDEO_HANDLE=", "DestroyWindow("):
        assert forbidden not in tick




def test_native_bink_owner_static_inspection_is_read_only_and_sha_gated():
    # The game EXE is deliberately NOT included in this repository/CI.
    # This locally-run Windows PowerShell tool produces a small hex report;
    # any candidate native instruction/branch still requires disassembly.
    script=(ROOT/"tools/Inspect-BinkNativeCode.ps1").read_text(encoding="utf-8")
    assert "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea" in script
    assert "$expectedLength = 1507328" in script
    assert '[IO.File]::ReadAllBytes($exe)' in script
    assert '$hash -ne $expectedHash' in script
    assert 'Ensure-Span' in script and 'Resolve-Address' in script
    assert '0x00470700 0x350' in script
    assert '0x00455C80 0x200' in script
    assert '$targetBinkOwner = [long]0x00470750' in script
    assert '0xE8' in script
    assert "Each candidate needs instruction-boundary verification" in script
    assert "Do not publish raw game code in the public GitHub repository." in script
    assert '[IO.File]::WriteAllLines($destination,$lines,[Text.Encoding]::UTF8)' in script
    for bad in ('WriteAllBytes(', 'Set-Content -LiteralPath $exe',
                'VirtualProtect(', 'Start-Process', 'Stop-Process',
                'Invoke-Expression', 'Add-Type', 'GetAsyncKeyState',
                'Set-ItemProperty', 'Patch-'):
        assert bad not in script



def test_opt_in_parent_trace_reads_only_exact_verified_native_stack_frame():
    tick=SRC[SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"):
             SRC.index("/* This wrapper executes ON THE ORIGINAL GAME THREAD")]
    parent=SRC[SRC.index("static int spider_f12_known_bink_parent_return(DWORD ret_addr)"):
               SRC.index("/* Native BinkFrame tick wrapper")]
    init=SRC[SRC.index("void SpiderQuickExitF12_Init(void (*log_fn)(const char *))"):]
    assert "SPIDERMAN2K1FIX_F12_PARENT_TRACE" in init
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in init
    assert "static volatile LONG parent_trace_enabled=0;" in SRC
    assert "if(InterlockedCompareExchange(&parent_trace_enabled,0,0)==1)" in tick
    assert "spider_f12_trace_native_bink_parent(" in tick
    assert "(const uintptr_t *)__builtin_frame_address(0)" in tick
    assert "if(n==1)" in tick
    assert "tick_return!=0x00470822u && tick_return!=0x00470864u" in parent
    assert parent.index("if(tick_return!=") < parent.index("parent_return=(DWORD)frame[4]")
    assert "parent_return=(DWORD)frame[4];" in parent
    assert "movie_id=(DWORD)frame[5];" in parent
    assert "allow_skip=(DWORD)frame[6];" in parent
    assert "POST-SHELL-BINK-PARENT" in parent
    assert "known_original_site=%d" in parent
    assert "[FRAME-READ-ONLY; NO EXIT AUTHORITY]" in parent
    # Exact list of 30 direct call-site return addresses (VA + 5).
    known=parent.split("static const DWORD known[]={",1)[1].split("};",1)[0]
    import re
    values=re.findall(r"0x[0-9A-F]{8}u",known)
    assert len(values)==30
    assert len(set(values))==30
    for expected in ("0x00443BECu","0x0049D659u",
                     "0x00455CF6u","0x00455D03u","0x00455D11u",
                     "0x00455D1Fu","0x00427A00u","0x00427D8Fu"):
        assert expected in known
    for forbidden in ("ExitProcess(", "TerminateProcess(", "BinkClose(",
                      "PostMessageA(", "DestroyWindow(", "VirtualProtect(",
                      "*SPIDER_GAME_QUIT_FLAG=", "*SPIDER_ENGINE_MODE="):
        assert forbidden not in parent
    assert tick.count("native_result=SPIDER_NATIVE_BINK_FRAME_TICK();")==1
    assert "return native_result;" in tick



def test_default_off_and_ini_section_validated():
    assert "QuickExitF12=0" in INI
    assert '"[System]\\r\\n"' in GEN
    assert '"QuickExitF12=0\\r\\n"' in GEN
    assert '"Debug","System","Display"' in VAL
    assert 'spider_ini_system[]={"QuickExitF12","ExperimentalQuickExitF12In3D"}' in VAL
    assert "ExperimentalQuickExitF12In3D=0" in INI
    assert '"ExperimentalQuickExitF12In3D=0\\r\\n"' in GEN
    assert 'GetPrivateProfileIntA("System","ExperimentalQuickExitF12In3D",0,ini)' in SRC
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
    assert "REAL Shell_MainMenu, startup movie or recognized 3D owner not active; F12 safely ignored" in SRC
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


def test_external_windows_watcher_verifies_direct_3d_f12_cleanup_and_expected_mode():
    script=(ROOT/"tools/Verify-F12QuickExit.ps1").read_text(encoding="utf-8")
    assert 'ValidateSet("Any","Menu","3D","Intro")' in script
    assert '$ExpectedMode = "Any"' in script
    assert "3D game-thread frame completed; native request 7 queued" in script
    assert "NATIVE-3D-CLEANUP completed" in script
    assert "CONTROLLER-7 native teardown COMPLETED" in script
    assert "bit4=1 .*outcome=NATIVE-FINAL-CLEANUP" in script
    assert "$game3dIntent -and $game3dCleanup -and $game3dFinal" in script
    assert '$exitPath = "3D"' in script
    assert '$ExpectedMode -ne $exitPath' in script
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
    assert "native_result=SPIDER_NATIVE_BINK_FRAME_TICK();" in SRC
    assert "return native_result;" in SRC
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
    assert "SPIDER_NATIVE_PAUSE_HANDLER();" in SRC  # original owner called once by passive wrapper
    assert "0x0047BC40u" in SRC  # only native pause owner, never a destructor


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


def test_native_game_input_handler_observer_covers_alternative_request_7_writer():
    # Unlike pause handler 0x47BC40, the 0x440EF0 routine executes
    # immediately before the game's pause-context check at 0x455540.
    # 0x44108A can request transition 7 from engine flags + timer.
    assert "#define SPIDER_NATIVE_GAME_INPUT_CALL ((BYTE *)(uintptr_t)0x0045553Bu)" in SRC
    assert "#define SPIDER_NATIVE_GAME_INPUT_UPDATE ((void (__cdecl *)(void))(uintptr_t)0x00440EF0u)" in SRC
    assert "native_game_input_call_original[5]={0xE8,0xB0,0xB9,0xFE,0xFF}" in SRC
    assert "memcmp(SPIDER_NATIVE_GAME_INPUT_CALL,native_game_input_call_original,5)!=0" in SRC
    assert "if(!install_passive_game_input_probe())" in SRC
    assert "restore_passive_game_input_probe();" in SRC
    start=SRC.index("static void __cdecl spider_observe_native_game_input(void)")
    end=SRC.index("/* Native BinkFrame tick wrapper",start)
    observer=SRC[start:end]
    assert observer.count("SPIDER_NATIVE_GAME_INPUT_UPDATE();")==1
    assert "NATIVE-GAME-INPUT owner ENTER" in observer
    assert "NATIVE REQUEST 7 observed after 0x440EF0 (internal writer unconfirmed)" in observer
    assert "if(native_after_request==7u && before_request!=7u)" in observer
    assert "SPIDER_NATIVE_GAME_INPUT_TIMER_ACTIVE" in observer
    assert "SPIDER_NATIVE_GAME_INPUT_FLAG_1" in observer
    assert "SPIDER_NATIVE_GAME_INPUT_FLAG_2" in observer
    for forbidden in ("VirtualProtect(", "ExitProcess(", "TerminateProcess(",
                      "PostMessageA(", "DestroyWindow(", "SendInput(",
                      "*SPIDER_NATIVE_GAME_INPUT_TIMER_ACTIVE=",
                      "*SPIDER_NATIVE_GAME_INPUT_FLAG_1=",
                      "*SPIDER_NATIVE_GAME_INPUT_FLAG_2=",
                      "*SPIDER_ENGINE_MODE=", "*SPIDER_GAME_QUIT_FLAG="):
        assert forbidden not in observer


def test_native_gameplay_controller_cleanup_is_instrumented_without_mutation():
    # 455A8B calls the original game-owned frame 455400; immediately at
    # 455A90, mode_request != 0 skips to 455BF3 for owner cleanup.
    # 455BF9 CALL 4302D0 is part of that original game-owned cleanup.
    assert "#define SPIDER_NATIVE_GAME_CLEANUP_CALL ((BYTE *)(uintptr_t)0x00455BF9u)" in SRC
    assert "#define SPIDER_NATIVE_GAME_CLEANUP ((void (__cdecl *)(void))(uintptr_t)0x004302D0u)" in SRC
    assert "native_game_cleanup_call_original[5]={0xE8,0xD2,0xA6,0xFD,0xFF}" in SRC
    assert "memcmp(SPIDER_NATIVE_GAME_CLEANUP_CALL,native_game_cleanup_call_original,5)!=0" in SRC
    assert "if(!install_passive_game_cleanup_probe())" in SRC
    assert "restore_passive_game_cleanup_probe();" in SRC
    start=SRC.index("static void __cdecl spider_observe_native_game_cleanup(void)")
    end=SRC.index("/* Native BinkFrame tick wrapper",start)
    observer=SRC[start:end]
    assert observer.count("SPIDER_NATIVE_GAME_CLEANUP();")==1
    assert "NATIVE-3D-CLEANUP begin" in observer
    assert "NATIVE-3D-CLEANUP completed" in observer
    for forbidden in ("VirtualProtect(", "TerminateProcess(", "ExitProcess(",
                      "DestroyWindow(", "SendInput(", "PostMessageA(",
                      "*SPIDER_ENGINE_MODE=", "*SPIDER_GAME_QUIT_FLAG="):
        assert forbidden not in observer


def test_native_3d_transition_7_condition_tracer_is_read_only():
    assert "#define SPIDER_NATIVE_GAME_TICK_COUNTER ((const volatile DWORD *)(uintptr_t)0x006B4CA0u)" in SRC
    assert "#define SPIDER_NATIVE_GAME_INPUT_TIMER_START ((const volatile DWORD *)(uintptr_t)0x005FAEDCu)" in SRC
    begin=SRC.index("static void __cdecl spider_observe_native_game_input(void)")
    end=SRC.index("/* PASSIVE owner cleanup checkpoint",begin)
    observer=SRC[begin:end]
    assert "SPIDER_NATIVE_GAME_INPUT_UPDATE();" in observer
    assert "timer_before=%lu timer_after=%lu" in observer
    assert "flag_before=%u,%u flag_after=%u,%u" in observer
    assert "NATIVE-GAME-INPUT flags" in observer
    assert "current_armed" in observer
    assert "elapsed_ticks=" in observer
    assert "*SPIDER_NATIVE_GAME_INPUT_TIMER_START=" not in observer
    assert "*SPIDER_NATIVE_GAME_TICK_COUNTER=" not in observer


def test_top_level_native_controller7_exit_decision_is_passively_observed():
    # Original x86 456198 CALL 48A820, then unmodified 45619D:
    # TEST BYTE [2E0988C],4; JNE 45620C
    # CMP DWORD [568FB0],0; JGE 45620C; otherwise JMP 455D98.
    assert "0x00456198u" in SRC
    assert "0x0048A820u" in SRC
    assert "0x02E0988Cu" in SRC
    assert "0x00568FB0u" in SRC
    assert "native_controller7_finish_call_original[5]={0xE8,0x83,0x46,0x03,0x00}" in SRC
    assert "memcmp(SPIDER_NATIVE_CONTROLLER7_FINISH_CALL,native_controller7_finish_call_original,5)!=0" in SRC
    assert "if(!install_passive_controller7_probe())" in SRC
    assert "restore_passive_controller7_probe();" in SRC
    start=SRC.index("static void __cdecl spider_observe_controller7_finish(void)")
    end=SRC.index("/* Native BinkFrame tick wrapper",start)
    observer=SRC[start:end]
    assert observer.count("SPIDER_NATIVE_CONTROLLER7_FINISH();")==1
    assert "CONTROLLER-7 native teardown ENTER" in observer
    assert "CONTROLLER-7 native teardown COMPLETED" in observer
    assert "NATIVE-FINAL-CLEANUP" in observer
    assert "RETURN-TO-CONTROLLER" in observer
    assert "after_status>=0" in observer
    assert "(after_bit&4u)!=0u" in observer
    for forbidden in ("VirtualProtect(", "TerminateProcess(", "ExitProcess(",
                      "DestroyWindow(", "SendInput(", "PostMessageA(",
                      "*SPIDER_NATIVE_CONTROLLER7_EXIT_FLAGS=",
                      "*SPIDER_NATIVE_CONTROLLER7_EXIT_STATUS=",
                      "*SPIDER_ENGINE_MODE=", "*SPIDER_GAME_QUIT_FLAG="):
        assert forbidden not in observer


def test_3d_f12_is_separately_opted_in_and_menu_keeps_priority():
    assert "static volatile LONG experimental_game3d_enabled=0;" in SRC
    assert "GetPrivateProfileIntA(\"System\",\"ExperimentalQuickExitF12In3D\",0,ini)" in SRC
    assert "QuickExitF12=0" in INI
    assert "ExperimentalQuickExitF12In3D=0" in INI
    assert 'if(strcmp(val,"1")!=0)' in SRC
    assert "game3d_exit_state,1,0" in SRC
    assert "last_native_game_input_ms" in SRC
    assert "last_game3d_flip_ms" in SRC
    assert "last_menu_frame_ms" in SRC
    thread=SRC[SRC.index("static DWORD WINAPI f12_input_thread"):
               SRC.index("void SpiderQuickExitF12_Init")]
    assert thread.index("if(menu_time &&") < thread.index("experimental_game3d_enabled")
    assert thread.index("intro_quit_pending,1,0") < thread.index("game3d_exit_state,1,0")
    assert "*(volatile DWORD *)(uintptr_t)0x0060CFA4u=7u;" not in thread
    assert "*SPIDER_GAME_QUIT_FLAG=" not in thread
    assert "0x0060CFA4u=7u" not in thread
    assert "0x02E0988Cu=" not in thread
    assert "TerminateProcess(" not in thread
    assert "PostMessageA(" not in thread



def test_3d_native_request_logging_excludes_post_call_f12_injection():
    start=SRC.index("static void __cdecl spider_observe_native_game_input(void)")
    end=SRC.index("/* PASSIVE owner cleanup checkpoint",start)
    cb=SRC[start:end]
    native_snapshot="native_after_request=after_request;"
    f12_write="*(volatile DWORD *)(uintptr_t)0x0060CFA4u=7u;"
    native_condition="if(native_after_request==7u && before_request!=7u)"
    assert cb.count("SPIDER_NATIVE_GAME_INPUT_UPDATE();")==1
    assert cb.index("SPIDER_NATIVE_GAME_INPUT_UPDATE();") < cb.index(native_snapshot)
    assert cb.index(native_snapshot) < cb.index(f12_write)
    assert cb.index(f12_write) < cb.index(native_condition)
    assert "if(after_request==7u && before_request!=7u)" not in cb
    assert cb.index(native_condition) < cb.index("InterlockedIncrement(&input_native_requested7)")
    assert "3D game-thread frame completed; native request 7 queued" in cb
    assert "NATIVE REQUEST 7 observed after 0x440EF0 (internal writer unconfirmed)" in cb
    # The F12-injected request must never increment the native-owner counter.


def test_3d_request_7_issued_only_on_native_owner_thread_after_input_tick():
    start=SRC.index("static void __cdecl spider_observe_native_game_input(void)")
    end=SRC.index("/* PASSIVE owner cleanup checkpoint",start)
    cb=SRC[start:end]
    assert cb.count("SPIDER_NATIVE_GAME_INPUT_UPDATE();")==1
    assert "InterlockedCompareExchange(&game3d_exit_state,2,1)==1" in cb
    assert "experimental_game3d_enabled" in cb
    assert "before_request==0u && after_request==0u" in cb
    assert "*SPIDER_GAME_BINK_ACTIVE==0u" in cb
    assert "*(volatile DWORD *)(uintptr_t)0x0060CFA4u=7u;" in cb
    assert cb.index("SPIDER_NATIVE_GAME_INPUT_UPDATE();") < cb.index("*(volatile DWORD *)(uintptr_t)0x0060CFA4u=7u;")
    assert "3D game-thread frame completed; native request 7 queued for original 0x455BF3 cleanup" in cb
    assert "game3d_cleanup_baseline" in cb
    assert "game3d_cleanup_confirmed" in cb
    assert "ExitProcess(" not in cb and "TerminateProcess(" not in cb


def test_3d_final_exit_arm_is_only_after_native_owner_and_controller_cleanup():
    start=SRC.index("static void __cdecl spider_observe_controller7_finish(void)")
    end=SRC.index("/* Native BinkFrame tick wrapper",start)
    cb=SRC[start:end]
    assert cb.count("SPIDER_NATIVE_CONTROLLER7_FINISH();")==1
    assert "request==7u" in cb
    assert "game3d_cleanup_confirmed,0,0)==1" in cb
    assert "game3d_exit_state,0,0)==2" in cb
    assert "native_game_cleanup_count,0,0)>" in cb
    assert "after_status<0" in cb
    assert "(after_bit&4u)==0u" in cb
    assert "*(volatile BYTE *)(uintptr_t)0x02E0988Cu=(BYTE)(after_bit|4u);" in cb
    assert cb.index("SPIDER_NATIVE_CONTROLLER7_FINISH();") < cb.index("*(volatile BYTE *)(uintptr_t)0x02E0988Cu=(BYTE)(after_bit|4u);")
    assert "3D owner+controller cleanup complete; engine native final-exit flag set at 0x45619D" in cb
    assert "InterlockedExchange(&game3d_exit_state,3)" in cb
    assert "0x0045620Cu" not in cb  # never directly jump or call native teardown
    assert "ExitProcess(" not in cb and "TerminateProcess(" not in cb


def test_3d_original_game_owned_cleanup_return_marks_checkpoint():
    start=SRC.index("static void __cdecl spider_observe_native_game_cleanup(void)")
    end=SRC.index("/* Native BinkFrame tick wrapper",start)
    cb=SRC[start:end]
    assert "SPIDER_NATIVE_GAME_CLEANUP();" in cb
    assert "game3d_exit_state,0,0)==2" in cb
    assert "request==7u" in cb
    assert "game3d_cleanup_confirmed,1" in cb
    assert cb.index("SPIDER_NATIVE_GAME_CLEANUP();") < cb.index("game3d_cleanup_confirmed,1")
