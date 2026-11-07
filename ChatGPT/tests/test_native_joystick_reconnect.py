"""Regression: legacy native controller reconnection has been removed.

The 2001 game still owns an original DirectInput pointer; SDL3 must isolate it
without ever rebinding a separate hardware device or losing keyboard control.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT/path).read_text(encoding="utf-8")


def test_native_rebind_is_absent_from_runtime_build_and_callsites():
    core=read("spiderman2k1fix.c")
    build=read("build.sh")
    probe=read("sdl3_gamepad_probe.c")
    plan=read("sdl3_gameplay_plan_probe.c")
    assert not (ROOT/"native_joystick_reconnect.c").exists()
    assert not (ROOT/"native_joystick_reconnect.h").exists()
    assert "native_joystick_reconnect.c" not in build
    for src in (core,probe,plan):
        assert '#include "native_joystick_reconnect.h"' not in src
        assert "SpiderNativeReconnect_" not in src
    assert "SpiderNativeReconnect_" not in build


def test_keyboard_still_operates_without_sdl3_controller():
    core=read("spiderman2k1fix.c")
    sdl3=read("sdl3_universal_gameplay.c")
    probe=read("sdl3_gamepad_probe.c")
    assert "requested=1; /* SDL3-only gameplay, no legacy-INI opt-out. */" in sdl3
    assert "if(!SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample))" in sdl3
    assert "suspend_original()" in sdl3
    assert "native_unclaimed_guard=1;" in sdl3
    assert "no selected mapped controller" in sdl3
    assert "Keyboard input remains independent." in sdl3
    assert "spider_sdl3_universal_neutral(&sample)" in sdl3
    assert "send_scan(" in sdl3
    assert "SpiderSDL3Probe_SelectedMappedSample(" in probe
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in core
    assert "SpiderDI8Retry_OnFlip(count);" in core


def test_fatal_native_unplug_and_focus_release_safety_remain():
    core=read("spiderman2k1fix.c")
    unplug=read("native_joystick_unplug.c")
    bridge=read("sdl3_universal_gameplay.c")
    assert "native_joystick_unplug.c" in read("build.sh")
    assert "SpiderNativeUnplug_OnFlip(count);" in core
    assert "if(flip_index==120)install_guard();" in unplug
    assert "VirtualProtect(" in unplug
    assert "DIERR_UNPLUGGED" in unplug
    assert "SPIDER_UNIVERSAL_DI_SLOT" in bridge
    assert "InterlockedCompareExchangePointer(SPIDER_UNIVERSAL_DI_SLOT" in bridge
    assert 'restore_original("focus or window lost");' in bridge
    assert 'restore_original("game HWND not foreground");' in bridge
    assert "release_all()" in bridge
