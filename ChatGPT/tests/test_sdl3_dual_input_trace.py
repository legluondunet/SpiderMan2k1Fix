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


def test_native_rebind_probe_is_removed_without_disabling_read_only_sdl3_trace():
    native=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    helper=(ROOT/"sdl3_controller_profile.h").read_text(encoding="utf-8")
    assert not (ROOT/"native_joystick_swap_probe.h").exists()
    assert not (ROOT/"native_joystick_reconnect.c").exists()
    assert "SpiderNativeReconnect_" not in native
    assert "sdl3_binding_trace_on_flip();" in native
    assert "sdl3_trace_native_owner(now);" in native
    assert "spider_sdl3_profile_valid(" in helper

