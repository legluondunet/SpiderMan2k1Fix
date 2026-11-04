"""Regression: the previous fixed Sony/Xbox input presets are gone.

Native retail input remains intact until the user explicitly selects
universal SDL3, and there is no legacy pre-bootstrap table override.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def test_original_bootstrap_never_injects_old_vendor_preset():
    core=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    stage=core.split("static HRESULT WINAPI hooked_create(",1)[1].split(
        "HRESULT hr=original_create(",1)[0]
    assert "SpiderCfg_BootstrapGameRead();" in stage
    assert "SpiderCfg_InstallModernKeyboardDefaults();" in stage
    for retired in ("SpiderCfg_EnableXbox360Preview(",
                    "SpiderCfg_EnablePS1Preview(",
                    "SpiderGamepadUI_PrepareXbox360(",
                    "SpiderGamepadUI_PreparePS1(",
                    "SpiderGamepadUI_ActivateXbox360(",
                    "SpiderGamepadUI_ActivatePS1("):
        assert retired not in stage


def test_universal_gameplay_is_single_input_owner():
    core=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    adapter=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    live=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in core
    assert "SpiderSDL3Probe_SelectedMappedSample(" in adapter
    assert "sdl3_generic_ui_on_flip(" not in ui
    assert "begin_scancode(" not in ui
    assert "spider_ps1_apply(" not in ui
    assert "swap_native_gameplay_mapping(" not in native
    assert "void SpiderSDL3GameplayLive_OnFlip(" in live
    assert "native action-map backend retired" in live


def test_opt_in_and_native_device_safety_still_separate():
    adapter=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    core=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3UniversalGameplay",0,ini)==1' in adapter
    assert "SpiderNativeUnplug_OnFlip(count);" in core
    assert "SpiderNativeReconnect_OnFlip(count,game_hwnd);" in core
    assert "if(SpiderSDL3Universal_Enabled())return;" in native
    assert "SpiderNativeReconnect_SDL3SelectionReady(" in native
