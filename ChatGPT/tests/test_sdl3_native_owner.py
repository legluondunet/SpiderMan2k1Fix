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


def test_sdl3_native_runtime_canonical_pause_and_return_ownership_guards():
    native = _read("native_joystick_reconnect.c")
    ui = _read("gamepad_ui_profile.c")
    policy = _read("sdl3_gamepad_selection.h")
    assert "spider_native_reconnect_canonical_mapping(SPIDER_RECONNECT_DS4)" in native
    assert "spider_native_reconnect_canonical_mapping(SPIDER_RECONNECT_XBOX360)" in native
    assert "SpiderNativeReconnect_CanonicalDS4Mapping()" in ui
    assert "SpiderNativeReconnect_CanonicalXboxMapping()" in ui
    assert "ps1_preview_active || late_ds4_menu_only || sdl3_live_sony" in ui
    assert "xbox360_preview_active || sdl3_live_xbox" in ui
    assert "SDL3-OWNER-UI: canonical native Sony DS4 SHARE -> Escape" in ui
    assert "spider_sdl3_selection_focus_lost(" in policy
    assert "spider_sdl3_selection_focus_lost(&selection)" in _read("sdl3_gamepad_probe.c")
    assert "sdl3_owner_last_attempt=now;" in native
    assert "sdl3_owner_last_verify=now;" in native


def test_3d_trace_is_opt_in_read_only_bounded_and_gameplay_only():
    ini = _read("ini_validation.h")
    ui = _read("gamepad_ui_profile.c")
    assert '"SDL3GameplayTrace"' in ini
    assert 'GetPrivateProfileIntA("Debug","SDL3GameplayTrace",0,ini)==1' in ui
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3NativeOwner",0,ini)==1' in ui
    assert "sdl3_3d_trace_lines>=64u" in ui
    assert "selected-id=%lu kind=%s" in ui
    assert "secondary DirectInput READ-ONLY; native game actions NOT inferred" in ui
    block = ui.split("static void sdl3_trace_3d_readonly(", 1)[1].split(
        "void SpiderGamepadUI_OnFlip(", 1
    )[0]
    for forbidden in (
        "IDirectInputDevice8_", "IDirectInput8_", "SendInput(",
        "VirtualProtect(", "WriteFile(", "Acquire(", "SetProperty(",
    ):
        assert forbidden not in block
    assert "if(gameplay_3d && !menu_2d && !bink_active && !native_pause_active)" in ui
    assert "sdl3_trace_3d_readonly(&state);" in ui
    assert "sdl3_3d_trace_valid=0;" in ui
    assert "sdl3_3d_trace_id!=sdl3_ui_instance" in block
    assert 'first?"OWNER":changed?"BUTTON":"AXIS"' in block
