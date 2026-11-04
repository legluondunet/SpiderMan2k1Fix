"""Wine/X11 host focus bridge: opt-in, stale-safe, and native-app key safe.

Static guards. Wine/X11 runtime testing is required before enabling broadly.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
KEYS=(ROOT/"pause_any_key.c").read_text(encoding="utf-8")
HEADER=(ROOT/"pause_any_key.h").read_text(encoding="utf-8")
SCRIPT=(ROOT/"tools/spiderman-x11-focus-bridge.sh").read_text(encoding="utf-8")
BRIDGE=CORE[CORE.index("/* Wine/X11 opt-in host focus bridge."):
            CORE.index("static void pause_at_flip_if_background(void)")]
PAUSE=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
           CORE.index("static HRESULT WINAPI hooked_flip(")]


def test_opt_in_and_file_is_adjacent_to_original_game_exe():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_X11_BRIDGE"' in BRIDGE
    assert "pause_x11_bridge_config=-1" in BRIDGE
    assert "n==1 && value[0]=='1'" in BRIDGE
    assert 'GetModuleFileNameA(NULL,pause_x11_bridge_path' in BRIDGE
    assert 'SpiderMan2k1Fix.x11-focus' in BRIDGE
    assert 'GetFileAttributesExA(pause_x11_bridge_path' in BRIDGE


def test_reader_rejects_old_unknown_and_partially_written_state():
    assert "current.QuadPart>=modified.QuadPart" in BRIDGE
    assert "current.QuadPart-modified.QuadPart<=1200ull*10000ull" in BRIDGE
    assert 'strcmp(line,"SPIDEY_X11_FOCUS_V1 GAME\\n")' in BRIDGE
    assert 'strcmp(line,"SPIDEY_X11_FOCUS_V1 OTHER\\n")' in BRIDGE
    assert "GetFileAttributesExA" in BRIDGE and "ReadFile(" in BRIDGE
    assert "pause_x11_bridge_last_state=state;" in BRIDGE


def test_native_x11_only_overrides_stale_win32_foreground_after_debounce():
    assert "x11_external=pause_x11_bridge_enabled() && x11_state==2;" in PAUSE
    assert "if(win32_game_foreground && !x11_external)" in PAUSE
    assert "pause_x11_bridge_confirmed_loss()" in PAUSE
    assert "pause_confirmed_focus_loss(hwnd)" in PAUSE
    assert "(DWORD)(tick-pause_x11_bridge_loss_since)>=150u" in BRIDGE
    assert "InterlockedCompareExchange(&pause_ever_foreground,0,0)==1" in PAUSE
    assert "InterlockedCompareExchange(&pause_latched,1,0)==0" in PAUSE
    assert "SpiderInputFocus_DiscardDeferredPause();" in PAUSE


def test_never_swallow_native_linux_keys_under_stale_win32_focus():
    assert "SpiderPauseAny_SetHostFocusBlocked(BOOL blocked)" in HEADER
    assert "static volatile LONG host_focus_blocked=0;" in KEYS
    assert "InterlockedCompareExchange(&host_focus_blocked,0,0)==0" in KEYS
    assert "InterlockedCompareExchange(&host_focus_blocked,0,0)!=0" in KEYS
    assert PAUSE.count("SpiderPauseAny_SetHostFocusBlocked(")==2
    assert "pause_x11_bridge_enabled() && x11_state!=1" in PAUSE
    assert "(pause_x11_bridge_enabled() && x11_state!=1)?0:" in PAUSE


def test_unix_helper_does_not_inject_keys_or_activate_windows():
    assert "pgrep -x 'SpideyPC.exe'" in SCRIPT
    assert 'xdotool getwindowfocus getwindowpid' in SCRIPT
    assert "SPIDEY_X11_FOCUS_V1" in SCRIPT
    assert "sleep 0.2" in SCRIPT
    assert 'mv -f -- "$tmp" "$out"' in SCRIPT
    for forbidden in ("xdotool key ", "xdotool windowactivate", "xdotool windowfocus",
                      "xdotool type ", "xdotool mousemove", "xdotool click ",
                      "kill -9 ", "ptrace(", "gdb "):
        assert forbidden not in SCRIPT
