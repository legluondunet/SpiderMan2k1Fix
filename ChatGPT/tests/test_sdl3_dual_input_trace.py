"""Regression contracts for opt-in SDL3/DI8 dual-view input diagnostics.

This records correlations, not native game-owned joystick mapping evidence.
No native COM polling, no input injection, no unsupported binding enabled.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def test_dual_view_samples_only_selected_verified_sdl3_owner():
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.h").read_text(encoding="utf-8")
    start=ui.index("static void sdl3_trace_3d_readonly(const DIJOYSTATE2 *state)")
    end=ui.index("void SpiderGamepadUI_OnFlip(",start)
    fn=ui[start:end]
    assert "sdl3_3d_trace_requested" in fn
    assert "sdl3_3d_trace_lines>=64u" in fn
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" in fn
    assert "SpiderSDL3Probe_SelectedSample(" in fn
    assert "selected_id==sdl3_ui_instance" in fn
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in fn
    assert "if(!selected_ok)return;" in fn
    assert "selected.buttons!=sdl3_3d_trace_sdl_buttons" in fn
    assert "trigger_edges!=sdl3_3d_trace_trigger_edges" in fn
    assert "state->rgdwPOV[0]!=sdl3_3d_trace_pov" in fn
    assert "selected.axis[SPIDER_SDL3_AXIS_LT]>=11000" in fn
    assert "selected.axis[SPIDER_SDL3_AXIS_RT]>=11000" in fn
    assert "state->rglSlider[0]" in fn and "state->rglSlider[1]" in fn
    assert "SDL-buttons=0x%08lX LT=%d RT=%d" in fn
    assert "DI-buttons=0x%04lX" in fn
    assert "native game mapping NOT inferred" in fn
    assert "selected.buttons" in probe or "SpiderSDL3ControllerSample" in probe
    for forbidden in ("IDirectInputDevice8_Poll(", "IDirectInputDevice8_Acquire(",
                      "IDirectInputDevice8_GetDeviceState(", "SendInput(",
                      "VirtualProtect(", "WriteProcessMemory("):
        assert forbidden not in fn


def test_pause_fail_closed_legacy_secondary_device_retires_once_on_end():
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    begin=ui.split("int SpiderGamepadUI_PauseWake_Begin(void) {",1)[1].split(
        "unsigned SpiderGamepadUI_PauseWake_Poll(",1)[0]
    poll=ui.split("unsigned SpiderGamepadUI_PauseWake_Poll(",1)[1].split(
        "void SpiderGamepadUI_PauseWake_End(",1)[0]
    end=ui.split("void SpiderGamepadUI_PauseWake_End(void) {",1)[1].split(
        "static void xbox_trace_trigger_axes(",1)[0]
    assert "pause_wake_retire_pending=0;" in begin
    assert "pause_wake_retire_inputlost=0;" in begin
    assert "pause_wake_retire_pending=1;" in poll
    assert "pause_wake_retire_inputlost=hr==DIERR_INPUTLOST;" in poll
    assert poll.index("if(SUCCEEDED(acquire_hr))return 0;") < poll.index(
        "pause_wake_retire_pending=1;")
    assert "if(pause_wake_retire_pending && device)" in end
    assert "disable_profile_with_policy(" in end
    assert "pause_wake_retire_inputlost);" in end
    assert "sdl3_ui_instance=0;" in end
    assert "probing=0;" in end
    assert "hotplug.scan_started=0u;" in end
    assert "pause_wake_retire_pending=0;" in end
    assert "pause_wake_retire_inputlost=0;" in end
    for forbidden in ("IDirectInputDevice8_Acquire(", "IDirectInput8_CreateDevice(",
                      "SpiderNativeReconnect_OnFlip(", "SetForegroundWindow("):
        assert forbidden not in end
    assert "GAMEPAD-PAUSE-END: retired stale secondary UI DirectInput" in end
