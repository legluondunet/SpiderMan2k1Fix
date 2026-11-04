"""Regression guards: no implicit native DirectInput takeover in SDL3 universal mode.

Hardware evidence: SpiderMan2k1Fix(20261104-163721).log. After DS4
detach, the remaining PS3 became usable with legacy native bindings
until its physical Start selected universal SDL3 gameplay.
These checks are intentionally structural; real pad/Win32 validation
is still required before approving experimental gameplay.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_unclaimed_sdl3_controller_cannot_reenable_native_joystick():
    src = (ROOT / "sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    prob = (ROOT / "sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    callback = src.split("void SpiderSDL3Universal_OnFlip(", 1)[1]
    no_owner = callback.split(
        "if(!SpiderSDL3Probe_SelectedMappedSample(", 1
    )[1].split("profile=SpiderSDL3Probe_SetupProfile(", 1)[0]

    # The passive singleton selection after DS4 unplug is NOT permission.
    selected = prob.split(
        "int SpiderSDL3Probe_SelectedMappedSample(", 1
    )[1].split("/* The player may hold physical Cross", 1)[0]
    assert "!generic_selected_id" in selected
    assert "return SpiderSDL3Probe_GenericSelectedSample(" in selected
    assert "return SpiderSDL3Probe_SelectedSample(" not in selected
    assert "selection.active" not in selected

    # On losing the old explicit owner, always release its synthetic
    # keys; then re-borrow the verified native slot before returning
    # control to the original game. Do not do this on every frame.
    assert 'restore_original("no selected mapped controller");' in no_owner
    assert "native_unclaimed_guard" in no_owner
    assert "writable_slot() && suspend_original()" in no_owner
    assert no_owner.index('restore_original("no selected mapped controller");') < (
        no_owner.index("writable_slot() && suspend_original()"))
    assert "native_unclaimed_guard=1;" in no_owner
    assert "if(!suspended && writable_slot() && suspend_original())" in no_owner
    assert "if(native_unclaimed_guard && (sent_mask || pulse_scan))" in no_owner
    assert "release_all();" in no_owner
    assert "focus_dir.phase==SPIDER_FOCUS_DIR_UP" in no_owner
    assert "UNCLAIMED native DirectInput joystick held inactive" in no_owner
    assert "if(native_unclaimed_guard && suspended" in no_owner


def test_unclaimed_native_guard_requires_neutral_fresh_start():
    src = (ROOT / "sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    callback = src.split("void SpiderSDL3Universal_OnFlip(", 1)[1]
    gate = "if(native_unclaimed_guard && !spider_sdl3_universal_neutral(&sample))"
    transfer = "native_unclaimed_guard=0;"
    assert gate in callback
    assert callback.index(gate) < callback.index("if(!suspend_original())")
    assert callback.index(gate) < callback.index(transfer)
    assert "UNCLAIMED native DI guard transferred to explicitly Started SDL3 controller" in callback
    assert "if(!suspended && !spider_sdl3_universal_neutral(&sample))" in callback


def test_unclaimed_guard_restores_native_on_focus_or_modal_loss():
    src = (ROOT / "sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    restore = src.split(
        "static void restore_original(const char *reason) {", 1
    )[1].split("void SpiderSDL3Universal_SetHostFocusBlocked(", 1)[0]
    focus = src.split("void SpiderSDL3Universal_SetHostFocusBlocked(", 1)[1].split(
        "static int suspend_original(", 1)[0]
    modal = src.split("void SpiderSDL3Universal_BeforeModal(", 1)[1].split(
        "void SpiderSDL3Universal_OnConfirmedFocusPause(", 1)[0]
    assert "native_unclaimed_guard=0;" in restore
    assert 'restore_original("focus or window lost");' in focus
    assert 'restore_original("opening controller configuration modal");' in modal
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3UniversalGameplay",1,ini)==1' in src
    assert "native_unclaimed_guard=0;native_guard_reports=0;" in src
