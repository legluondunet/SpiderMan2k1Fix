"""Legacy-input isolation when the experimental universal SDL3 adapter is enabled.

The Xbox/DS4 native overlays remain available only in explicitly
non-universal compatibility mode. A first-time universal controller must
never silently inherit the legacy hard-coded native action layout.
Tests are structural; CI compilation and real pad validation remain
separate gates.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def _bootstrap():
    core = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
    return core.split("static HRESULT WINAPI hooked_create(", 1)[1].split(
        "HRESULT hr=original_create(", 1
    )[0]


def test_universal_bootstrap_does_not_install_legacy_fixed_profiles():
    bootstrap = _bootstrap()
    assert "int universal=SpiderSDL3Universal_Enabled();" in bootstrap
    native = bootstrap.split("if(!universal) {", 1)[1].split(
        "SpiderCfg_BootstrapGameRead();", 1
    )[0]
    assert "SpiderGamepadUI_PreparePS1(early)" in native
    assert "SpiderGamepadUI_PrepareXbox360(early)" in native
    assert "SpiderCfg_EnablePS1Preview()" in native
    assert "SpiderCfg_EnableXbox360Preview()" in native
    # No early legacy native RAM mapping before universal ownership.
    assert "SpiderCfg_EnableXbox360Preview()" not in bootstrap.split(
        "if(!universal) {", 1
    )[0]
    assert "SpiderCfg_EnablePS1Preview()" not in bootstrap.split(
        "if(!universal) {", 1
    )[0]
    assert "SpiderCfg_BootstrapGameRead();" in bootstrap
    assert "SpiderCfg_InstallModernKeyboardDefaults();" in bootstrap
    legacy = bootstrap.split("if(universal)", 1)[1].split(
        "HRESULT hr=original_create", 1
    )[0]
    assert "else {" in legacy
    for call in (
        "SpiderGamepadUI_ArmPS1ReadOnly();",
        "SpiderGamepadUI_ArmXbox360ReadOnly();",
        "SpiderGamepadUI_ActivatePS1();",
        "SpiderGamepadUI_ActivateXbox360();",
    ):
        assert call in legacy


def test_universal_runtime_has_one_controller_input_authority():
    ui = (ROOT / "gamepad_ui_profile.c").read_text(encoding="utf-8")
    native = (ROOT / "native_joystick_reconnect.c").read_text(encoding="utf-8")
    live = (ROOT / "sdl3_gameplay_live.c").read_text(encoding="utf-8")
    universal = (ROOT / "sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    ui_flip = ui.split("void SpiderGamepadUI_OnFlip(", 1)[1]
    assert ui_flip.index("if(SpiderSDL3Universal_Enabled()) {") < (
        ui_flip.index("sdl3_generic_ui_on_flip(")
    )
    assert 'xbox_rt_swing_release("universal SDL3 3D keyboard adapter");' in ui_flip
    native_flip = native.split("void SpiderNativeReconnect_OnFlip(", 1)[1]
    assert native_flip.index("if(SpiderSDL3Universal_Enabled())return;") < (
        native_flip.index("if(!requested || !initialized")
    )
    assert '"ExperimentalSDL3UniversalGameplay",0,ini)!=1' in live
    assert "SpiderSDL3Probe_SelectedMappedSample(" in universal


def test_legacy_paths_retained_outside_universal_opt_in():
    bootstrap = _bootstrap()
    assert "if(!universal) {" in bootstrap
    assert "else {" in bootstrap.split("if(universal)", 1)[1]
    assert "SpiderCfg_EnableXbox360Preview()" in bootstrap
    assert "SpiderCfg_EnablePS1Preview()" in bootstrap
    assert "SpiderCfg_CancelXbox360Preview()" in bootstrap
    assert "SpiderCfg_CancelPS1Preview()" in bootstrap
    init = (ROOT / "sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3UniversalGameplay",0,ini)==1' in init
