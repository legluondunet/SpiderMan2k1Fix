"""Regression: SDL3 profiles own gamepad mapping; original joystick bytes stay in RawHex."""
from pathlib import Path
import configparser
import struct

ROOT = Path(__file__).resolve().parents[2]
VIRTUAL = (ROOT / "virtual_spidey_cfg.c").read_text()
LEGACY = (ROOT / "ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini").read_text()
DEFAULT = (ROOT / "ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
ACTIONS = ("Up", "Down", "Left", "Right", "Jump", "Kick", "Punch",
           "Web", "Aim", "Zipline", "Swing")


def parse(text):
    p = configparser.ConfigParser(interpolation=None)
    p.read_string(text)
    return p


def test_new_ini_exposes_keyboard_but_no_original_joystick_bindings():
    cfg = parse(DEFAULT)
    assert "Keyboard" in cfg and "Internal" in cfg
    assert not cfg.has_section("Joystick")
    assert not cfg.has_section("Gamepad")
    assert not cfg.has_section("Controls")
    assert set(cfg["Keyboard"]) == {k.lower() for k in ACTIONS}
    assert DEFAULT.count("\nRawHex=") == 1
    raw = bytes.fromhex(cfg["Internal"]["RawHex"])
    assert len(raw) == 252
    # Original native joystick map and axis metadata are kept byte-for-byte.
    assert struct.unpack_from('<7I',raw,0x50) == (3,4,0,1,6,9,7)
    assert struct.unpack_from('<4I',raw,0x40) == (16384,)*4
    assert cfg["Keyboard"]["Jump"] == "SPACE"


def test_old_ini_still_parses_but_joystick_is_not_a_runtime_override():
    old = parse(LEGACY)
    assert old.has_section("Joystick")  # Legacy user inputs remain accepted.
    assert old["Joystick"]["Jump"] == "BUTTON3"
    assert 'GetPrivateProfileStringA("Joystick",' not in VIRTUAL
    assert 'WritePrivateProfileStringA("Joystick",' not in VIRTUAL
    assert 'load_joystick();' not in VIRTUAL
    assert 'save_joystick()' not in VIRTUAL
    assert 'joystick_header' not in VIRTUAL
    assert 'GetPrivateProfileStringA("Internal","RawHex"' in VIRTUAL
    assert 'WritePrivateProfileStringA("Internal","RawHex"' in VIRTUAL
    assert 'memcpy(saved_cfg,cfg,CFG_SIZE);' in VIRTUAL
    assert 'load_keyboard();' in VIRTUAL and 'save_keyboard()' in VIRTUAL
