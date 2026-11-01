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
    after=scoped(ui,"void SpiderGamepadUI_AfterControllerSetup(HWND game_window) {",
        "static void detect_device(HWND hwnd)")
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" in after
    assert "disable_profile_with_policy(" in after
    assert "retired separate foreground-only UI DirectInput device after modal" in after
    assert "sdl3_ui_instance=0;" in after
    assert "probing=0;" in after
    assert "hotplug.scan_started=0u;" in after
    assert after.index("spider_ds4_hotplug_focus_lost(&hotplug)") < after.index(
        "disable_profile_with_policy(")
    for forbidden in ("IDirectInputDevice8_Acquire(", "IDirectInput8_CreateDevice(",
                      "SetForegroundWindow(", "SpiderNativeReconnect_OnFlip(",
                      "GetDeviceData(", "DirectInput8Create("):
        assert forbidden not in after

def test_external_focus_loss_retires_only_verified_sdl3_secondary_com():
    ui=text("gamepad_ui_profile.c")
    main=scoped(ui,"void SpiderGamepadUI_OnFlip(", "  /* An SDL3 selected canonical native map")
    focus=main.split("if(GetForegroundWindow()!=hwnd || !IsWindowVisible(hwnd)) {",1)[1]
    assert "SpiderSDL3Probe_NativeOwnerEnabled() && device &&" in focus
    assert "sdl3_ui_instance!=0u" in focus
    assert "disable_profile_with_policy(" in focus
    assert "retired separate foreground-only UI DirectInput on focus loss" in focus
    assert "sdl3_ui_instance=0;" in focus
    assert "probing=0;" in focus
    assert "hotplug.scan_started=0u;" in focus
    assert focus.index("disable_profile_with_policy(") < focus.index("return;")
    for forbidden in ("IDirectInputDevice8_Acquire(", "IDirectInput8_CreateDevice(",
                      "SpiderNativeReconnect_OnFlip(", "SetForegroundWindow("):
        assert forbidden not in focus

def test_next_foreground_flip_must_verify_original_native_owner_and_recreate():
    ui=text("gamepad_ui_profile.c")
    flip=scoped(ui,"void SpiderGamepadUI_OnFlip(", "  /* If this Xbox was attached")
    assert "SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id)" in flip
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in flip
    assert "if(id!=sdl3_ui_instance)" in flip
    assert "detect_device(hwnd);" in flip
    detect=scoped(ui,"static void detect_device(HWND hwnd) {",
                  "/* This is the ORIGINAL, game-owned CutSceneSkipAllow")
    assert "DISCL_FOREGROUND|DISCL_NONEXCLUSIVE" in detect
    assert "IDirectInput8_CreateDevice(di,choose_ds4?" in detect
    assert "spider_ds4_hotplug_attached(&hotplug)" in detect
    assert "sdl3_ui_instance=sdl_owner?selected_id:0u;" in detect
    assert "SpiderNativeReconnect_SDL3SelectionReady(" in detect
    flip_remainder=ui.split("void SpiderGamepadUI_OnFlip(",1)[1]
    failed=scoped(flip_remainder,
                  "  hr=IDirectInputDevice8_GetDeviceState(device,sizeof(state),&state);",
                  "  if(sdl3_3d_trace_requested &&")
    assert "IDirectInputDevice8_Acquire(" not in failed
    assert "modal_reacquire" not in ui
    assert "spider_hotplug_drop_device(hr," in failed
    assert "hr==DIERR_INPUTLOST || repeatedly_failed" in failed
    assert "IDirectInputDevice8_Release(old);" in ui
    assert "if(already_inputlost)" in ui
