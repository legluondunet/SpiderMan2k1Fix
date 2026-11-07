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


def test_native_probe_is_bounded_and_never_assumes_old_vendor_mappings():
    native=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    helper=(ROOT/"native_joystick_swap_probe.h").read_text(encoding="utf-8")
    assert not (ROOT/"native_joystick_reconnect.c").exists()
    assert "native_joystick_reconnect.c" not in (ROOT/"build.sh").read_text()
    assert "spider_native_swap_preset(" not in native
    assert "spider_xbox360_buttons" not in helper
    assert "spider_ps1_buttons" not in helper
    assert "restore_original(" in native
    assert "suspend_original()" in native
