"""Regression: retired native joystick rebind never activates as a fallback.

The game continues to use original DirectInput internally, but
SpiderMan2k1Fix supports only SDL3-mapped controllers. Failure to select
one must NOT synthesize a legacy Sony/Xbox native gamepad mapping.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
RECONNECT=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
PROBE=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
UNIVERSAL=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
UNPLUG=(ROOT/"native_joystick_unplug.c").read_text(encoding="utf-8")
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")


def test_legacy_reconnect_exports_are_inert():
    for symbol in ("SpiderNativeReconnect_Init", "SpiderNativeReconnect_OnFlip",
                   "SpiderNativeReconnect_SDL3SelectionReady",
                   "SpiderNativeReconnect_LogSyntheticMenuKey",
                   "SpiderNativeReconnect_ObserveMode"):
        assert symbol in RECONNECT
    assert "SpiderNativeReconnect_SDL3SelectionReady(uint16_t vid,uint16_t pid)" in RECONNECT
    section=RECONNECT.split("int SpiderNativeReconnect_SDL3SelectionReady(",1)[1].split("}",1)[0]
    assert "return 0;" in section
    assert "SDL3 is the only supported controller mapping backend" in RECONNECT


def test_no_native_joystick_enumeration_or_rebind_survives():
    for forbidden in ("dinput.h", "IDirectInput8_", "IDirectInputDevice8_",
                      "DI8DEVCLASS_GAMECTRL", "CreateDevice(", "EnumDevices(",
                      "GetDeviceState(", "Acquire(", "GetDeviceData(",
                      "SetDataFormat(", "SetCooperativeLevel(",
                      "InterlockedCompareExchangePointer(",
                      "VirtualProtect(", "SendInput(", "try_rebind(",
                      "detach_stale_native(", "native action table", "SPIDER_NATIVE_DEVICE"):
        assert forbidden not in RECONNECT, forbidden
    assert "native_joystick_reconnect.c" in BUILD


def test_keyboard_remains_available_when_no_sdl3_gamepad_selected():
    assert "requested=1; /* SDL3-only gameplay, no legacy-INI opt-out. */" in UNIVERSAL
    assert "native_owner_requested=0;" in PROBE
    assert "SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample)" in UNIVERSAL
    assert 'restore_original("no selected mapped controller");' in UNIVERSAL
    assert "UNCLAIMED native DirectInput joystick held inactive until explicit SDL3 Start (keyboard unaffected)" in UNIVERSAL
    assert "if(!suspended && writable_slot() && suspend_original())" in UNIVERSAL
    assert "SpiderNativeUnplug_OnFlip(count);" in CORE
    assert "if(flip_index==120)install_guard();" in UNPLUG


def test_no_legacy_mapping_or_forced_exit_when_sdl3_is_unavailable():
    assert "SpiderSDL3Universal_Enabled()" not in RECONNECT
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" not in RECONNECT
    assert "TerminateProcess(" not in RECONNECT
    assert "exit(" not in RECONNECT
    assert "SpiderSDL3Probe_SelectedMappedSample" in UNIVERSAL
