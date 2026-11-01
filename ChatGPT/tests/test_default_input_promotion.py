"""Release staging: guarded SDL3/native-owner/Alt defaults and explicit opt-outs.

These are static/INI regression checks. Physical Windows/Wine testing of the
new defaults is required separately; they do not assert unknown pad support.
"""
from pathlib import Path
import configparser

ROOT=Path(__file__).resolve().parents[2]
CFG=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
EXAMPLE=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")
PROBE=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
UI=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
LIVE=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
SETUP=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
UNPLUG=(ROOT/"native_joystick_unplug.c").read_text(encoding="utf-8")
RECONNECT=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
ALT=(ROOT/"alt_latch_recovery.h").read_text(encoding="utf-8")

KEYS={
    "ExperimentalSDL3Probe":PROBE,
    "ExperimentalSDL3NativeOwner":PROBE,
    "ExperimentalSDL3ControllerSetup":SETUP,
    "ExperimentalSDL3ContextBridge":UI,
    "ExperimentalSDL3GameplayBindings":LIVE,
    "ExperimentalNativeJoystickUnplugGuard":UNPLUG,
    "ExperimentalNativeJoystickReconnect":RECONNECT,
    "ExperimentalNativeJoystickMappingSwap":RECONNECT,
    "ExperimentalAltLatchRecovery":ALT,
}


def test_new_generated_ini_defaults_and_explicit_runtime_fallback():
    cfg=configparser.ConfigParser(interpolation=None)
    cfg.read_string(EXAMPLE)
    for key,src in KEYS.items():
        assert cfg["Input"][key]=="1", key
        assert f'"{key}=1\\r\\n"' in CFG, key
        assert f'"{key}",1,ini)' in src, key
        assert f'"{key}",0,ini)' not in src, key
    assert cfg["Input"]["ExperimentalSDL3XboxRTSwing"]=="1"
    assert '"ExperimentalSDL3XboxRTSwing=1\\r\\n"' in CFG
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3XboxRTSwing",1,ini)==1' in UI
    assert cfg["Debug"]["Log"]=="0"


def test_legacy_explicit_zero_still_disables_each_feature():
    # GetPrivateProfileIntA returns a saved 0 over a default=1 fallback.
    # Every runtime path still checks ==1; absent -> 1, explicit 0 -> 0.
    for key,src in KEYS.items():
        assert f'"{key}",1,ini)==1' in src or (
            f'"{key}",1,ini)!=1' in src), key
    assert 'native_owner_requested=0;' in PROBE
    assert 'SpiderSDL3Probe_NativeOwnerEnabled()' in RECONNECT
    assert 'spider_alt_latch_preflight()' in ALT
    assert '*left==0x7fu' in ALT
    assert 'GetAsyncKeyState(VK_TAB)' in ALT


def test_native_safety_dependency_chain_remains_fail_closed():
    for key in ("ExperimentalNativeJoystickUnplugGuard",
                "ExperimentalNativeJoystickReconnect",
                "ExperimentalNativeJoystickMappingSwap"):
        assert f'GetPrivateProfileIntA("Input","{key}",1,ini)!=1' in PROBE
    assert "SDL3-OWNER: REFUSED missing native" in PROBE
    assert "target.matches!=1u" in RECONNECT
    assert "SPIDER_NATIVE_FORCE_FEEDBACK" in RECONNECT
    assert "GetForegroundWindow()==hwnd" in ALT
    assert "ExperimentalSDL3XboxRTSwing" in UI


def test_debug_and_untested_ps1_experiments_not_promoted():
    for k in ("ControllerPS1Preview","ControllerXbox360Preview",
              "ControllerPS1OptionsSkip"):
        assert f'GetPrivateProfileIntA("Input","{k}",0,ini)==1' in UI
    assert 'GetPrivateProfileIntA("Debug","Xbox360TriggerProbe",0,ini)==1' in UI
    assert 'GetPrivateProfileIntA("Debug","SDL3GameplayTrace",0,ini)==1' in UI
    assert '"Log=0\\r\\n"' in CFG


def test_legacy_ini_saved_zero_is_not_silently_changed():
    """An old generated value of 0 is indistinguishable from a user opt-out.

    Missing key -> new runtime fallback ON; present =0 -> OFF.
    A valid old configuration should not undergo the auto-repair path.
    """
    old=configparser.ConfigParser(interpolation=None)
    old.read_string("""[Input]
KeyboardLayout=Auto
ExperimentalNativeJoystickUnplugGuard=0
ExperimentalNativeJoystickReconnect=0
ExperimentalNativeJoystickMappingSwap=0
ExperimentalAltLatchRecovery=0
""")
    for key in ("ExperimentalNativeJoystickUnplugGuard",
                "ExperimentalNativeJoystickReconnect",
                "ExperimentalNativeJoystickMappingSwap",
                "ExperimentalAltLatchRecovery"):
        assert old.getint("Input",key,fallback=1)==0
    for key in ("ExperimentalSDL3Probe","ExperimentalSDL3NativeOwner",
                "ExperimentalSDL3GameplayBindings"):
        assert old.getint("Input",key,fallback=1)==1

    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3NativeOwner",1,ini)==1' in PROBE
    assert 'GetPrivateProfileIntA("Input","ExperimentalNativeJoystickUnplugGuard",1,ini)!=1' in PROBE
    assert 'GetPrivateProfileIntA("Input","ExperimentalAltLatchRecovery",1,ini)==1' in ALT
    validator=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    for key in ("ExperimentalNativeJoystickUnplugGuard",
                "ExperimentalNativeJoystickReconnect",
                "ExperimentalNativeJoystickMappingSwap",
                "ExperimentalAltLatchRecovery"):
        assert f'"{key}"' in validator
    assert "Duplicate key [%s] %s" in validator
    assert "spider_ini_uint(value,0,1)" in validator
    boot=CFG[CFG.index("void SpiderCfg_ValidateCurrentIni("):]
    assert "if(!check.errors && !check.warnings)" in boot
    assert 'INI-CHECK: valid SpiderMan2k1Fix.ini' in boot
    # Do not add a second occurrence to "turn on" an older setting.
    # An explicit edit of each existing line or moving the INI aside
    # is the safe, user-directed upgrade path.


def test_factory_input_promotion_never_duplicates_known_switches():
    generated=configparser.ConfigParser(interpolation=None,strict=True)
    generated.read_string(EXAMPLE)
    for key in (*KEYS,"ExperimentalSDL3XboxRTSwing"):
        assert list(generated["Input"]).count(key.lower())==1
        assert generated["Input"][key]=="1"
