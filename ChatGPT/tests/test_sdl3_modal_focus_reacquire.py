"""Source contracts for SDL3 modal and Alt+Tab secondary-DI8 lifecycle.

Windows/Wine hardware is required to validate the COM behavior. Only the
fix's separate DISCL_FOREGROUND|DISCL_NONEXCLUSIVE device is retired; the
retail game's own DirectInput joystick must never be released or acquired.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def text(path):
    return (ROOT/path).read_text(encoding="utf-8")

def scoped(src, begin, end):
    return src.split(begin,1)[1].split(end,1)[0]

def test_modal_close_preserves_focus_owner_and_retires_secondary_on_return():
    setup=text("sdl3_controller_setup.c")
    ui=text("gamepad_ui_profile.c")
    header=text("gamepad_ui_profile.h")
    assert '#include "gamepad_ui_profile.h"' in setup
    assert "void SpiderGamepadUI_AfterControllerSetup(HWND game_window);" in header
    restoration=scoped(setup,
        'if(setup_report)setup_report("SDL3-SETUP: modal message loop EXIT',
        'if(setup_report)setup_report("SDL3-SETUP: preview closed;')
    assert restoration.index("DestroyWindow(window)") < restoration.index(
        "EnableWindow(owner,TRUE)")
    assert restoration.index("EnableWindow(owner,TRUE)") < restoration.index(
        "SetActiveWindow(owner)")
    assert restoration.index("SetActiveWindow(owner)") < restoration.index(
        "SetForegroundWindow(owner)")
    assert "prior_foreground=GetForegroundWindow()" in restoration
    # Take foreground at WM_CLOSE while owned modal is still a valid
    # foreground HWND, not only AFTER DestroyWindow (which can activate
    # Explorer without any user Alt+Tab).
    close=scoped(setup,"case WM_CLOSE:", "case WM_DESTROY:")
    assert "ui->closing_foreground=GetForegroundWindow();" in close
    assert "ui->closing_foreground_captured=1u;" in close
    assert close.index("ui->closing_foreground=GetForegroundWindow();") < close.index(
        "DestroyWindow(hwnd);")
    assert "restore_allowed=spider_sdl3_setup_restore_focus(" in restoration
    assert "ui.closing_foreground_captured" in restoration
    assert "ui.closing_foreground==owner || ui.closing_foreground==window" in restoration
    assert "prior_foreground==owner || prior_foreground==window" in restoration
    assert "ui.closing_foreground==NULL,prior_foreground==NULL" in restoration
    assert "preclose=%p" in restoration
    assert "has_quit" in restoration
    assert "if(GetForegroundWindow()==owner)SetFocus(owner);" in restoration
    assert "SpiderGamepadUI_AfterControllerSetup(owner)" in restoration
    assert restoration.index("SetForegroundWindow(owner)") < restoration.index(
        "SpiderGamepadUI_AfterControllerSetup(owner)")
    assert "AttachThreadInput" not in restoration
    assert "SDL3-SETUP: owner focus restore" in restoration
    # There is no second controller-owned DirectInput COM handle now.
    # The SDL3 modal restores original native focus; the former cleanup
    # adapter cannot reopen/release a parallel Sony/Xbox device.
    assert "void SpiderGamepadUI_AfterControllerSetup(HWND hwnd) {(void)hwnd;}" in ui
    assert "IDirectInput8_CreateDevice(" not in ui
    assert "IDirectInputDevice8_Acquire(" not in ui
    assert "IDirectInputDevice8_Release(" not in ui
    assert "SpiderSDL3Universal_BeforeModal(owner);" in setup

def test_external_focus_loss_releases_universal_keys_without_secondary_com():
    ui=text("gamepad_ui_profile.c")
    adapter=text("sdl3_universal_gameplay.c")
    core=text("spiderman2k1fix.c")
    assert "disable_profile_with_policy(" not in ui
    assert "sdl3_ui_instance" not in ui
    assert "begin_scancode(" not in ui
    assert "SpiderSDL3Universal_OnConfirmedFocusPause();" in core
    assert 'restore_original("focus or window lost");' in adapter
    assert "native_host_focus_blocked" in adapter
    assert "release_all()" in adapter
    assert "SpiderSDL3Universal_SetHostFocusBlocked(" in adapter

def test_next_foreground_flip_claims_only_explicit_selected_sdl3_pad():
    ui=text("gamepad_ui_profile.c")
    probe=text("sdl3_gamepad_probe.c")
    adapter=text("sdl3_universal_gameplay.c")
    assert not (ROOT/"native_joystick_reconnect.c").exists()
    assert "IDirectInput8_CreateDevice(" not in ui
    assert "SpiderNativeReconnect_OnFlip(" not in ui
    assert "SpiderSDL3Probe_SelectedMappedSample(" in adapter
    assert "if(!suspended && !spider_sdl3_universal_neutral(&sample))" in adapter
    assert "SpiderSDL3Probe_GenericSelectedSample(" in probe
    assert "spider_sdl3_selection_focus_lost(&selection)" in probe
    assert "spider_sdl3_selected_model_unique(" in probe
    assert "SpiderNativeReconnect_" not in probe
