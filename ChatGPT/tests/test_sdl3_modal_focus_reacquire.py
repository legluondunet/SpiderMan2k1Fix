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
    guide=text("sdl3_controller_setup.c")
    ui=text("gamepad_ui_profile.c")
    header=text("gamepad_ui_profile.h")
    assert '#include "gamepad_ui_profile.h"' in guide
    assert "void SpiderGamepadUI_AfterControllerSetup(HWND game_window);" in header
    restored=guide.split("if(IsWindow(window))DestroyWindow(window);",1)[1]
    assert restored.index("DestroyWindow(window)") < restored.index(
        "EnableWindow(owner,TRUE)") if "DestroyWindow(window)" in restored else (
        "if(parent_enabled && IsWindow(owner))EnableWindow(owner,TRUE);" in restored
    )
    assert restored.index("EnableWindow(owner,TRUE)") < restored.index(
        "SetActiveWindow(owner)")
    assert restored.index("SetActiveWindow(owner)") < restored.index(
        "SetForegroundWindow(owner)")
    assert "previous=GetForegroundWindow()" in restored
    close=scoped(guide,"case WM_CLOSE:", "case WM_NCDESTROY:")
    assert "ui->closing_foreground=GetForegroundWindow();" in close
    assert "ui->closing_captured=1;" in close
    assert close.index("ui->closing_foreground=GetForegroundWindow();") < close.index(
        "DestroyWindow(hwnd);")
    assert "restore_allowed=spider_sdl3_setup_restore_focus(" in restored
    assert "ui.closing_captured" in restored
    assert "preclose_owned=ui.closing_foreground==owner" in restored
    assert "previous_owned=previous==owner" in restored
    assert "ui.closing_foreground==NULL,previous==NULL" in restored
    assert "has_quit" in restored
    assert "SpiderGamepadUI_AfterControllerSetup(owner)" in restored
    assert restored.index("SetForegroundWindow(owner)") < restored.index(
        "SpiderGamepadUI_AfterControllerSetup(owner)")
    assert "AttachThreadInput" not in restored
    assert "void SpiderGamepadUI_AfterControllerSetup(HWND hwnd) {(void)hwnd;}" in ui
    assert "IDirectInput8_CreateDevice(" not in ui
    assert "IDirectInputDevice8_Acquire(" not in ui
    assert "SpiderSDL3Universal_BeforeModal(owner);" in guide

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

def test_next_foreground_flip_claims_only_safe_selected_sdl3_pad():
    ui=text("gamepad_ui_profile.c")
    probe=text("sdl3_gamepad_probe.c")
    adapter=text("sdl3_universal_gameplay.c")
    native=text("native_joystick_reconnect.c")
    assert "IDirectInput8_CreateDevice(" not in ui
    assert "SpiderNativeReconnect_OnFlip(" not in ui
    assert "SpiderSDL3Probe_SelectedMappedSample(" in adapter
    assert "if(!suspended && !spider_sdl3_universal_owner_neutral(&sample) &&" in adapter
    assert "SpiderSDL3Probe_GenericSelectedSample(" in probe
    assert "spider_sdl3_selection_focus_lost(&selection)" in probe
    assert "spider_sdl3_selection_singleton_candidate(&selection)" in probe
    assert "spider_sdl3_profile_neutral(&sample)" in probe
    assert "start_claims.count==1u" in probe
    assert "CreateDevice(" not in native
    assert "swap_native_gameplay_mapping(" not in native
