"""Prevent ghost-window-induced spurious focus pause using an opt-in Win32 policy."""
from pathlib import Path
import pytest

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
EXAMPLE=(ROOT/"SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")
GENERATOR=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
VALIDATOR=(ROOT/"ini_validation.h").read_text(encoding="utf-8")

def test_optional_process_ghost_suppression_is_off_by_default():
    assert "DisableWindowsGhosting=0" in INI
    assert "DisableWindowsGhosting=0" in EXAMPLE
    assert '"DisableWindowsGhosting=0\\r\\n"' in GENERATOR
    assert '"DisableWindowsGhosting"' in VALIDATOR
    assert 'GetPrivateProfileIntA("Input","DisableWindowsGhosting",0,path)!=1' in CORE

def test_no_foreground_hacks_or_media_changes_in_ghosting_function():
    code=CORE[CORE.index("static void pause_apply_windows_ghosting_option(void)"):
              CORE.index("/* Keep the exact SHA-256 guard.")]
    assert '"DisableProcessWindowsGhosting"' in code
    assert "GetProcAddress(" in code
    assert "disable_ghosting();" in code
    assert 'FOCUS-GHOST: DisableProcessWindowsGhosting CALLED' in code
    assert 'FOCUS-GHOST: normal Windows ghosting enabled' in code
    assert "GetPrivateProfileIntA" in code
    for banned in ("SetForegroundWindow(", "SetActiveWindow(", "SetFocus(",
                   "PostMessage(", "SendMessage(", "AttachThreadInput(",
                   "TerminateThread(", "SuspendThread(", "Sleep("):
        assert banned not in code

def test_only_supported_game_and_validated_ini_can_request_suppression():
    start=CORE.index("static DWORD WINAPI initialize(")
    boot=CORE[start:]
    assert boot.index("supported_exe()") < boot.index("SpiderCfg_ValidateCurrentIni(log_line)")
    assert boot.index("SpiderCfg_ValidateCurrentIni(log_line)") < boot.index("pause_apply_windows_ghosting_option()")
    assert boot.index("pause_apply_windows_ghosting_option()") < boot.index("SpiderDI8Retry_Init(log_line)")
    assert "pause_confirmed_focus_loss(hwnd)" in CORE
    assert "SpiderFocusMedia_Pause()" in CORE
    assert "SpiderFocusDSound_Pause()" in CORE
    assert "SpiderPauseOverlay_Show(hwnd,pause_key_vk)" in CORE
    assert "SpiderPauseAny_Begin(hwnd)" in CORE
