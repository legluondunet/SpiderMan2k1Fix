"""Guardrails for the experimental SDL3 -> native DirectInput owner bridge.

These portable source-contract regressions do not claim physical hardware
validation; actual COM/device lifetime still needs Windows 11 + Wine tests.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def _read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def test_sdl3_owner_requires_explicit_complete_opt_in_and_is_off_by_default():
    ini = _read("ini_validation.h")
    probe = _read("sdl3_gamepad_probe.c")
    native = _read("native_joystick_reconnect.c")
    assert '"ExperimentalSDL3NativeOwner"' in ini
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3NativeOwner",0,ini)==1' in probe
    for key in (
        "ExperimentalNativeJoystickUnplugGuard",
        "ExperimentalNativeJoystickReconnect",
        "ExperimentalNativeJoystickMappingSwap",
    ):
        assert f'GetPrivateProfileIntA("Input","{key}",0,ini)!=1' in probe
    assert 'native_owner_requested=0;' in probe
    assert "SDL3-OWNER: REFUSED missing native" in probe
    assert "if(SpiderSDL3Probe_NativeOwnerEnabled())" in native


def test_native_owner_uses_game_owned_di_and_never_mutates_unverified_pointers():
    native = _read("native_joystick_reconnect.c")
    ui = _read("gamepad_ui_profile.c")
    main = _read("spiderman2k1fix.c")
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
    assert "swap_native_gameplay_mapping(target.kind);" in native
    assert 'report("NATIVE-SWAP: REFUSED CUSTOM joystick mapping;' in native
    assert "SpiderSDL3Probe_SelectedIdentity(&selected_vid,&selected_pid" in ui
    assert "SpiderNativeReconnect_SDL3SelectionReady(" in ui
    assert "d.ds4_count==1u" in ui and "d.xbox_count==1u" in ui
    assert "SDL3-OWNER-UI: HANDOFF" in ui
    assert "release_active_key();" in ui
    assert main.index("SpiderSDL3Probe_OnFlip(count,game_hwnd);") < main.index(
        "SpiderGamepadUI_OnFlip(game_hwnd,")
    assert main.index("SpiderNativeUnplug_OnFlip(count);") < main.index(
        "SpiderNativeReconnect_OnFlip(count,game_hwnd);")


def test_native_owner_does_not_hook_di_com_vtables_or_write_game_files():
    native = _read("native_joystick_reconnect.c")
    probe = _read("sdl3_gamepad_probe.c")
    assert "IDirectInputDevice8_GetDeviceInfo" not in probe
    assert "SendInput(" not in native
    assert "SetWindowLong" not in native
    assert "CreateThread(" not in native
    assert "WriteFile(" not in native
    assert "SetDataFormat(device," in native
