"""Auto-started Wine/X11 focus backend, native observer and fallback guards.

Source-level checks only; CI also compiles ELF + PE and native Wine tests
must confirm the launcher/permission and pause paths at runtime.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BOOT=(ROOT/"wine_focus_autostart.c").read_text()
FOCUS=(ROOT/"input_focus_trace.c").read_text()
PAUSE=(ROOT/"pause_any_key.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
WORKFLOW=(ROOT/".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()
NATIVE=(ROOT/"linux/spiderman-focus-helper.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()


def test_deferred_helper_handshake_reactivates_core_detector_after_first_flip():
    # First Flip can cache disabled while the ELF starts. Later a verified
    # GAME/OTHER sample must be able to turn bridge config 0 -> 1.
    assert 'if(pause_x11_bridge_config<=0)' in CORE
    assert "if(pause_x11_bridge_config==0 && !(n==1 && value[0]=='1'))" in CORE
    assert 'SpiderWineFocusAutostart_PollReady(focus_log);' in FOCUS
    assert 'pause_x11_bridge_config=n==1 && value[0]==' in CORE
    # Manual opt-in works immediately; successful automatic activation
    # happens only after PollReady saw a fresh, valid state.
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE"' in CORE


def test_autostart_is_wine_only_on_first_native_flip_and_not_in_dllmain():
    assert 'GetProcAddress(ntdll,"wine_get_version")' in BOOT
    assert 'GetProcAddress(ntdll,"__wine_unix_spawnvp")' in BOOT
    assert 'GetProcAddress(kernel,"wine_get_unix_file_name")' in BOOT
    assert 'SpiderWineFocusAutostart_TryStart(focus_log);' in FOCUS
    assert 'InterlockedCompareExchange(&native_bridge_autostarted,1,0)==0' in FOCUS
    assert FOCUS.index('SpiderWineFocusAutostart_TryStart(focus_log);') < FOCUS.index('if(!diagnostic_active || !tracked_window) return;',FOCUS.index('void SpiderInputFocus_OnFlip'))
    assert 'wine_focus_autostart.c quick_exit_f12.c' in BUILD


def test_no_shell_execution_and_existing_override_is_preserved():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE"' in BOOT
    assert 'if(requested || GetLastError()!=ERROR_ENVVAR_NOT_FOUND)return 0;' in BOOT
    assert 'SetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE","1")' in BOOT
    assert 'argv[1]=game_unix;' in BOOT
    assert 'argv[0]=helper_unix;' in BOOT
    assert 'spawn(argv,0)' in BOOT
    assert 'spawn(chmod_argv,1)' in BOOT
    assert '"/usr/bin/chmod","u+x","--",helper_unix,NULL' in BOOT
    for forbidden in ("system(", "ShellExecute", "CreateProcess", "/bin/sh", "bash -c"):
        assert forbidden not in BOOT


def test_bridge_waits_for_verified_recent_state_before_opt_in():
    assert 'SpiderWineFocusAutostart_PollReady(focus_log);' in FOCUS
    assert 'static int helper_waiting_for_state=0;' in BOOT
    assert 'helper_waiting_for_state=1;' in BOOT
    assert 'GetFileAttributesExW(helper_state_path' in BOOT
    assert 'modified.QuadPart<started.QuadPart' in BOOT
    assert 'now.QuadPart-modified.QuadPart>1200ull*10000ull' in BOOT
    assert 'if(!valid)return 0;' in BOOT
    assert 'SPIDEY_X11_FOCUS_V1 GAME\\n' in BOOT
    assert 'SPIDEY_X11_FOCUS_V1 OTHER\\n' in BOOT
    assert BOOT.index('if(!valid)return 0;') < BOOT.index('SetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE","1")')
    assert 'no verified X11 state in 4s; Win32 fallback' in BOOT


def test_wayland_does_not_autostart_or_publish_fictional_x11_focus():
    assert 'GetEnvironmentVariableA("XDG_SESSION_TYPE"' in BOOT
    assert 'lstrcmpiA(session,"wayland")==0' in BOOT
    assert 'GetEnvironmentVariableA("WAYLAND_DISPLAY",NULL,0)' in BOOT
    assert BOOT.index('Wayland session; X11 backend disabled') < BOOT.index('spawn(argv,0)')
    assert 'strcasecmp(session, "wayland") == 0' in NATIVE
    assert 'wayland_socket = getenv("WAYLAND_DISPLAY")' in NATIVE
    assert 'if ((!session || !*session) && wayland_socket && *wayland_socket)' in NATIVE


def test_native_helper_reads_x11_without_typing_or_raising_windows():
    assert 'XOpenDisplay(NULL)' in NATIVE
    assert 'XGetInputFocus(display, &focused, &revert_to)' in NATIVE
    assert '"_NET_WM_PID"' in NATIVE
    assert 'XGetWindowProperty' in NATIVE
    assert 'XQueryTree' in NATIVE
    assert 'find_unique_game()' in NATIVE
    assert 'strcmp(name, "SpideyPC.exe") == 0' in NATIVE
    assert 'SPIDEY_X11_FOCUS_V1 UNKNOWN\\n' in NATIVE
    assert 'SPIDEY_X11_FOCUS_V1 GAME\\n' in NATIVE
    assert 'SPIDEY_X11_FOCUS_V1 OTHER\\n' in NATIVE
    assert 'rename(tmp, path)' in NATIVE
    assert 'unlink(state_file)' in NATIVE
    for forbidden in ("XSetInputFocus(", "XWarpPointer(", "XTestFakeKeyEvent",
                      "XSendEvent(", "system(", "popen(", "ptrace("):
        assert forbidden not in NATIVE


def test_ci_ships_elf_directly_inside_flat_install_artifact_and_release_zip():
    assert 'libx11-dev' in WORKFLOW
    assert 'linux/spiderman-focus-helper.c -lX11' in WORKFLOW
    assert WORKFLOW.count('SpiderMan2k1Fix/linux/spiderman-focus-helper') >= 6
    assert 'SpiderMan2k1Fix-Windows-x86' in WORKFLOW


def test_pause_can_recover_when_helper_disappears():
    assert 'host_bridge_unavailable_for_long()' in PAUSE
    assert 'if(InterlockedCompareExchange(&host_focus_blocked,0,0)!=0 &&' in PAUSE
    assert 'now.QuadPart-modified.QuadPart<=1200ull*10000ull' in PAUSE
    assert 'return (DWORD)(tick-host_bad_since)>=2200u;' in PAUSE
    assert 'host_bad_since=0;' in PAUSE
    assert 'SPIDEY_X11_FOCUS_V1 OTHER\\n' in PAUSE
