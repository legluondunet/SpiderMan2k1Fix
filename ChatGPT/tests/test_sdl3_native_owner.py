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
    native=_read("sdl3_universal_gameplay.c")
    assert '"ExperimentalSDL3NativeOwner=0' not in ini
    assert '"ExperimentalSDL3UniversalGameplay=1' not in ini
    assert 'native_owner_requested=0;' in probe
    assert "requested=1;" in probe
    assert "universal_requested=1;" in probe
    assert "SPIDER_UNIVERSAL_DI_SLOT" in native
    assert '"ExperimentalNativeJoystickMappingSwap",1,ini)!=1' not in probe


def test_native_owner_uses_verified_original_game_device_only():
    native=_read("sdl3_universal_gameplay.c")
    main=_read("spiderman2k1fix.c")
    assert "#define SPIDER_NATIVE_DEVICE 0x006B7A2Cu" in native
    assert "valid_binary_layout()" in native
    assert "SpiderNativeUnplug_Installed()" in native
    assert "enumerate_sdl3_target" in native
    assert "target.matches!=1u" in native
    assert "IDirectInputDevice8_GetDeviceInfo(old,&info)" in native
    assert "IsEqualGUID(&info.guidInstance,&target.instance)" in native
    assert "IDirectInput8_CreateDevice(di,&target.instance,&candidate,NULL)" in native
    assert "if(!neutral_and_flush(candidate))" in native
    assert "SPIDER_NATIVE_FORCE_FEEDBACK" in native
    assert "InterlockedCompareExchangePointer(" in native
    assert "zero_native_buttons();" in native
    assert "swap_native_gameplay_mapping(" not in native
    assert "spider_ps1_buttons" not in native
    assert "spider_xbox360_buttons" not in native
    assert main.index("SpiderSDL3Probe_OnFlip(count,game_hwnd);") < main.index(
        "SpiderGamepadUI_OnFlip(game_hwnd,")
    assert main.index("SpiderNativeUnplug_OnFlip(count);") < main.index(
        "SpiderNativeReconnect_OnFlip(count,game_hwnd);")


def test_original_native_reconnect_writes_no_custom_profiles_or_synthetic_input():
    native=_read("sdl3_universal_gameplay.c")
    probe=_read("sdl3_gamepad_probe.c")
    assert "IDirectInputDevice8_GetDeviceInfo" not in probe
    for token in ("SendInput(", "SetWindowLong", "CreateThread(",
                  "WriteFile(", "spider_native_swap_preset("):
        assert token not in native
    assert "SetDataFormat(device," in native
    assert "swap_mapping_enabled=0;" in native


def test_universal_owner_and_native_focus_guards_remain_fail_closed():
    policy=_read("sdl3_gamepad_selection.h")
    probe=_read("sdl3_gamepad_probe.c")
    native=_read("sdl3_universal_gameplay.c")
    adapter=_read("sdl3_universal_gameplay.c")
    assert "spider_sdl3_selection_focus_lost(" in policy
    assert "spider_sdl3_selection_focus_lost(&selection)" in probe
    assert "sdl3_owner_last_attempt=now;" in native
    assert "sdl3_owner_last_verify=now;" in native
    assert "SpiderSDL3Probe_SelectedMappedSample(" in adapter
    assert "native_host_focus_blocked" in adapter
    assert "SpiderSDL3Universal_OnConfirmedFocusPause(" in adapter
    assert "restore_original(\"focus or window lost\")" in adapter
