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




def test_startup_static_screen_probe_is_extra_opt_in_bounded_and_read_only():
    """Two intro stills share game menu infrastructure; don't guess owner."""
    assert "SPIDERMAN2K1FIX_F12_STATIC_TRACE" in SRC
    assert "static volatile LONG static_screen_trace_enabled=0;" in SRC
    assert "static volatile LONG post_intro_cleanup_seen=0;" in SRC
    assert "static_trace[0]=='1'" in SRC
    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in init
    assert 'strcmp(val,"1")==0' in init
    assert "STARTUP-STATIC-PROBE opt-in enabled" in init

    observer=SRC[SRC.index("void SpiderQuickExitF12_ObserveEngineState("):
                 SRC.index("/* Diagnostic wrapper for the EXACT ORIGINAL pause-owner call")]
    assert "static_screen_trace_enabled,0,0)==1" in observer
    assert "STARTUP-STATIC-PROBE flip=%ld" in observer
    assert "[READ-ONLY; SCREEN ID UNKNOWN]" in observer
    assert "(DWORD)(now-start_ms)<=90000u" in observer
    assert "samples<61" in observer
    assert "(DWORD)(now-last_ms)>=1500u" in observer
    assert "menu_ctx_present=%lu" in observer
    assert "menu_stage_byte=%lu" in observer
    assert "dispatcher_raw=%lu" in observer
    assert "transition_raw=%lu" in observer
    assert "real_menu_tick_count=%ld" in observer
    assert "still_frame_count=%lu" in observer
    assert "timed_page_once=%lu" in observer
    assert "native_clock_raw=%lu" in observer
    assert "#define SPIDER_TRACE_STILL_FRAME_COUNT ((const volatile DWORD *)(uintptr_t)0x0060CFA8u)" in SRC
    assert "#define SPIDER_TRACE_TIMED_PAGE_ONCE ((const volatile BYTE *)(uintptr_t)0x006A7785u)" in SRC
    assert "#define SPIDER_TRACE_NATIVE_CLOCK_RAW ((const volatile DWORD *)(uintptr_t)0x006B4CA0u)" in SRC
    for scalar in ("SPIDER_TRACE_STILL_FRAME_COUNT", "SPIDER_TRACE_TIMED_PAGE_ONCE",
                   "SPIDER_TRACE_NATIVE_CLOCK_RAW"):
        assert f"*{scalar}" in observer
        assert f"*{scalar}=" not in observer
    for name in ("SPIDER_TRACE_MENU_CONTEXT", "SPIDER_TRACE_MENU_STAGE_BYTE",
                 "SPIDER_TRACE_DISPATCH_RAW", "SPIDER_TRACE_TRANSITION_RAW"):
        assert name in observer
        assert f"*{name}=" not in observer
    for forbidden in ("ExitProcess(", "TerminateProcess(", "DestroyWindow(",
                      "PostMessageA(", "BinkClose(", "*SPIDER_ENGINE_MODE=",
                      "*SPIDER_GAME_QUIT_FLAG="):
        assert forbidden not in observer

    checkpoint=SRC[SRC.index("static void __cdecl spider_after_intro_cleanup(int mode)"):
                   SRC.index("/* Returns nonzero ONLY when a recent")]
    assert "SPIDER_AFTER_INTRO_CLEANUP(mode);" in checkpoint
    assert checkpoint.index("SPIDER_AFTER_INTRO_CLEANUP(mode);") < checkpoint.index(
        "post_intro_cleanup_seen,1,0")
    assert "STARTUP-STATIC-PROBE original four-movie transition cleanup" in checkpoint
    # Opt-in diagnostic is not an extra game binary instruction hook.
    assert "patch_native_call(" not in observer
    assert "install_real_menu_hook(" not in observer



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
             SRC.index("/* Wrapper on the ORIGINAL GAME THREAD")]
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
             SRC.index("/* Wrapper on the ORIGINAL GAME THREAD")]
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
             SRC.index("/* Wrapper on the ORIGINAL GAME THREAD")]
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
    assert "movie_id=(DWORD)frame[5];" not in parent
    assert "original first argument at frame[5] was already OVERWRITTEN" in parent
    assert "ID MUST COME FROM CALLSITE" in parent
    assert "allow_skip=(DWORD)frame[6];" in parent
    assert "POST-SHELL-BINK-PARENT" in parent
    assert "known_original_site=%d" in parent
    assert "[FRAME-READ-ONLY; ID MUST COME FROM CALLSITE; NO EXIT AUTHORITY]" in parent
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




def test_postmenu_movie_opt_in_is_off_by_default_and_verified_per_owner():
    assert "static volatile LONG postmovie_opt_in=0;" in SRC
    assert "static volatile LONG postmovie_state=0;" in SRC
    assert "static volatile LONG postmovie_request_ms=0;" in SRC
    assert "SPIDERMAN2K1FIX_F12_POSTMENU_MOVIE_EXIT" in SRC
    assert 'GetPrivateProfileIntA("System","ExperimentalQuickExitF12InNewGameMovie",0,ini)==1' in SRC
    assert "movie_trial[0]=='1'" in SRC
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in SRC
    assert "postmovie_opt_in,1" in SRC
    tick=SRC[SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"):
             SRC.index("/* Wrapper on the ORIGINAL GAME THREAD")]
    assert "POSTMENU-MOVIE verified native movie owner 0x4279FB" in tick
    assert "tick_return==0x00470822u || tick_return==0x00470864u" in tick
    assert "(DWORD)frame[4]==0x00427A00u" in tick
    assert "(DWORD)frame[6]==1u" in tick
    assert "postmovie_state,2,stage" in tick
    assert "SPIDER_BINK_VIDEO_HANDLE!=0u" in tick
    assert tick.count("native_result=SPIDER_NATIVE_BINK_FRAME_TICK();")==1
    assert "return native_result;" in tick
    # Scope the order check to the original native movie-exit guard:
    # F12TestMode additionally observes the exact movie-4 stack before
    # this block, and that read-only probe has its own signature check.
    verified=tick[tick.index("if(stage==1 || stage==4) {"):]
    assert verified.index("tick_return==") < verified.index("frame[4]") < verified.index("frame[6]")
    for forbidden in ("ExitProcess(", "TerminateProcess(", "PostMessageA(",
                      "DestroyWindow(", "BinkClose(", "SPIDER_NATIVE_BINK_FRAME_TICK(1)"):
        assert forbidden not in tick


def test_postmenu_movie_handoff_is_post_native_script_and_cleanup_gated():
    watcher=SRC[SRC.index("static DWORD WINAPI f12_input_thread("):
                SRC.index("void SpiderQuickExitF12_Init(")]
    assert "POSTMENU-MOVIE F12 intent requested" in watcher
    assert "input_native_invocations,0,0)==0" in watcher
    assert "last_comic_candidate_ms" in watcher
    assert watcher.index("InterlockedExchange(&postmovie_request_ms,(LONG)now);") < watcher.index(
        "InterlockedCompareExchange(&postmovie_state,1,0)")
    owner=SRC[SRC.index("static void __cdecl spider_observe_native_game_input(void)"):
              SRC.index("/* PASSIVE owner cleanup checkpoint")]
    assert "postmovie_state,0,0)==2" in owner
    assert "POSTMENU-MOVIE original Bink + script returned" in owner
    assert "SPIDER_GAME_BINK_ACTIVE==0u" in owner
    assert "SPIDER_BINK_VIDEO_HANDLE==0u" in owner
    assert "before_request==0u && after_request==0u" in owner
    assert "started && (DWORD)(GetTickCount()-started)<15000u" in owner
    assert "game3d_exit_state,2,0" in owner
    assert "game3d_cleanup_baseline" in owner
    assert "game3d_cleanup_confirmed,0" in owner
    assert "postmovie_state,3" in owner
    assert "*(volatile DWORD *)(uintptr_t)0x0060CFA4u=7u;" in owner
    assert "POSTMENU-MOVIE FINAL-TEARDOWN-CHECKPOINT elapsed_since_F12_ms=" in SRC
    assert "[NO PRESENT/COLOR CLAIM]" in SRC
    assert owner.count("SPIDER_NATIVE_GAME_INPUT_UPDATE();")==1
    assert owner.index("SPIDER_NATIVE_GAME_INPUT_UPDATE();") < owner.index(
        "POSTMENU-MOVIE original Bink + script returned")
    for forbidden in ("ExitProcess(", "TerminateProcess(", "PostMessageA(",
                      "DestroyWindow(", "BinkClose("):
        assert forbidden not in owner



def test_deferred_comic_f12_keeps_native_still_owner_and_then_uses_movie_four():
    """The pre-film still has no proven native owner: never cut its timer."""
    assert "static volatile LONG comic_exit_opt_in=0;" in SRC
    assert "SPIDERMAN2K1FIX_F12_COMIC_EXIT" in SRC
    assert 'GetPrivateProfileIntA("System","ExperimentalQuickExitF12InNewGameComic",0,ini)==1' in SRC
    assert "comic_trial[0]=='1'" in SRC
    assert "comic_exit_opt_in,1" in SRC
    assert "postmovie_opt_in,0,0)==1 &&" in SRC
    watcher=SRC[SRC.index("static DWORD WINAPI f12_input_thread("):
                SRC.index("void SpiderQuickExitF12_Init(")]
    assert "COMIC F12 deferred until native New Game movie 4" in watcher
    assert "postmovie_state,4,0" in watcher
    assert "SPIDER_GAME_BINK_ACTIVE==0u" in watcher
    assert "SPIDER_BINK_VIDEO_HANDLE==0u" in watcher
    assert "last_comic_candidate_ms" in watcher
    assert "input_native_invocations,0,0)==0" in watcher
    assert "postmovie_state,0,4" in watcher
    assert "12000u" in watcher
    # Native still can start less than 400 ms after a menu frame; the
    # exactly observed comic marker must win over stale menu priority.
    assert watcher.index("COMIC F12 deferred until") < watcher.index(
        "F12 queued for REAL Shell_MainMenu")
    assert watcher.index("InterlockedExchange(&postmovie_request_ms,(LONG)now);") < watcher.index(
        "InterlockedCompareExchange(&postmovie_state,4,0)")
    tick=SRC[SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"):
             SRC.index("/* Wrapper on the ORIGINAL GAME THREAD")]
    assert "stage==1 || stage==4" in tick
    assert "stage==4 ? 12000u : 5000u" in tick
    assert "comic_exit_opt_in,0,0)==1" in tick
    assert "(DWORD)frame[4]==0x00427A00u" in tick
    assert "(DWORD)frame[6]==1u" in tick
    assert "postmovie_state,2,stage" in tick
    assert 'stage==4 ? "DEFERRED-COMIC" : "CINEMATIC"' in tick
    assert tick.count("native_result=SPIDER_NATIVE_BINK_FRAME_TICK();")==1
    assert "return native_result;" in tick
    # It is a delayed exit, never a synthetic animation skip / resource free.
    for forbidden in ("ExitProcess(", "TerminateProcess(", "BinkClose(",
                      "PostMessageA(", "DestroyWindow("):
        assert forbidden not in tick
    comic=SRC[SRC.index("void SpiderQuickExitF12_ObserveComicRender("):
              SRC.index("/* Read-only stage discovery")]
    assert "*SPIDER_GAME_BINK_ACTIVE=" not in comic
    assert "*SPIDER_ENGINE_MODE=" not in comic
    assert "SpiderQuickExitF12_ObserveComicRender" in comic




def test_new_game_movie_and_comic_ini_require_existing_f12_and_debug_gates():
    init=SRC[SRC.index("void SpiderQuickExitF12_Init(void (*log_fn)(const char *))"):]
    movie=init[init.index('GetPrivateProfileIntA("System","ExperimentalQuickExitF12InNewGameMovie"'):]
    comic=init[init.index('GetPrivateProfileIntA("System","ExperimentalQuickExitF12InNewGameComic"'):]
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in init
    assert 'InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1' in init
    assert 'strcmp(val,"1")==0' in init
    assert "postmovie_opt_in,1" in movie
    assert "comic_exit_opt_in,1" in comic
    assert "postmovie_opt_in,0,0)==1" in init
    # Ensure both old environment variables remain available for Wine/Lutris
    # users, and new INI fields do not automatically enable either stage.
    assert "SPIDERMAN2K1FIX_F12_POSTMENU_MOVIE_EXIT" in movie
    assert "SPIDERMAN2K1FIX_F12_COMIC_EXIT" in comic
    for option in ("ExperimentalQuickExitF12InNewGameMovie",
                   "ExperimentalQuickExitF12InNewGameComic"):
        assert f"{option}=0" in INI
        assert f'"{option}=0\\r\\n"' in GEN




def test_intro_still_f12_is_separate_default_off_and_preserves_native_cleanup():
    assert "ExperimentalQuickExitF12InIntroStills=0" in INI
    assert '"ExperimentalQuickExitF12InIntroStills=0\\r\\n"' in GEN
    assert '"ExperimentalQuickExitF12InIntroStills"' in VAL
    assert 'GetPrivateProfileIntA("System","ExperimentalQuickExitF12InIntroStills",0,ini)==1' in SRC
    assert "static volatile LONG intro_still_opt_in=0;" in SRC
    assert "static volatile LONG intro_still_exit_state=0;" in SRC
    assert "SPIDER_INTRO_STILL_INPUT_CALL ((BYTE *)(uintptr_t)0x004A1896u)" in SRC
    assert "intro_still_input_original[5]={0xE8,0xE5,0xA8,0x06,0x00}" in SRC
    assert "SPIDER_INTRO_STILL_NATIVE_INPUT ((BYTE (__cdecl *)(DWORD,DWORD,DWORD))(uintptr_t)0x0050C180u)" in SRC

    owner=SRC[SRC.index("static BYTE __cdecl spider_f12_intro_still_native_input("):
              SRC.index("/* Returns nonzero ONLY when a recent")]
    assert "SPIDER_INTRO_STILL_NATIVE_INPUT(mask,arg1,arg2)" in owner
    assert "(DWORD)frame[1]==0x004A189Bu" in owner
    assert "mask==0x40010u && arg1==1u && arg2==1u" in owner
    assert "post_intro_cleanup_seen,0,0)==1" in owner
    assert "real_main_menu_seen,0,0)==0" in owner
    assert "intro_still_exit_state,2,1" in owner
    assert "InterlockedExchange(&pending,1);" in owner
    assert "return 1;" in owner
    assert "return original;" in owner
    for forbidden in ("ExitProcess(", "TerminateProcess(", "DestroyWindow(",
                      "PostMessageA(", "BinkClose(", "SPIDER_GAME_QUIT_FLAG=",
                      "0x4A18AEu;"):
        assert forbidden not in owner

    watcher=SRC[SRC.index("static DWORD WINAPI f12_input_thread("):
                SRC.index("void SpiderQuickExitF12_Init(")]
    assert "INTRO-STILL F12 latched after original Bink cleanup" in watcher
    assert "intro_still_exit_state,1,0" in watcher
    assert "intro_still_exit_state,0,state" in watcher
    assert "(DWORD)(GetTickCount()-start)>15000u" in watcher
    assert watcher.index("intro_still_exit_state,1,0") < watcher.index(
        "intro movie exit requested")
    assert "INTRO-STILL native page owner returned; original Shell_MainMenu Quit now activated" in SRC
    assert "install_intro_still_input_hook()" in SRC
    assert "restore_intro_still_input_hook()" in SRC
    assert "intro_still_hook_installed,0,1" in SRC



def test_default_off_and_ini_section_validated():
    assert "QuickExitF12=0" in INI
    assert '"[System]\\r\\n"' in GEN
    assert '"QuickExitF12=0\\r\\n"' in GEN
    assert '"Debug","System","Display"' in VAL
    assert 'spider_ini_system[]={"QuickExitF12","ExperimentalQuickExitF12In3D","ExperimentalQuickExitF12InNewGameMovie","ExperimentalQuickExitF12InNewGameComic","ExperimentalQuickExitF12InIntroStills"}' in VAL
    for option in ("ExperimentalQuickExitF12InNewGameMovie",
                   "ExperimentalQuickExitF12InNewGameComic"):
        assert f"{option}=0" in INI
        assert f'"{option}=0\\r\\n"' in GEN
        assert f'GetPrivateProfileIntA("System","{option}",0,ini)' in SRC
    assert "ExperimentalQuickExitF12In3D=0" in INI
    assert '"ExperimentalQuickExitF12In3D=0\\r\\n"' in GEN
    assert 'GetPrivateProfileIntA("System","ExperimentalQuickExitF12In3D",0,ini)' in SRC
    assert 'GetPrivateProfileStringA("System","QuickExitF12","0"' in SRC
    assert 'if(strcmp(val,"1")!=0)' in SRC
    assert 'OFF; unsafe original F12 WM_CLOSE suppressed' in SRC



def test_f12_debug_fast_test_mode_default_off_and_valid_ini_key():
    assert "F12TestMode=0" in INI
    assert '"F12TestMode=0\\r\\n"' in GEN
    assert 'spider_ini_debug[]={"Log","F12TestMode"}' in VAL
    assert 'GetPrivateProfileIntA("Debug","F12TestMode",0,ini)==1' in SRC
    assert "static volatile LONG debug_f12_test_mode=0;" in SRC

    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert 'strcpy(val,"1");' in init
    assert 'InterlockedExchange(&debug_f12_test_mode,1)' in init
    assert "debug_f12_test_mode,0,0)==1 ||" in init
    assert "intro_still_opt_in,1" in init
    assert "experimental_game3d_enabled" in init


def test_f12_debug_shortcut_uses_game_owned_menu_cleanup_without_quit():
    after_intro=SRC[SRC.index("static void __cdecl spider_after_intro_cleanup("):
                    SRC.index("/* Wrapper for ORIGINAL 0x4A1896")]
    assert "SPIDER_AFTER_INTRO_CLEANUP(mode);" in after_intro
    assert "intro_quit_pending,0,0)==1 &&" in after_intro
    assert "debug_f12_test_mode,0,0)==1" in after_intro
    assert after_intro.index("debug_f12_test_mode,0,0)==1") < after_intro.index(
        "*SPIDER_GAME_QUIT_FLAG=1u;")
    assert "DEBUG-MENU-SKIP opening Bink cleanup completed" in after_intro

    input_owner=SRC[SRC.index("static BYTE __cdecl spider_f12_intro_still_native_input("):
                    SRC.index("/* Returns nonzero ONLY")]
    assert "SPIDER_INTRO_STILL_NATIVE_INPUT(mask,arg1,arg2)" in input_owner
    assert "intro_still_exit_state,2,1" in input_owner
    assert "DEBUG-MENU-SKIP original 0x4A1896 input-wait TRUE completion" in input_owner
    assert input_owner.index('debug_f12_test_mode,0,0)==1') < input_owner.index(
        "InterlockedExchange(&pending,1);")
    assert "return 1;" in input_owner

    menu=SRC[SRC.index("int __cdecl SpiderQuickExitF12_ConsumeRealMenuRequest("):
             SRC.index("/* x86 stack at entry")]
    assert "DEBUG-MENU-SKIP real Shell_MainMenu reached" in menu
    assert menu.index('debug_f12_test_mode,0,0)==1') < menu.index(
        "InterlockedCompareExchange(&pending,0,1)==1")
    assert "return 0;" in menu



def test_debug_intro_skip_latch_is_one_shot_and_cannot_cancel_new_game_bink():
    """Windows/Wine log 20261023-112854: intro F12 reaches main menu,
    then New Game comic appears but Bink #4 is missing. The original
    0x470750 Bink tick is SHARED: a stale intro_quit_pending=1 returns
    zero BEFORE the real tick, silently skipping the next cinematic.
    """
    tick=SRC[SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"):
             SRC.index("static void __cdecl spider_after_intro_cleanup(int mode)")]
    assert 'if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1)' in tick
    assert tick.index('if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1)') < tick.index(
        "native_result=SPIDER_NATIVE_BINK_FRAME_TICK();")
    assert "return 0;" in tick

    after_intro=SRC[SRC.index("static void __cdecl spider_after_intro_cleanup(int mode)"):
                    SRC.index("/* Executes instead of the exact ORIGINAL")]
    assert "SPIDER_AFTER_INTRO_CLEANUP(mode);" in after_intro
    debug=after_intro[after_intro.index(
        "if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1 &&"):
        after_intro.index(
        "} else if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1) {")]
    # Original game cleanup before publishing the still latch, then
    # clear the shared movie-skip flag BEFORE any New Game movie.
    assert "DEBUG-MENU-SKIP opening Bink cleanup completed" in debug
    assert debug.index("intro_still_exit_state,1,0") < debug.index(
        "InterlockedExchange(&intro_quit_pending,0);")
    assert "InterlockedExchange(&intro_tick_shortcircuited,0);" in debug
    assert "movie #4 tick restored for future New Game playback" in debug
    assert "SPIDER_GAME_QUIT_FLAG" not in debug
    assert "ExitProcess(" not in debug
    legacy=after_intro[after_intro.index(
        "} else if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1) {"):]
    assert "*SPIDER_GAME_QUIT_FLAG=1u;" in legacy
    assert "InterlockedExchange(&intro_quit_pending,0);" not in legacy




def test_debug_menu_input_delay_probe_is_read_only_and_native_gated():
    """Log visible-menu input eligibility vs original transition flag.
    Native 0x493DDB gates the keyboard/mouse handling when [0x54D38C]
    is nonzero. This does not prove that DirectInput accepted an event.
    """
    menu=SRC[SRC.index("int __cdecl SpiderQuickExitF12_ConsumeRealMenuRequest("):
             SRC.index("/* x86 stack at entry")]
    assert "DEBUG-MENU-INPUT-PROBE" in menu
    assert "intro_still_exit_state,0,0)==3" in menu
    assert "n<=210" in menu
    assert "n<=150 && n%15==0" in menu
    assert "*SPIDER_TRACE_TRANSITION_RAW" in menu
    assert "(const volatile DWORD *)(uintptr_t)0x006B78F0u" in menu
    assert 'transition?"GATED":"ELIGIBLE"' in menu
    assert "NO DEVICE-HR CLAIM" in menu
    assert "ms_since_first_menu" in menu
    for forbidden in ("ExitProcess(", "TerminateProcess(", "DestroyWindow(",
                      "SetForegroundWindow(", "SetWindowPos(", "Acquire(",
                      "GetDeviceData(", "*SPIDER_TRACE_TRANSITION_RAW=",
                      "*SPIDER_ENGINE_MODE=", "*SPIDER_GAME_QUIT_FLAG="):
        assert forbidden not in menu



def test_debug_fast_intro_menu_tick_wall_time_partition_is_passive():
    """Differentiate 4-second gap *inside* original 0x505720 tick
    from a 4-second gap between native menu ticks, with no input edits."""
    assert "static volatile LONG debug_menu_tick_entry_ms=0;" in SRC
    assert "static volatile LONG debug_menu_last_exit_ms=0;" in SRC
    start=SRC.index("void __cdecl SpiderQuickExitF12_MarkMenuTickEntry(void)")
    end=SRC.index("int __cdecl SpiderQuickExitF12_ConsumeRealMenuRequest",start)
    entry=SRC[start:end]
    assert "debug_f12_test_mode,0,0)==1" in entry
    assert "intro_still_exit_state,0,0)>=2" in entry
    assert "menu_frame_count,0,0)<65" in entry
    assert "InterlockedExchange(&debug_menu_tick_entry_ms,(LONG)GetTickCount())" in entry

    consumer=SRC[SRC.index("int __cdecl SpiderQuickExitF12_ConsumeRealMenuRequest(void)"):
                 SRC.index("/* x86 stack at entry")]
    assert "DEBUG-MENU-TICK-TIMING frame=" in consumer
    assert "native_0x505720_ms=" in consumer
    assert "since_last_exit_ms=" in consumer
    assert "InterlockedExchange(&debug_menu_last_exit_ms,(LONG)exit_ms)" in consumer
    assert "entry_ms-prev_ms" in consumer
    assert "exit_ms-entry_ms" in consumer
    assert "n<=20 || n%10==0 || native_ms>=100u || between_ms>=100u" in consumer

    start=SRC.index("__attribute__((naked,used)) static void spider_real_menu_tick_hook(void)")
    end=SRC.index("static int write_code(",start)
    hook=SRC[start:end]
    assert '"pushfl' in hook and '"pushal' in hook
    assert '"call _SpiderQuickExitF12_MarkMenuTickEntry' in hook
    assert hook.index('call _SpiderQuickExitF12_MarkMenuTickEntry') < hook.index(
        'movl $0x00505720')
    assert hook.index('movl $0x00505720') < hook.index(
        'call _SpiderQuickExitF12_ConsumeRealMenuRequest')
    assert '"popal' in hook and '"popfl' in hook
    assert '"ret' in hook

    for forbidden in ("ExitProcess(", "TerminateProcess(", "DestroyWindow(",
                      "SetForegroundWindow(", "SetWindowPos(", "Acquire(",
                      "GetDeviceData(", "*SPIDER_TRACE_TRANSITION_RAW=",
                      "*SPIDER_ENGINE_MODE=", "*SPIDER_GAME_QUIT_FLAG="):
        assert forbidden not in entry and forbidden not in consumer



def test_debug_first_menu_native_clock_busy_wait_probe_is_gated_and_read_only():
    """Original 0x4E5D60(1) loops until game clock [0x6B4CA0]
    advances. Both exact native menu callsites are wrapped and
    always invoke the original wait once, with no bypass."""
    assert "SPIDER_MENU_WAIT_COND_SITE ((BYTE *)(uintptr_t)0x0049415Bu)" in SRC
    assert "SPIDER_MENU_WAIT_ALWAYS_SITE ((BYTE *)(uintptr_t)0x0049416Fu)" in SRC
    assert "SPIDER_NATIVE_CLOCK_WAIT ((void (__cdecl *)(DWORD))(uintptr_t)0x004E5D60u)" in SRC
    assert "menu_wait_cond_original[5]={0xE8,0x00,0x1C,0x05,0x00}" in SRC
    assert "menu_wait_always_original[5]={0xE8,0xEC,0x1B,0x05,0x00}" in SRC
    assert "static volatile LONG debug_menu_wait_hooks_installed=0;" in SRC

    area=SRC[SRC.index("static void __cdecl spider_f12_debug_menu_wait(DWORD ticks,int which)"):
             SRC.index("static int install_intro_timed_clock_hook(void)")]
    assert area.count("SPIDER_NATIVE_CLOCK_WAIT(ticks);")==1
    assert area.index("SPIDER_NATIVE_CLOCK_WAIT(ticks);") < area.index(
        "DEBUG-MENU-CLOCK-WAIT site=")
    assert "native_clock_before=" in area
    assert "native_clock_after=" in area
    assert "elapsed_ms=" in area
    assert "intro_still_exit_state,0,0)>=2" in area
    assert "menu_frame_count,0,0)<=3" in area
    assert "memcmp(SPIDER_MENU_WAIT_COND_SITE,menu_wait_cond_original,5)" in area
    assert "memcmp(SPIDER_MENU_WAIT_ALWAYS_SITE,menu_wait_always_original,5)" in area
    assert "patch_native_call(SPIDER_MENU_WAIT_COND_SITE,menu_wait_cond_original" in area
    assert "patch_native_call(SPIDER_MENU_WAIT_ALWAYS_SITE,menu_wait_always_original" in area
    assert "restore_intro_tick_call(SPIDER_MENU_WAIT_COND_SITE,menu_wait_cond_original)" in area
    assert "restore_intro_tick_call(SPIDER_MENU_WAIT_ALWAYS_SITE,menu_wait_always_original)" in area
    assert "InterlockedExchange(&debug_menu_wait_hooks_installed,1)" in area

    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in init
    assert "install_debug_menu_wait_hooks()" in init
    assert "signature mismatch; original waits preserved" in init
    assert "restore_debug_menu_wait_hooks();" in init
    assert init.count("restore_debug_menu_wait_hooks();")>=2
    for forbidden in ("ExitProcess(", "TerminateProcess(", "DestroyWindow(",
                      "SetForegroundWindow(", "Acquire(", "GetDeviceData(",
                      "*SPIDER_TRACE_NATIVE_CLOCK_RAW=", "Sleep(", "PostMessageA("):
        assert forbidden not in area




def test_new_game_submenu_clock_wait_probe_is_passive_and_covers_menu_exit():
    """Original Shell_MainMenu calls 0x4E5D60 at 0x494209 AFTER
    the menu item has been selected and the menu resources cleaned up.
    User reports a new 3-4s stall when choosing New Game. This
    diagnostic callsite must ALWAYS run the original wait untouched.
    """
    assert "SPIDER_MENU_WAIT_EXIT_SITE ((BYTE *)(uintptr_t)0x00494209u)" in SRC
    assert "menu_wait_exit_original[5]={0xE8,0x52,0x1B,0x05,0x00}" in SRC
    assert "static volatile LONG debug_menu_wait_exit_hook_installed=0;" in SRC
    body=SRC[SRC.index("static void __cdecl spider_f12_debug_menu_wait(DWORD ticks,int which)"):
             SRC.index("static void __cdecl spider_f12_debug_menu_wait_cond(DWORD ticks)")]
    assert "if(which!=3 &&" not in body
    assert "SPIDER_NATIVE_CLOCK_WAIT(ticks);" in body
    assert body.count("SPIDER_NATIVE_CLOCK_WAIT(ticks);")==1
    assert "if(which==3 &&" in body
    assert "DEBUG-MENU-EXIT-CLOCK-WAIT site=0x00494209" in body
    assert "SELECTED ACTION UNKNOWN" in body
    assert "elapsed_ms>=100u" in body
    assert "init_bypass_state" not in body
    assert "native_clock_before=" in body
    assert "native_clock_after=" in body
    install=SRC[SRC.index("static void install_debug_menu_wait_exit_probe(void)"):
                SRC.index("static int install_intro_timed_clock_hook(void)")]
    assert "patch_native_call(SPIDER_MENU_WAIT_EXIT_SITE" in install
    assert "menu_wait_exit_original" in install
    assert "spider_f12_debug_menu_wait_exit" in install
    assert "InterlockedExchange(&debug_menu_wait_exit_hook_installed,1)" in install
    assert "restore_intro_tick_call(SPIDER_MENU_WAIT_EXIT_SITE,menu_wait_exit_original)" in install
    assert "if(GetPrivateProfileIntA(\"Debug\",\"Log\",0,ini)==1)" in SRC
    assert "install_debug_menu_wait_exit_probe();" in SRC
    for forbidden in ("ExitProcess(", "TerminateProcess(", "DestroyWindow(",
                      "SetForegroundWindow(", "SendInput(", "GetDeviceData(",
                      "*SPIDER_TRACE_NATIVE_CLOCK_RAW=", "Sleep("):
        assert forbidden not in body

def test_retired_fast_menu_wait_workaround_is_absent():
    """Native clock synchronization is never bypassed in F12TestMode."""
    for source in (SRC, INI, GEN, VAL):
        assert "F12SkipInitialMenuWait" not in source
    for obsolete in ("debug_skip_initial_menu_wait",
                     "debug_menu_wait_skip_state",
                     "DEBUG-MENU-FAST-WAIT", "4500MS-SAFETY-TIMEOUT"):
        assert obsolete not in SRC
    body=SRC[SRC.index("static void __cdecl spider_f12_debug_menu_wait(DWORD ticks,int which)"):
             SRC.index("static void __cdecl spider_f12_debug_menu_wait_cond(DWORD ticks)")]
    assert body.count("SPIDER_NATIVE_CLOCK_WAIT(ticks);")==1
    assert "return;" not in body
    assert "DEBUG-MENU-EXIT-CLOCK-WAIT" in body
    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert 'GetPrivateProfileIntA("Debug","F12TestMode",0,ini)==1' in init
    assert "install_debug_menu_wait_hooks()" in init
    assert "signature mismatch; original waits preserved" in init


def test_f12_debug_shortcut_restricts_3d_exit_to_native_gameplay():
    worker=SRC[SRC.index("static DWORD WINAPI f12_input_thread("):
               SRC.index("void SpiderQuickExitF12_Init(")]
    debug=worker[worker.index("if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {"):
                 worker.index("} else if(InterlockedCompareExchange(&comic_exit_opt_in,0,0)==1 &&")]
    assert "post_intro_cleanup_seen,0,0)==1" in debug
    assert "real_main_menu_seen,0,0)==0" in debug
    assert "DEBUG-MENU-SKIP F12 in opening Bink" in debug
    assert "DEBUG-MENU-SKIP F12 on startup still" in debug
    assert "DEBUG-3D-EXIT native gameplay exit queued" in debug
    assert "last_native_game_input_ms,0,0" in debug
    assert "last_game3d_flip_ms,0,0" in debug
    assert "*SPIDER_GAME_BINK_ACTIVE==0u" in debug
    assert "*SPIDER_BINK_VIDEO_HANDLE==0u" in debug
    assert "game3d_exit_state,1,0" in debug
    for forbidden in ("ExitProcess(", "TerminateProcess(", "DestroyWindow(",
                      "PostMessageA(", "SendInput(", "keybd_event(",
                      "BinkClose(", "intro_still_exit_state,3,2"):
        assert forbidden not in debug



def test_f12_debug_accelerates_timed_still_via_original_clock_comparison():
    """Only the debug mode is permitted to end the timed still early.
    An exact five-byte clock read becomes a CALL, which returns EDI+1
    to the game's existing CMP EAX,EDI / JA normal SEH/resource cleanup.
    """
    assert "SPIDER_INTRO_TIMED_CLOCK_SITE ((BYTE *)(uintptr_t)0x004A1A26u)" in SRC
    assert "intro_timed_clock_original[5]={0xA1,0xA0,0x4C,0x6B,0x00}" in SRC
    assert "static volatile LONG intro_timed_clock_hook_installed=0;" in SRC
    assert "static volatile LONG debug_timed_accelerated=0;" in SRC
    begin=SRC.index("__attribute__((naked,used)) static DWORD spider_f12_debug_timed_clock(void)")
    end=SRC.index("/* Wrapper for ORIGINAL 0x4A1896 input check",begin)
    hook=SRC[begin:end]
    assert '"movl 0x006B4CA0, %eax' in hook
    assert '"cmpl $1, _debug_f12_test_mode' in hook
    assert '"cmpl $1, _intro_still_exit_state' in hook
    assert '"cmpl $1, _post_intro_cleanup_seen' in hook
    assert '"cmpl $1, _intro_still_hook_installed' in hook
    assert '"cmpl $0, _real_main_menu_seen' in hook
    assert '"cmpb $1, 0x006A7785' in hook
    assert '"cmpl $0, 0x006151F8' in hook
    assert '"cmpl $0, 0x00AC0BA4' in hook
    assert '"cmpl $-1, %edi' in hook
    assert '"leal 1(%edi), %eax' in hook
    assert '"movl $1, _debug_timed_accelerated' in hook
    assert '"ret' in hook
    for forbidden in ('movl %eax, 0x006B4CA0', 'jmp 0x4A1A36',
                      'ExitProcess(', 'TerminateProcess(', 'DestroyWindow('):
        assert forbidden not in hook

    install=SRC[SRC.index("static int install_intro_timed_clock_hook(void)"):
                SRC.index("static int install_intro_still_input_hook(void)")]
    assert "patch_native_call(SPIDER_INTRO_TIMED_CLOCK_SITE" in install
    assert "intro_timed_clock_original" in install
    assert "restore_intro_tick_call(SPIDER_INTRO_TIMED_CLOCK_SITE" in install

    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert "debug_f12_test_mode,0,0)==1 &&" in init
    assert "!install_intro_timed_clock_hook()" in init
    assert "DEBUG F12TestMode DISABLED: timed-still clock signature mismatch" in init
    assert init.count("restore_intro_timed_clock_hook();")>=2
    assert "DEBUG first timed-page clock read 0x4A1A26 guarded hook installed" in init

    owner=SRC[SRC.index("static BYTE __cdecl spider_f12_intro_still_native_input("):
              SRC.index("/* Returns nonzero ONLY")]
    assert "DEBUG-MENU-SKIP first timed-page deadline accelerated" in owner
    assert "InterlockedCompareExchange(&debug_timed_accelerated,0,1)" in owner
    assert "DEBUG-MENU-SKIP original 0x4A1896 input-wait TRUE completion" in owner



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


def test_intro_exit_latch_survives_four_movies_even_when_top_level_quit_check_is_skipped():
    assert "static volatile LONG intro_quit_pending=0;" in SRC
    assert "InterlockedCompareExchange(&intro_quit_pending,1,0)==0" in SRC
    assert "InterlockedCompareExchange(&intro_quit_pending,0,0)==1" in SRC
    assert "InterlockedCompareExchange(&intro_quit_pending,0,1)==1" not in SRC
    assert "static void __cdecl spider_after_intro_cleanup(int mode)" in SRC
    assert "SPIDER_AFTER_INTRO_CLEANUP(mode);" in SRC
    assert "*SPIDER_GAME_QUIT_FLAG=1u;" in SRC
    # The original 0x455E50 comparison exists but is not on every
    # startup control-flow path (0x455DC8 / 0x455DD0 may bypass it).
    assert "Quit byte forwarded (top-level 0x455E50 may be bypassed by startup branches)" in SRC
    assert "INTRO-MOVIE single F12 carried into original timed/input-wait still cleanup" in SRC
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
    watcher=SRC[SRC.index("static DWORD WINAPI f12_input_thread("):
                SRC.index("void SpiderQuickExitF12_Init(")]
    # The NEW early-still wrapper also checks menu_seen==0. The original
    # main-menu priority assertion is only about the F12 watcher order.
    assert watcher.index("if(menu_time && (DWORD)(now-(DWORD)menu_time)<=400u)") < watcher.index(
        "intro movie exit requested")


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



def test_opening_movie_single_f12_survives_bypassed_quit_test_by_native_still_handoff():
    """Windows #589: quit=1 after four Binks, but original startup
    branches can BYPASS 0x455E50 and advance to the first still.
    The *same* F12 must carry into native still cleanup when opted in.
    Do not manufacture another keypress or bypass original page owners.
    """
    begin=SRC.index("static void __cdecl spider_after_intro_cleanup(int mode)")
    end=SRC.index("/* Wrapper for ORIGINAL 0x4A1896 input check",begin)
    checkpoint=SRC[begin:end]
    assert "0x455DC8 / 0x455DD0" in checkpoint
    assert "0x455E50" in checkpoint
    assert checkpoint.index("SPIDER_AFTER_INTRO_CLEANUP(mode);") < checkpoint.index(
        "post_intro_cleanup_seen,1,0")
    assert checkpoint.index("post_intro_cleanup_seen,1,0") < checkpoint.index(
        "intro_quit_pending,0,0")
    # The legacy exit branch still forwards Quit first; Debug F12TestMode
    # is a different branch and must NOT write the game Quit flag.
    legacy=checkpoint[checkpoint.index(
        "} else if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1) {"):]
    assert legacy.index("*SPIDER_GAME_QUIT_FLAG=1u;") < legacy.index(
        "intro_still_exit_state,1,0")
    for guard in (
        "SpiderQuickExitF12_Enabled()",
        "intro_still_opt_in,0,0)==1",
        "intro_still_hook_installed,0,0)==1",
        "real_main_menu_seen,0,0)==0",
        "intro_still_exit_state,0,0)==0",
    ):
        assert guard in checkpoint
    assert "InterlockedExchange(&intro_still_request_ms,(LONG)GetTickCount());" in checkpoint
    assert "InterlockedCompareExchange(&intro_still_exit_state,1,0)==0" in checkpoint
    assert "INTRO-MOVIE single F12 carried into original timed/input-wait still cleanup" in checkpoint

    native_input=SRC[SRC.index("static BYTE __cdecl spider_f12_intro_still_native_input("):
                     SRC.index("/* Returns nonzero ONLY",begin)]
    assert "SPIDER_INTRO_STILL_NATIVE_INPUT(mask,arg1,arg2)" in native_input
    assert "intro_still_exit_state,2,1" in native_input
    assert "InterlockedExchange(&pending,1);" in native_input
    assert "return 1;" in native_input
    assert "INTRO-STILL native page owner returned; original Shell_MainMenu Quit now activated" in SRC

    watchdog=SRC[SRC.index("static DWORD WINAPI f12_input_thread("):
                 SRC.index("void SpiderQuickExitF12_Init(")]
    assert "(DWORD)(GetTickCount()-start)>15000u" in watchdog
    assert "intro_still_exit_state,0,state" in watchdog
    assert "if(state==2)InterlockedExchange(&pending,0);" in watchdog
    for forbidden in ("ExitProcess(", "TerminateProcess(", "DestroyWindow(",
                      "PostMessageA(", "SendInput(", "keybd_event(",
                      "BinkClose("):
        assert forbidden not in checkpoint


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
    # Comic is now a separately gated pre-menu exception; native menu
    # must still retain priority over the ordinary playable 3D exit.
    assert thread.index("COMIC F12 deferred until") < thread.index("if(menu_time &&")
    assert thread.index("if(menu_time &&") < thread.index("3D native exit intent queued")
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


def test_f12_newgame_movie_skip_does_not_use_shutdown_controller():
    """Movie #4 may be ended natively, but not via process-exit action 7."""
    worker=SRC[SRC.index("if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {",SRC.index("static DWORD WINAPI f12_input_thread(")):
               SRC.index("} else if(InterlockedCompareExchange(&comic_exit_opt_in,0,0)==1 &&",SRC.index("static DWORD WINAPI f12_input_thread("))]
    assert "DEBUG-NEWGAME-COMIC F12 armed" in worker
    assert "DEBUG-NEWGAME-MOVIE F12 queued" in worker
    assert "DEBUG-NEWGAME-3D F12 consumed" in worker
    assert worker.index("DEBUG-NEWGAME-3D F12 consumed") < worker.index(
        "DEBUG-3D-EXIT native gameplay exit queued")
    assert "InterlockedCompareExchange(&debug_newgame_progress,0,0)>=1" in worker
    assert "postmovie_state,4,0" in worker
    assert "postmovie_state,1,0" in worker
    assert "SendInput(" not in worker
    owner=SRC[SRC.index("static void __cdecl spider_observe_native_game_input(void)"):
              SRC.index("static void __cdecl spider_observe_native_game_cleanup(",SRC.index("static void __cdecl spider_observe_native_game_input(void)"))]
    assert "DEBUG-NEWGAME-MOVIE original Bink/script returned" in owner
    assert "InterlockedCompareExchange(&postmovie_state,0,2)==2" in owner
    assert owner.index("InterlockedCompareExchange(&postmovie_state,0,2)==2") < owner.index(
        "if(InterlockedCompareExchange(&postmovie_state,0,0)==2) {")
    assert "DEBUG-NEWGAME-3D-PHASE" in owner
    assert "GetAsyncKeyState(VK_SPACE)" in owner
    assert "PLAYABLE NOT VERIFIED" in owner
    wrapper=SRC[SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"):
                SRC.index("static void __cdecl spider_after_intro_cleanup(",SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"))]
    assert "InterlockedCompareExchange(&debug_newgame_progress,1,0)" in wrapper
    assert "(DWORD)frame[4]==0x00427A00u" in wrapper
    assert "InterlockedCompareExchange(&debug_newgame_progress,2,1)" in wrapper
    assert "native_result=SPIDER_NATIVE_BINK_FRAME_TICK();" in wrapper
    assert "DEBUG-NEWGAME-MOVIE original movie-4 finished" in wrapper


def test_f12_gameplay_exit_fails_closed_after_movie_owner_even_if_native_last_tick_was_missed():
    """A Bink/Enter boundary must not accidentally dispatch EXIT 7."""
    worker=SRC[SRC.index("if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {",SRC.index("static DWORD WINAPI f12_input_thread(")):
               SRC.index("} else if(InterlockedCompareExchange(&comic_exit_opt_in,0,0)==1 &&",SRC.index("static DWORD WINAPI f12_input_thread("))]
    assert "debug_newgame_progress,0,0)>=1" in worker
    assert worker.index("debug_newgame_progress,0,0)>=1") < worker.index(
        "game3d_exit_state,1,0")
    game_owner=SRC[SRC.index("static void __cdecl spider_observe_native_game_input(void)"):
                   SRC.index("static void __cdecl spider_observe_native_game_cleanup(")]
    assert "if((phase==1 || phase==2)" in game_owner
    assert "InterlockedCompareExchange(&debug_newgame_progress,3,phase)" in game_owner
    assert "InterlockedCompareExchange(&game3d_exit_state,0,1)==1" in game_owner
    assert "NO CONTROLLER 7" in game_owner


def test_newgame_f12_3d_two_space_skippable_scenes_remain_distinct_from_gameplay():
    """SPACE skips two consecutive native 3D scenes; their identity is
    not established by elapsed time or the number of SPACE key edges."""
    assert "static volatile LONG debug_newgame_3d_space_edges=0;" in SRC
    owner=SRC[SRC.index("static void __cdecl spider_observe_native_game_input(void)"):
              SRC.index("static void __cdecl spider_observe_native_game_cleanup(")]
    assert "GetAsyncKeyState(VK_SPACE)" in owner
    assert "space_down=%ld space_edges=%ld" in owner
    assert "space_gate_bit100=%lu [READ-ONLY; PLAYABLE NOT VERIFIED]" in owner
    assert "GetAsyncKeyState(VK_RETURN)" not in owner
    worker=SRC[SRC.index("static DWORD WINAPI f12_input_thread("):
               SRC.index("void SpiderQuickExitF12_Init(")]
    assert "DEBUG-NEWGAME-3D-SPACE-EVENT" in worker
    assert "InterlockedIncrement(&debug_newgame_3d_space_edges)" in worker
    assert "previous_3d_space==0 && space_down" in worker
    assert "SCENE ID UNKNOWN; NO EXIT AUTHORITY" in worker
    assert "TWO SPACE-skippable 3D introductions vs gameplay NOT YET VERIFIED" in worker
    assert worker.index("DEBUG-NEWGAME-3D F12 consumed") < worker.index(
        "DEBUG-3D-EXIT native gameplay exit queued")
    assert "SendInput(" not in worker


def test_new_game_comic_native_l1cov_timer_is_skipped_without_clock_or_script_writes():
    """The native L1COV.bmp loop compares delta clock ticks to 0x12c,
    then falls through the real native expiry branch to video #4.
    Only the original 6-byte game-clock *read* may be replaced."""
    assert "SPIDER_NEWGAME_COMIC_CLOCK_SITE ((BYTE *)(uintptr_t)0x004279B8u)" in SRC
    assert "SPIDER_NEWGAME_L1COV_OWNER ((const BYTE *)(uintptr_t)0x0042796Du)" in SRC
    assert "SPIDER_NEWGAME_MOVIE4_OPCODE ((const BYTE *)(uintptr_t)0x004279F9u)" in SRC
    assert "newgame_comic_clock_original[6]={0x8B,0x15,0xA0,0x4C,0x6B,0x00}" in SRC
    assert "newgame_l1cov_owner_signature[5]={0x68,0x14,0x92,0x54,0x00}" in SRC
    assert "newgame_movie4_opcode_signature[7]={0x6A,0x04,0xE8,0x50,0x8D,0x04,0x00}" in SRC
    asm=SRC[SRC.index("__attribute__((naked,used)) static void spider_f12_debug_comic_clock(void)"):
            SRC.index("/* Wrapper for ORIGINAL 0x4A1896 input check")]
    assert '"movl 0x006B4CA0, %edx' in asm
    for gate in ('cmpl $1, _debug_f12_test_mode',
                 'cmpl $1, _debug_newgame_comic_clock_hook_installed',
                 'cmpl $4, _postmovie_state',
                 'cmpl $1, _real_main_menu_seen',
                 'cmpl $0, _debug_newgame_progress',
                 'cmpl $0, 0x006151F8',
                 'cmpl $0, 0x00AC0BA4'):
        assert gate in asm
    assert '"leal 0x12c(%esi), %edx' in asm
    assert '"movl $1, _debug_newgame_comic_fastforward_observed' in asm
    assert 'ret' in asm
    for forbidden in ('movl %edx, 0x006B4CA0', 'jmp 0x4279f0',
                      'ExitProcess(', 'TerminateProcess(', 'SendInput('):
        assert forbidden not in asm
    install=SRC[SRC.index("static int install_newgame_comic_clock_hook(void)"):
                SRC.index("static int install_intro_still_input_hook(void)")]
    assert "BYTE patched[6]={0xE8,0,0,0,0,0x90}" in install
    assert "SPIDER_NEWGAME_COMIC_CLOCK_SITE+5" in install
    assert "newgame_l1cov_owner_signature" in install
    assert "newgame_movie4_opcode_signature" in install
    assert "newgame_comic_clock_original,patched,6" in install
    assert "PAGE_EXECUTE_READWRITE" in install
    assert "memcpy(SPIDER_NEWGAME_COMIC_CLOCK_SITE,newgame_comic_clock_original,6)" in install
    worker=SRC[SRC.index("static DWORD WINAPI f12_input_thread("):
               SRC.index("void SpiderQuickExitF12_Init(")]
    assert "DEBUG-NEWGAME-COMIC F12 armed" in worker
    assert "postmovie_state,4,0" in worker
    assert "DEBUG-NEWGAME-COMIC native clock hook unavailable" in worker
    assert "SPIDER_GAME_BINK_ACTIVE=0" not in asm
    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert "install_newgame_comic_clock_hook()" in init
    assert "restore_newgame_comic_clock_hook();" in init
    movie=SRC[SRC.index("static BYTE __cdecl spider_f12_native_intro_tick(void)"):
              SRC.index("/* Wrapper on the ORIGINAL GAME THREAD")]
    assert "DEBUG-NEWGAME-COMIC-FINISHED" in movie
    assert "postmovie_state,2,stage" in movie


def test_debug_3d_original_action_callsite_attribution_cannot_skip_or_exit():
    """Five original 0x440EF0 native input calls are observed unmodified."""
    for va,blob in (
        ("0x00440F34u","0xE8,0x47,0xB2,0x0C,0x00"),
        ("0x00440F77u","0xE8,0x04,0xB2,0x0C,0x00"),
        ("0x00440F8Eu","0xE8,0xED,0xB1,0x0C,0x00"),
        ("0x00440FAFu","0xE8,0xCC,0xB1,0x0C,0x00"),
        ("0x00441606u","0xE8,0x75,0xAB,0x0C,0x00"),
    ):
        assert va in SRC
        assert blob in SRC
    assert "SPIDER_NATIVE_3D_INPUT_SITES_COUNT 5" in SRC
    owner=SRC[SRC.index("static BYTE __cdecl spider_probe_3d_native_input("):
              SRC.index("/* PASSIVE owner cleanup checkpoint")]
    assert "BYTE original=SPIDER_NATIVE_3D_INPUT(mask,arg1,arg2);" in owner
    assert "return original;" in owner
    assert "DEBUG-3D-NATIVE-INPUT" in owner
    assert "phase_state=%ld phase_object=%lu script_slots_active=%lu" in owner
    assert "GetAsyncKeyState(VK_SPACE)" in owner
    assert "SPIDER_NATIVE_3D_INPUT(mask,arg1,arg2)" in owner
    for bad in ("SendInput(", "keybd_event(", "PostMessageA(",
                "ExitProcess(", "TerminateProcess(",
                "*(volatile DWORD *)(uintptr_t)0x0060CFA4u=",
                "return 1;"):
        assert bad not in owner
    hooks=SRC[SRC.index("static int install_debug_3d_native_input_probes(void)"):
              SRC.index("static int install_passive_game_input_probe(void)")]
    assert "memcmp(spider_native_3d_input_sites[i]" in hooks
    assert "spider_native_3d_input_original[i],5" in hooks
    assert "spider_probe_3d_native_input" in hooks
    assert "for(j=i-1;j>=0;--j)" in hooks
    assert "restore_intro_tick_call" in hooks
    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert "install_debug_3d_native_input_probes()" in init
    assert init.count("restore_debug_3d_native_input_probes();")>=2
    assert "GetPrivateProfileIntA(\"Debug\",\"Log\",0,ini)==1" in init
    assert "F12 exit guard preserved" in init


def test_original_3d_space_direct_poll_gates_are_read_only():
    """Two original native Space keyboard polls must never be forced."""
    assert "SPIDER_NATIVE_3D_SPACE_CALL_1 ((BYTE *)(uintptr_t)0x00455427u)" in SRC
    assert "SPIDER_NATIVE_3D_SPACE_CALL_2 ((BYTE *)(uintptr_t)0x00455444u)" in SRC
    assert "spider_native_3d_space_original_1[5]={0xE8,0x24,0x52,0x0B,0x00}" in SRC
    assert "spider_native_3d_space_original_2[5]={0xE8,0x07,0x52,0x0B,0x00}" in SRC
    assert "SPIDER_NATIVE_3D_SPACE_POLL ((DWORD (__cdecl *)(DWORD,DWORD))(uintptr_t)0x0050A650u)" in SRC
    assert "SPIDER_NATIVE_CUTSCENE_INPUT_FLAGS ((const volatile DWORD *)(uintptr_t)0x02E0988Cu)" in SRC
    probe=SRC[SRC.index("static DWORD __cdecl spider_probe_3d_space_gate("):
              SRC.index("/* PASSIVE owner cleanup checkpoint")]
    assert "DWORD original=SPIDER_NATIVE_3D_SPACE_POLL(dik,input_mode);" in probe
    assert "return original;" in probe
    assert "dik==0x39u && input_mode==0u" in probe
    assert "native_flags&0x100u" in probe
    assert "DEBUG-3D-SPACE-GATE" in probe
    for forbidden in ("SendInput(", "keybd_event(", "ExitProcess(",
                      "TerminateProcess(", "PostMessageA(", "return 1;",
                      "*(volatile DWORD *)(uintptr_t)0x0060CFA4u="):
        assert forbidden not in probe
    install=SRC[SRC.index("static int install_debug_3d_space_gate_probes(void)"):
                SRC.index("/* Instrument all five exact original 3D input CALL sites")]
    assert "memcmp(SPIDER_NATIVE_3D_SPACE_CALL_1" in install
    assert "memcmp(SPIDER_NATIVE_3D_SPACE_CALL_2" in install
    assert "spider_probe_3d_space_gate" in install
    assert "restore_intro_tick_call(SPIDER_NATIVE_3D_SPACE_CALL_1" in install
    assert "restore_intro_tick_call(SPIDER_NATIVE_3D_SPACE_CALL_2" in install
    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert "install_debug_3d_space_gate_probes()" in init
    assert init.count("restore_debug_3d_space_gate_probes();")>=2
    assert "F12 exit guard preserved" in init


def test_native_raw_space_read_traces_caller_without_modifying_return():
    """The real 0x50A650 helper calls 0x501CB0 by exact two 5-byte CALLs.
    Wrappers log only DIK_SPACE and pass along actual native raw result."""
    assert "SPIDER_NATIVE_KEY_EDGE_CALL ((BYTE *)(uintptr_t)0x0050A65Du)" in SRC
    assert "SPIDER_NATIVE_KEY_LEVEL_CALL ((BYTE *)(uintptr_t)0x0050A674u)" in SRC
    assert "spider_native_key_edge_original[5]={0xE8,0x4E,0x76,0xFF,0xFF}" in SRC
    assert "spider_native_key_level_original[5]={0xE8,0x37,0x76,0xFF,0xFF}" in SRC
    assert "SPIDER_NATIVE_KEY_READ ((DWORD (__cdecl *)(DWORD))(uintptr_t)0x00501CB0u)" in SRC
    wrapper=SRC[SRC.index("static DWORD __cdecl spider_probe_native_raw_key("):
                SRC.index("/* PASSIVE owner cleanup checkpoint")]
    assert "DWORD result=SPIDER_NATIVE_KEY_READ(dik);" in wrapper
    assert "return result;" in wrapper
    assert "frame[3]" in wrapper
    assert "frame[4]" in wrapper
    assert "frame[5]" in wrapper
    assert "dik==0x39u" in wrapper
    assert "DEBUG-3D-RAW-SPACE origin=0x%08lX" in wrapper
    assert "raw_code=0x%02lX" in wrapper
    for bad in ("SendInput(", "keybd_event(", "PostMessageA(",
                "ExitProcess(", "TerminateProcess(", "return 1;",
                "*(volatile DWORD *)(uintptr_t)0x0060CFA4u="):
        assert bad not in wrapper
    install=SRC[SRC.index("static int install_debug_raw_space_probes(void)"):
                SRC.index("/* Instrument all five exact original 3D input CALL sites")]
    assert "memcmp(SPIDER_NATIVE_KEY_EDGE_CALL" in install
    assert "memcmp(SPIDER_NATIVE_KEY_LEVEL_CALL" in install
    assert "patch_native_call(SPIDER_NATIVE_KEY_EDGE_CALL" in install
    assert "patch_native_call(SPIDER_NATIVE_KEY_LEVEL_CALL" in install
    assert "spider_probe_native_raw_key" in install
    assert "restore_intro_tick_call(SPIDER_NATIVE_KEY_EDGE_CALL" in install
    assert "restore_intro_tick_call(SPIDER_NATIVE_KEY_LEVEL_CALL" in install
    init=SRC[SRC.index("void SpiderQuickExitF12_Init("):]
    assert "install_debug_raw_space_probes()" in init
    assert init.count("restore_debug_raw_space_probes();")>=2
