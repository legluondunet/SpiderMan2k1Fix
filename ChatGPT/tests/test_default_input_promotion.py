"""New-user INI: generic SDL3 discovery, opt-in universal gameplay, no old preset."""
from pathlib import Path
import configparser

ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
EXAMPLE=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")
PROBE=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
ADAPTER=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
RECONNECT=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
UNPLUG=(ROOT/"native_joystick_unplug.c").read_text(encoding="utf-8")
ALT=(ROOT/"alt_latch_recovery.h").read_text(encoding="utf-8")


def _new_ini():
    cfg=configparser.ConfigParser(interpolation=None,strict=True)
    cfg.read_string(EXAMPLE)
    return cfg


def test_new_generated_config_matches_only_live_controller_settings():
    cfg=_new_ini()
    expected={
      "ExperimentalSDL3Probe":"1",
      "ExperimentalSDL3NativeOwner":"0",
      "ExperimentalSDL3ControllerSetup":"1",
      "ExperimentalSDL3UniversalGameplay":"0",
      "ExperimentalNativeJoystickUnplugGuard":"1",
      "ExperimentalNativeJoystickReconnect":"1",
      "ExperimentalAltLatchRecovery":"1",
    }
    for key,value in expected.items():
        assert cfg["Input"][key]==value
        assert f'"{key}={value}\\r\\n"' in SRC
    assert cfg["Debug"]["Log"]=="0"
    for retired in (
      "ControllerProfile","ControllerXbox360Preview",
      "ControllerPS1Preview","ControllerXbox360RTSwing",
      "ControllerEnterButtons","ControllerBackButton",
      "ControllerPauseButton","ExperimentalNativeJoystickMappingSwap",
      "ExperimentalSDL3ContextBridge","ExperimentalSDL3GameplayBindings",
      "ExperimentalSDL3XboxRTSwing"):
        assert retired not in cfg["Input"]


def test_old_controller_preset_options_are_ignored_not_synthesized():
    for key in ("ControllerProfile","ControllerXbox360Preview",
                "ControllerPS1Preview","ControllerXbox360RTSwing",
                "ControllerEnterButtons","ControllerBackButton",
                "ControllerPauseButton",
                "ExperimentalSDL3GameplayBindings","ExperimentalSDL3XboxRTSwing"):
        assert f'{{"Input","{key}"}}' in SRC
    assert "swap_mapping_enabled=0;" in RECONNECT
    assert "swap_native_gameplay_mapping(" not in RECONNECT
    assert '"ExperimentalNativeJoystickMappingSwap=1' not in SRC
    assert "sdl3_generic_ui_on_flip(" not in (
        ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")


def test_native_safety_and_universal_optin_remain_guarded():
    assert '"ExperimentalSDL3Probe",1,ini)==1' in PROBE
    assert '"ExperimentalSDL3NativeOwner",1,ini)==1' in PROBE
    assert '"ExperimentalSDL3UniversalGameplay",0,ini)==1' in ADAPTER
    assert '"ExperimentalNativeJoystickUnplugGuard",1,ini)==1' in UNPLUG
    assert '"ExperimentalNativeJoystickReconnect",1,ini)==1' in RECONNECT
    assert "native_owner_requested=0;" in PROBE
    assert "SDL3-OWNER: REFUSED missing native UnplugGuard+Reconnect" in PROBE
    assert "spider_alt_latch_preflight()" in ALT
    assert "GetAsyncKeyState(VK_TAB)" in ALT


def test_default_config_preserves_original_retained_game_settings():
    cfg=_new_ini()
    assert cfg["Input"]["KeyboardLayout"]=="Auto"
    assert cfg["Display"]["AspectRatio"]=="Auto"
    assert "Joystick" in cfg
    assert "Internal" in cfg and len(cfg["Internal"]["RawHex"])==504
    assert "Keyboard" in cfg
    validator=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    assert "spider_ini_uint(value,0,1)" in validator
    assert "Duplicate key [%s] %s" in validator
    assert "if(!check.errors && !check.warnings)" in SRC
