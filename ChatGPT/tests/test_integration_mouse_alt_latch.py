"""Combined experimental build: independently guarded Alt latch and permanently disabled game mouse."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
INI=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
SRC=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
UI=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
MOUSE=(ROOT/"ps1_mouse_mode.c").read_text(encoding="utf-8")
ALT=(ROOT/"alt_latch_recovery.h").read_text(encoding="utf-8")

def test_combined_opt_in_alt_and_permanent_mouse_coexist():
    assert '"ExperimentalAltLatchRecovery"' in INI
    assert '"MouseEnabled"' in INI
    assert '"ControllerAutoDisableMouse"' in INI
    assert '"SDL3NativeSourceTrace"' in INI
    assert '#include "alt_latch_recovery.h"' in SRC
    assert 'SpiderAltLatch_Init(ini,log_line);' in SRC
    assert 'SpiderAltLatch_OnConfirmedExternalLoss();' in SRC
    assert 'SpiderPS1Mouse_Configure(logger);' in UI
    assert 'GetPrivateProfileIntA("Input","MouseEnabled"' not in UI
    assert 'obsolete MouseEnabled option ignored; game mouse is permanently OFF' in UI
    assert 'SpiderPS1Mouse_EnsureDisabled();' in MOUSE
    assert 'GAME MOUSE RESTORED' not in MOUSE
    assert 'ExperimentalAltLatchRecovery' in ALT
    assert 'MouseEnabled=1 is always IGNORED' in INI

def test_separate_native_scope_and_no_mouse_reenable():
    assert 'SPIDER_GAME_MOUSE_DISABLED 0x10u' in MOUSE
    assert 'before&~SPIDER_GAME_MOUSE_DISABLED' not in MOUSE
    assert 'ALT-LATCH: REPAIRED' in ALT
    assert 'GetAsyncKeyState(VK_TAB)' in ALT
    assert 'GetAsyncKeyState(VK_RMENU)' in ALT
