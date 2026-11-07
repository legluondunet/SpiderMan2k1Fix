"""SDL3-only controller backend: the original native device is never rebound.

Retain the original game's fatal-unplug and keyboard/focus safety protections.
Source-level guards do not replace physical keyboard-only and hotplug tests.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MAIN=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
PROBE=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
HEADER=(ROOT/"sdl3_gamepad_probe.h").read_text(encoding="utf-8")
SDL=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
GUARD=(ROOT/"native_joystick_unplug.c").read_text(encoding="utf-8")
BUILD=(ROOT/"build.sh").read_text(encoding="utf-8")


def test_native_rebind_source_and_hardware_assumptions_are_gone():
    for name in ("native_joystick_reconnect.c", "native_joystick_reconnect.h",
                 "native_joystick_reconnect_policy.h", "native_joystick_swap_probe.h"):
        assert not (ROOT/name).exists(), name
        assert name not in BUILD
    for src in (MAIN,PROBE,HEADER):
        assert "SpiderNativeReconnect_" not in src
    assert "IDirectInput8_CreateDevice(" not in PROBE
    assert "SpiderSDL3Probe_NativeOwnerEnabled" not in PROBE
    assert "spider_reconnect_kind(" not in PROBE


def test_original_unplug_crash_guard_is_preserved_as_safety_not_mapping():
    assert "native_joystick_unplug.c" in BUILD
    assert "SpiderNativeUnplug_Init(ini,log_line);" in MAIN
    assert "SpiderNativeUnplug_OnFlip(count);" in MAIN
    assert MAIN.index("SpiderNativeUnplug_OnFlip(count);") < MAIN.index(
        "SpiderSDL3Universal_OnFlip(game_hwnd,")
    assert "InterlockedExchange(&requested,1);" in GUARD
    assert "if(flip_index==120)install_guard();" in GUARD
    assert "VirtualProtect(" in GUARD
    assert "FlushInstructionCache(" in GUARD


def test_no_controller_does_not_enable_legacy_gamepad_or_block_keyboard():
    # No mapped device -> no synthetic keyboard-action input from SDL3.
    assert "if(!SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample))" in SDL
    assert 'restore_original("no selected mapped controller");' in SDL
    assert "native_unclaimed_guard" in SDL
    assert "SPIDER_UNIVERSAL_DI_SLOT" in SDL
    assert "if(!read_scans(scans))" in SDL
    assert "release_all()" in SDL
    assert "SpiderDI8Retry_OnFlip(count);" in MAIN
    assert "SpiderPauseAny_Begin(hwnd)" in MAIN
    # A failed SDL3 gamepad lookup is not a failure of the original keyboard.
    assert "selected mapped pad" in PROBE
    assert "return 0;" in PROBE


def test_selected_sdl3_is_only_gamepad_owner_and_has_no_native_fallback():
    assert "SpiderSDL3Probe_SelectedMappedSample(" in PROBE
    assert "SpiderSDL3Probe_GenericSelectedSample(" in PROBE
    assert "spider_sdl3_selection_start_edge(" in PROBE
    assert "spider_sdl3_selected_model_unique(" in PROBE
    assert "SpiderSDL3Probe_SelectedIdentity" not in PROBE
    assert "SpiderSDL3Probe_SelectedSample(" not in PROBE
    assert "SpiderSDL3Probe_SelectedRT(" not in PROBE
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in MAIN
    assert "suspend_original()" in SDL
    assert "restore_original(" in SDL
