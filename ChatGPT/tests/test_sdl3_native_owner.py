"""Only SDL3-mapped gamepads may claim controller input.

The original game-owned DirectInput pointer is preserved and quarantined
for runtime safety, never replaced by native physical-controller rebinding.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT/path).read_text(encoding="utf-8")


def test_legacy_native_owner_is_not_a_controller_backend():
    ini=read("virtual_spidey_cfg.c")
    probe=read("sdl3_gamepad_probe.c")
    build=read("build.sh")
    core=read("spiderman2k1fix.c")
    assert 'native_owner_requested=0;' in probe
    assert "requested=1;" in probe
    assert "universal_requested=1;" in probe
    assert '"ExperimentalSDL3NativeOwner=0' not in ini
    assert '"ExperimentalSDL3UniversalGameplay=1' not in ini
    assert not (ROOT/"native_joystick_reconnect.c").exists()
    assert not (ROOT/"native_joystick_reconnect.h").exists()
    assert "native_joystick_reconnect.c" not in build
    assert "SpiderNativeReconnect_" not in core + probe
    assert 'SpiderSDL3Probe_SelectedMappedSample(' in read("sdl3_universal_gameplay.c")


def test_original_di_isolated_during_unclaimed_controller_and_focus_loss():
    core=read("spiderman2k1fix.c")
    probe=read("sdl3_gamepad_probe.c")
    adapter=read("sdl3_universal_gameplay.c")
    assert "spider_sdl3_selection_focus_lost(&selection)" in probe
    assert "suspend_original()" in adapter
    assert "native_unclaimed_guard" in adapter
    assert 'restore_original("no selected mapped controller");' in adapter
    assert 'restore_original("focus or window lost");' in adapter
    assert 'restore_original("game HWND not foreground");' in adapter
    assert "SPIDER_UNIVERSAL_DI_SLOT" in adapter
    assert "InterlockedCompareExchangePointer(SPIDER_UNIVERSAL_DI_SLOT" in adapter
    assert "SpiderSDL3Universal_OnConfirmedFocusPause();" in core
    assert "native_host_focus_blocked" in adapter
    assert "release_all()" in adapter


def test_retail_unplug_guard_remains_instead_of_native_rebind():
    core=read("spiderman2k1fix.c")
    unplug=read("native_joystick_unplug.c")
    assert "SpiderNativeUnplug_OnFlip(count);" in core
    assert "native_joystick_unplug.c" in read("build.sh")
    assert "if(flip_index==120)install_guard();" in unplug
    assert "DIERR_UNPLUGGED" in unplug
    assert "VirtualProtect(" in unplug
