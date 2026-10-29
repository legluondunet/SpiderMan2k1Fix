"""Regression contracts for returning from the SDL3 controller setup modal.

No hardware COM/Win32 calls run in host pytest. The Windows x86 compiler
validates the actual sources. These checks prevent accidentally restoring
the pre-fix behavior: foreground-only devices were discarded after three
DIERR_NOTACQUIRED reads following the owned modal dialog.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def text(path):
    return (ROOT/path).read_text(encoding="utf-8")


def scoped(src, begin, end):
    return src.split(begin,1)[1].split(end,1)[0]


def test_modal_close_first_restores_original_foreground_then_arms_one_shot_recovery():
    setup=text("sdl3_controller_setup.c")
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
    assert "prior_foreground==NULL" in restoration
    assert "prior_foreground==owner" in restoration
    assert "prior_foreground==window" in restoration
    assert "restore_allowed=!has_quit" in restoration
    assert "IsWindowVisible(owner)" in restoration
    assert "if(GetForegroundWindow()==owner)SetFocus(owner);" in restoration
    assert "SpiderGamepadUI_AfterControllerSetup(owner)" in restoration
    assert restoration.index("SetForegroundWindow(owner)") < restoration.index(
        "SpiderGamepadUI_AfterControllerSetup(owner)")
    assert "GetGUIThreadInfo" not in restoration or "AttachThreadInput" not in restoration
    assert "SDL3-SETUP: owner focus restore" in restoration


def test_secondary_di8_reacquire_only_after_own_dialog_and_matching_game_owner():
    ui=text("gamepad_ui_profile.c")
    init=scoped(ui,"void SpiderGamepadUI_AfterControllerSetup(HWND game_window) {",
                  "static void detect_device(HWND hwnd)")
    assert "modal_reacquire_disarm()" in init
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" in init
    assert "SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&instance)" in init
    assert "instance!=sdl3_ui_instance" in init
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in init
    assert "modal_reacquire_device=device;" in init
    assert "modal_reacquire_hwnd=game_window;" in init
    assert "modal_reacquire_pending=1;" in init
    assert "spider_ds4_hotplug_focus_lost(&hotplug)" in init
    assert "IDirectInputDevice8_Acquire(" not in init
    assert "IDirectInputDevice8_Release(" not in init

    on_flip=scoped(ui,"void SpiderGamepadUI_OnFlip(", "if(sdl3_3d_trace_requested &&")
    # Reacquire ONLY for DIERR_NOTACQUIRED, NEVER an unplug or INPUTLOST.
    assert "permitted=hr==DIERR_NOTACQUIRED" in on_flip
    assert "modal_reacquire_device==device" in on_flip
    assert "modal_reacquire_hwnd==hwnd" in on_flip
    assert "SpiderSDL3Probe_SelectedIdentity(&selected_vid,&selected_pid," in on_flip
    assert "selected_id==sdl3_ui_instance" in on_flip
    assert "SpiderNativeReconnect_SDL3SelectionReady(selected_vid,selected_pid)" in on_flip
    assert "modal_reacquire_disarm(); /* one attempt MAX" in on_flip
    assert on_flip.count("IDirectInputDevice8_Acquire(device)") == 1
    assert on_flip.index("modal_reacquire_disarm(); /* one attempt MAX") < on_flip.index(
        "IDirectInputDevice8_Acquire(device)")
    assert on_flip.index("IDirectInputDevice8_Acquire(device)") < on_flip.index(
        "spider_ds4_hotplug_read_failed(&hotplug)")
    assert "if(SUCCEEDED(acquire_hr))" in on_flip
    assert "bad_reads=0;" in on_flip
    assert "spider_ds4_hotplug_focus_lost(&hotplug)" in on_flip
    assert "spider_sdl3_context_menu_reset(&sdl3_context_bridge)" in on_flip
    assert "DIERR_INPUTLOST || repeatedly_failed" in on_flip
    assert 'modal_reacquire_disarm();\n    report("SDL3-SETUP: secondary UI DirectInput survived modal' in ui


def test_no_native_game_com_handoff_or_unsafe_foreground_stealing():
    setup=text("sdl3_controller_setup.c")
    ui=text("gamepad_ui_profile.c")
    closing=scoped(setup,"prior_foreground=GetForegroundWindow()", "setup_open=0;")
    for forbidden in ("AttachThreadInput","SetWindowPos","BringWindowToTop",
                      "SetCapture","AllocConsole","Sleep("):
        assert forbidden not in closing
    handler=scoped(ui,"void SpiderGamepadUI_AfterControllerSetup(HWND game_window) {",
                   "static void detect_device(HWND hwnd)")
    for forbidden in ("GetDeviceData","SpiderNativeReconnect_OnFlip(",
                      "DirectInput8Create","SetCooperativeLevel"):
        assert forbidden not in handler
    # One-sided secondary COM release on definite input loss remains intact.
    assert "IDirectInputDevice8_Release(old);" in ui
    assert "spider_hotplug_drop_device(hr," in ui
    # Optional experiment remains off when absent from the user's INI.
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3ControllerSetup",0,ini)==1' in setup
