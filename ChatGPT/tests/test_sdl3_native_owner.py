"""Only mapped SDL3 gamepads may claim player input.

The original game's DirectInput joystick remains a guarded, isolated pointer
during SDL3 gameplay, but must never be rebound by a second controller mapper.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT/path).read_text(encoding="utf-8")


def test_legacy_native_owner_and_vendor_rebind_are_removed():
    probe=read("sdl3_gamepad_probe.c")
    core=read("spiderman2k1fix.c")
    build=read("build.sh")
    assert "requested=1;" in probe
    assert "universal_requested=1;" in probe
    assert "spider_sdl3_selection_start_edge(" in probe
    for name in ("native_joystick_reconnect.c", "native_joystick_reconnect.h",
                 "native_joystick_reconnect_policy.h", "native_joystick_swap_probe.h"):
        assert not (ROOT/name).exists()
        assert name not in build
    for token in ("SpiderNativeReconnect_", "SpiderSDL3Probe_NativeOwnerEnabled",
                  "spider_reconnect_kind(", "SpiderSDL3Probe_SelectedIdentity("):
        assert token not in probe+core
    assert "native_joystick_unplug.c" in build  # original crash guard retained


def test_original_directinput_pointer_is_only_temporarily_isolated():
    adapter=read("sdl3_universal_gameplay.c")
    assert "SPIDER_UNIVERSAL_DI_SLOT" in adapter
    assert "static int suspend_original(void)" in adapter
    assert "static void restore_original(const char *reason)" in adapter
    assert "InterlockedCompareExchangePointer(SPIDER_UNIVERSAL_DI_SLOT" in adapter
    assert "if(!writable_slot())return 0;" in adapter
    assert "if(!release_all())" in adapter
    assert "SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample)" in adapter
    assert "spider_sdl3_universal_neutral(&sample)" in adapter
    assert "IDirectInput8_CreateDevice(" not in adapter
    assert "IDirectInputDevice8_Acquire(" not in adapter


def test_focus_disconnection_and_uncertain_identity_fail_closed():
    probe=read("sdl3_gamepad_probe.c")
    adapter=read("sdl3_universal_gameplay.c")
    assert "spider_sdl3_selected_model_unique(" in probe
    assert "spider_sdl3_selection_focus_lost(&selection)" in probe
    assert "SDL3-UNIVERSAL: START REFUSED ambiguous/unidentified mapped pad" in probe
    assert "native_host_focus_blocked" in adapter
    assert "SpiderSDL3Universal_OnConfirmedFocusPause" in adapter
    assert 'restore_original("focus or window lost")' in adapter
    assert 'restore_original("no selected mapped controller")' in adapter
    assert "release_all()" in adapter
    assert "if(!suspended && !spider_sdl3_universal_neutral(&sample))" in adapter


def test_original_keyboard_and_sdl3_selection_remain_independent():
    core=read("spiderman2k1fix.c")
    probe=read("sdl3_gamepad_probe.c")
    adapter=read("sdl3_universal_gameplay.c")
    assert "SpiderDI8Retry_OnFlip(count);" in core
    assert "SpiderPauseAny_Begin(hwnd)" in core
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in core
    assert "SpiderSDL3Probe_PauseWake_Begin();" in core
    assert "SpiderGamepadUI_PauseWake_" not in core
    assert "if(!SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample))" in adapter
    assert "if(!read_scans(scans))" in adapter
    assert "SPIDER_UNIVERSAL_DI_SLOT" in adapter
    assert "generic_selected_id" in probe
