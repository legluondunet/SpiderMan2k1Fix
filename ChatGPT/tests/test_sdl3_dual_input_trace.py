"""Retired raw dual-input comparative tracer must not claim a fake Xbox map.

The game-owned native joystick mapping stays untouched; diagnostics
continue to inspect the original game via the shared native integrity
probe. SDL3 logical per-model profiles drive opt-in gameplay.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def test_legacy_secondary_directinput_view_is_removed():
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.h").read_text(encoding="utf-8")
    assert "sdl3_trace_3d_readonly(" not in ui
    assert "IDirectInputDevice8_Acquire(" not in ui
    assert "IDirectInputDevice8_GetDeviceState(" not in ui
    assert "begin_scancode(" not in ui
    assert "SpiderSDL3ControllerSample" in probe


def test_retired_native_probe_cannot_access_device_or_action_table():
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    for forbidden in ("observe_native_swap_table(", "spider_native_swap_read(",
                      "VirtualQuery(", "EnumDevices(", "CreateDevice(",
                      "native-table=%s buttons[7]=", "IDirectInputDevice8_"):
        assert forbidden not in native
