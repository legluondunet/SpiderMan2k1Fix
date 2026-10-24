"""Regressions for separate keyboard and joystick menu bindings."""
from pathlib import Path
import configparser
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
VIRTUAL = (ROOT / "virtual_spidey_cfg.c").read_text()
INI = (ROOT / "ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini").read_text()
DEFAULT = (ROOT / "ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
ACTIONS = ("Up", "Down", "Left", "Right", "Jump", "Kick", "Punch", "Web", "Aim", "Zipline", "Swing")
BUTTON_ACTIONS = ("Jump", "Kick", "Punch", "Web", "Aim", "Zipline", "Swing")


def test_two_independent_sections():
    for content in (INI, DEFAULT):
        parsed = configparser.ConfigParser(interpolation=None)
        parsed.read_string(content)
        assert "Keyboard" in parsed and "Joystick" in parsed
        assert "Internal" in parsed
        assert not parsed.has_section("Controls")
        assert not parsed.has_section("Gamepad")
        assert not parsed.has_section("GameConfig")
        assert set(parsed["Keyboard"]) == {k.lower() for k in ACTIONS}
        assert set(parsed["Joystick"]) == {k.lower() for k in BUTTON_ACTIONS}
        assert content.count("\nRawHex=") == 1
        assert len(bytes.fromhex(parsed["Internal"]["RawHex"])) == 252
        assert {key: parsed["Keyboard"][key] for key in ACTIONS} == {
            "Up":"UP", "Down":"DOWN", "Left":"LEFT", "Right":"RIGHT",
            "Jump":"SPACE", "Kick":"X", "Punch":"C", "Web":"V",
            "Aim":"D", "Zipline":"F", "Swing":"G"
        }
        assert [int(parsed["Audio"][k]) for k in ("MusicSFXLevel","VoiceLevel","MovieLevel")] == [69,79,69]
        raw=bytes.fromhex(parsed["Internal"]["RawHex"])
        for i,key in enumerate(BUTTON_ACTIONS):
            code=struct.unpack_from('<I',raw,0x50+i*4)[0]
            assert parsed["Joystick"][key] == f"BUTTON{code}"
        # Unidentified joystick movement values are never touched by the mapping.
        assert struct.unpack_from('<4I',raw,0x40) == (16384,)*4


def test_read_write_and_auto_ini():
    for token in ('GetPrivateProfileStringA("Keyboard",action_names[i]',
                  'WritePrivateProfileStringA("Keyboard",action_names[i]',
                  'GetPrivateProfileStringA("Joystick",joystick_actions[i]',
                  'WritePrivateProfileStringA("Joystick",joystick_actions[i]',
                  'GetPrivateProfileStringA("Internal","RawHex"',
                  'WritePrivateProfileStringA("Internal","RawHex"',
                  'load_keyboard();', 'load_joystick();', 'save_keyboard()', 'save_joystick()',
                  'decode_joystick_button', 'joystick_offsets[7]={0x50,0x54,0x58,0x5C,0x60,0x64,0x68}',
                  r'"\r\n[Keyboard]\r\n"', r'"\r\n[Joystick]\r\n"'):
        assert token in VIRTUAL, token
    assert 'GetPrivateProfileStringA("Input","KeyboardLayout","Off"' in VIRTUAL
    assert 'load_keyboard_layout();' in VIRTUAL
    assert VIRTUAL.index('load_keyboard_layout();') < VIRTUAL.index('load_keyboard();')
    assert '"Controls"' not in VIRTUAL
    assert 'GetPrivateProfileStringA("GameConfig",' not in VIRTUAL
