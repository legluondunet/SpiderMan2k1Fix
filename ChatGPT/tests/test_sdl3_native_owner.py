"""Native joystick recovery must not smuggle old Sony/Xbox button maps.

Original game-owned DirectInput controller lifetime is still guarded.
The actual SDL3 universal profile does not depend on physical button
indices of the hand-tested 045E:028E / 054C:09CC devices.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def _read(path):
    return (ROOT/path).read_text(encoding="utf-8")


def test_legacy_native_owner_cannot_be_enabled_as_sdl3_fallback():
    ini=_read("virtual_spidey_cfg.c")
    probe=_read("sdl3_gamepad_probe.c")
    native=_read("native_joystick_reconnect.c")
    assert '"ExperimentalSDL3NativeOwner=0' not in ini
    assert '"ExperimentalSDL3UniversalGameplay=1' not in ini
    assert 'native_owner_requested=0;' in probe
    assert "requested=1;" in probe
    assert "universal_requested=1;" in probe
    assert "try_rebind(" not in native
    assert '"ExperimentalNativeJoystickMappingSwap",1,ini)!=1' not in probe


def test_native_rebind_cannot_reopen_directinput_gamepad():
    native=_read("native_joystick_reconnect.c")
    main=_read("spiderman2k1fix.c")
    for banned in ("EnumDevices(", "CreateDevice(", "Acquire(", "IDirectInputDevice8_",
                   "InterlockedCompareExchangePointer(", "try_rebind("):
        assert banned not in native
    assert "SpiderNativeReconnect_OnFlip(count,game_hwnd);" in main
    assert "return 0;" in native.split("int SpiderNativeReconnect_SDL3SelectionReady(",1)[1].split("}",1)[0]
    assert "native_joystick_unplug.c" in _read("build.sh")


def test_universal_owner_and_native_focus_guards_remain_fail_closed():
    policy=_read("sdl3_gamepad_selection.h")
    probe=_read("sdl3_gamepad_probe.c")
    native=_read("native_joystick_reconnect.c")
    adapter=_read("sdl3_universal_gameplay.c")
    assert "spider_sdl3_selection_focus_lost(" in policy
    assert "spider_sdl3_selection_focus_lost(&selection)" in probe
    assert "try_rebind(" not in native
    assert "detach_stale_native(" not in native
    assert "SpiderSDL3Probe_SelectedMappedSample(" in adapter
    assert "native_host_focus_blocked" in adapter
    assert "SpiderSDL3Universal_OnConfirmedFocusPause(" in adapter
    assert "restore_original(\"focus or window lost\")" in adapter
