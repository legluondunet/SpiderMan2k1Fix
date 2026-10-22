"""Opt-in F12 clean-shutdown regression checks.

These are source/INI checks; actual WM_CLOSE cleanup and absence of a native
confirmation dialog must also be validated against SpideyPC.exe on Windows/Wine.
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


def test_exit_uses_normal_window_shutdown_and_reports_if_unhandled():
    assert "PostMessageA(hwnd,WM_CLOSE,0,0)" in SRC
    assert "IsWindow(hwnd)" in SRC
    assert "game window destroyed after WM_CLOSE (normal path)" in SRC
    assert "game window still exists after 5s" in SRC
    for forbidden in ("TerminateProcess(", "ExitProcess(", "PostQuitMessage(", "WM_DESTROY,",
                      "SendInput(", "keybd_event(", "SetWindowsHookEx("):
        assert forbidden not in SRC
    assert "QuickExitF12" in HDR


def test_external_windows_watcher_verifies_exit_without_forcing_termination():
    script = (ROOT/"tools/Verify-F12QuickExit.ps1").read_text(encoding="utf-8")
    assert "Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru" in script
    assert "Press F12 in the game window" in script
    assert "$watch.WaitForExit(500)" in script
    assert "$watch.ExitCode" in script
    assert "Get-Process -Id $pidToWatch" in script
    assert 'Get-Process -Name "SpideyPC"' in script
    assert "Get-WinEvent -FilterHashtable" in script
    assert "QUICK-EXIT-F12: F12 pressed in foreground; WM_CLOSE posted" in script
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


def test_bink_intro_exit_uses_original_cleanup_not_window_destruction():
    # The verified EXE's 0x0047081D / 0x0047085F calls tick at 0x0050B5A0.
    # The 0x00470871 continuation calls native movie destructor 0x0050B790.
    # WM_CLOSE before this cleanup hid the window but left video/audio running.
    assert "SPIDER_INTRO_CALL_1" in SRC
    assert "0x0047081Du" in SRC
    assert "0x0047085Fu" in SRC
    assert "0x0050B5A0u" in SRC
    assert "0x006151F8u" in SRC
    assert "0xE8,0x7E,0xAD,0x09,0x00" in SRC
    assert "0xE8,0x3C,0xAD,0x09,0x00" in SRC
    assert "memcmp(SPIDER_INTRO_CALL_1,intro_call_1_original,5)" in SRC
    assert "memcmp(SPIDER_INTRO_CALL_2,intro_call_2_original,5)" in SRC
    assert "static BYTE __cdecl quick_exit_intro_tick(void)" in SRC
    assert "if(InterlockedCompareExchange(&intro_abort_requested,0,0)==1)" in SRC
    assert "return 0; /* original game now jumps to 0x00470871 cleanup */" in SRC
    assert "return SPIDER_INTRO_TICK();" in SRC
    assert "FlushInstructionCache(GetCurrentProcess(),address,5);" in SRC
    assert "restore_intro_call(SPIDER_INTRO_CALL_1,intro_call_1_original);" in SRC
    assert "if(!install_intro_exit_calls())" in SRC
    assert "DISABLED: supported intro Bink CALL signatures unavailable" in SRC


def test_close_occurs_only_after_bink_movie_owner_reports_cleanup():
    active = SRC.index("if(*SPIDER_INTRO_ACTIVE!=0u)")
    drain = SRC.index("*SPIDER_INTRO_ACTIVE==0u",active)
    posted = SRC.index("if(PostMessageA(hwnd,WM_CLOSE,0,0))",drain)
    assert active < drain < posted
    assert "InterlockedExchange(&intro_abort_requested,1);" in SRC
    assert "native intro completed; Bink released by original game" in SRC
    assert "WM_CLOSE NOT posted" in SRC
    assert "movie active; requesting native Bink completion BEFORE WM_CLOSE" in SRC
    assert "game window destroyed; verify process exit independently" in SRC
    assert "BinkClose(" not in SRC  # native owner does cleanup itself
    assert "TerminateProcess(" not in SRC
    assert "ExitProcess(" not in SRC
