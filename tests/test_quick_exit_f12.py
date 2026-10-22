"""Opt-in F12 clean-shutdown regression checks.

These are source/INI checks; actual native engine cleanup still requires Windows testing.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
SRC = (ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
HDR = (ROOT/"quick_exit_f12.h").read_text(encoding="utf-8")
VAL = (ROOT/"ini_validation.h").read_text(encoding="utf-8")
GEN = (ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
INI = (ROOT/"SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")
BUILD = (ROOT/"build.sh").read_text(encoding="utf-8")


def test_f12_exits_only_when_explicitly_opted_in_in_system_section():
    assert "QuickExitF12=0" in INI
    assert '"[System]\\r\\n"' in GEN
    assert '"QuickExitF12=0\\r\\n"' in GEN
    assert '"Debug","System","Display"' in VAL
    assert 'spider_ini_system[]={"QuickExitF12"}' in VAL
    assert 'spider_ini_icmp(section,"System")' in VAL
    assert 'GetPrivateProfileStringA("System","QuickExitF12","0"' in SRC
    assert 'if(strcmp(value,"1")!=0)' in SRC
    assert "InterlockedExchange(&enabled,1);" in SRC


def test_foreground_edge_trigger_is_independent_of_flip_and_directinput():
    assert "static DWORD WINAPI quick_exit_f12_thread(LPVOID unused)" in SRC
    assert "GetAsyncKeyState(VK_F12)&0x8000" in SRC
    assert "GetForegroundWindow()==hwnd" in SRC
    assert "previous_down=TRUE;" in SRC
    assert "previous_down=FALSE;" in SRC
    assert "InterlockedCompareExchange(&closing,1,0)==0" in SRC
    assert "Sleep(25);" in SRC
    assert "SpiderQuickExitF12_AttachWindow(hwnd);" in CORE
    assert "SpiderQuickExitF12_Init(log_line);" in CORE
    assert "SpiderQuickExitF12_Enabled()" in CORE
    assert "quick_exit_f12.c" in BUILD
    assert "SetWindowLongPtr(" not in SRC  # no subclassing


def test_native_quit_flag_replaces_unsafe_wm_close():
    for s in ("0x0060CF88u", "0x004A238Au", "0x00455E50u",
              "0xC6,0x05,0x88,0xCF,0x60,0x00,0x01",
              "0x38,0x1D,0x88,0xCF,0x60,0x00"):
        assert s in SRC
    assert "*SPIDER_GAME_QUIT_FLAG=1u;" in SRC
    assert "game-native quit requested flag=1" in SRC
    assert "memcmp(SPIDER_NATIVE_QUIT_SETTER,game_quit_set_signature,7)" in SRC
    assert "memcmp(SPIDER_NATIVE_QUIT_CHECK,game_quit_check_signature,6)" in SRC
    assert "PostMessageA(hwnd,WM_CLOSE,0,0)" not in SRC
    for forbidden in ("TerminateProcess(", "ExitProcess(", "DestroyWindow(",
                      "PostQuitMessage(", "SendInput(", "SetWindowsHookEx("):
        assert forbidden not in SRC


def test_external_windows_watcher_verifies_exit_without_forcing_termination():
    script = (ROOT/"tools/Verify-F12QuickExit.ps1").read_text(encoding="utf-8")
    assert "Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru" in script
    assert "Press F12 in the game window" in script
    assert "$watch.WaitForExit(500)" in script
    assert "$watch.ExitCode" in script
    assert "Get-Process -Id $pidToWatch" in script
    assert 'Get-Process -Name "SpideyPC"' in script
    assert "Get-WinEvent -FilterHashtable" in script
    assert "QUICK-EXIT-F12: game-native quit requested flag=1" in script
    assert "OS-owned process memory and handles are released" in script
    assert "this does not prove that every native COM object was explicitly released" in script
    assert "Stop-Process" not in script
    assert ".Kill(" not in script
    assert "TerminateProcess" not in script


def test_readme_identifies_exact_verified_original_f12_route():
    readme = (ROOT/"README.md").read_text(encoding="utf-8")
    assert "VA 0x00515368" in readme
    assert "VA 0x00515336" in readme
    assert "VA 0x00515306" in readme
    assert "tools/Verify-F12QuickExit.ps1" in readme


def test_native_f12_wndproc_is_signature_gated_and_bypassed_without_subclass():
    # Verified original SpideyPC.exe WM_KEYDOWN -> VK_F12 jump table
    # lands at 0x515368 and posts WM_CLOSE at 0x515373.
    assert "#define SPIDER_NATIVE_F12_POST ((BYTE *)(uintptr_t)0x00515373u)" in SRC
    assert "0xFF,0x15,0x70,0xB1,0x53,0x00" in SRC
    assert "0x83,0xC4,0x10,0x90,0x90,0x90" in SRC
    assert "memcmp(SPIDER_NATIVE_F12_POST,native_f12_original,6)" in SRC
    assert "native_f12_suppress_unsafe_window_close()" in SRC
    assert "native_f12_restore();" in SRC
    assert "FlushInstructionCache(GetCurrentProcess(),SPIDER_NATIVE_F12_POST,6);" in SRC
    assert "SetWindowLongPtr(" not in SRC


def test_bink_intro_exit_uses_original_cleanup_not_window_destruction():
    # SHA-pinned Bink tick CALLs in original game:
    # 0x47081d and 0x47085f call 0x50b5a0; native cleanup at 0x470871.
    for marker in ("0x0047081Du","0x0047085Fu","0x0050B5A0u",
                   "0x006151F8u", "0xE8,0x7E,0xAD,0x09,0x00",
                   "0xE8,0x3C,0xAD,0x09,0x00"):
        assert marker in SRC
    assert "memcmp(SPIDER_INTRO_CALL_1,intro_call_1_original,5)" in SRC
    assert "memcmp(SPIDER_INTRO_CALL_2,intro_call_2_original,5)" in SRC
    assert "static BYTE __cdecl quick_exit_intro_tick(void)" in SRC
    assert "if(InterlockedCompareExchange(&intro_abort_requested,0,0)==1)" in SRC
    assert "return 0; /* original game now jumps to 0x00470871 cleanup */" in SRC
    assert "return SPIDER_INTRO_TICK();" in SRC
    assert "FlushInstructionCache(GetCurrentProcess(),address,5);" in SRC
    assert "restore_intro_call(SPIDER_INTRO_CALL_1,intro_call_1_original);" in SRC
    assert "if(!install_intro_exit_calls())" in SRC
    assert "DISABLED: native F12/Bink call signatures unavailable" in SRC


def test_bink_safe_native_exit_and_video_frame_observation():
    assert "void SpiderQuickExitF12_ObserveMovieFrame(void)" in HDR
    assert "SpiderQuickExitF12_ObserveMovieFrame();" in CORE
    assert "InterlockedIncrement(&intro_movie_blt_count);" in SRC
    assert "InterlockedExchange(&intro_abort_requested,1);" in SRC
    assert "*SPIDER_GAME_QUIT_FLAG=1u;" in SRC
    assert "BOOL movie=movie_frame_recent(now,1500u)" in SRC
    assert "native movie frames stopped; waiting for game's Quit cleanup" in SRC
    assert "window kept intact, NO WM_CLOSE" in SRC
    assert "return 0; /* original game now jumps to 0x00470871 cleanup */" in SRC
    assert "native_f12_suppress_unsafe_window_close()" in SRC
    assert "native_f12_restore();" in SRC
    assert "if(PostMessageA(hwnd,WM_CLOSE,0,0))" not in SRC
