"""AZERTY key positions and safe legacy INI compatibility.

The pure-C policy is tested on Linux; native GetKeyboardLayout and Wine menu
labels still require a real game run, not merely a successful DLL build.
"""

from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
VIRTUAL = (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
HEADER = (ROOT / "keyboard_layout.h").read_text(encoding="utf-8")


def test_legacy_and_azerty_semantic_binding_positions(tmp_path):
    if not shutil.which("gcc"):
        pytest.skip("host C compiler unavailable")
    src = tmp_path / "layout.c"
    binary = tmp_path / "layout"
    src.write_text(r'''
#include <assert.h>
#include "keyboard_layout.h"

int main(void) {
  /* Legacy physical names preserve the original DIK layout. */
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_PHYSICAL,'A',0x1E,0x10)==0x1E);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_PHYSICAL,'M',0x32,0x27)==0x32);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_QWERTY,'Z',0x2C,0x11)==0x2C);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_QWERTY,'W',0x11,0x2C)==0x11);
  /* French AZERTY maps letter names onto the user's physical letter keys. */
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AZERTY,'A',0x1E,0)==0x10);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AZERTY,'Q',0x10,0)==0x1E);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AZERTY,'Z',0x2C,0)==0x11);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AZERTY,'W',0x11,0)==0x2C);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AZERTY,'M',0x32,0)==0x27);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AZERTY,'F',0x21,0)==0x21);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AZERTY,'C',0x2E,0)==0x2E);
  /* Auto uses a valid host scan, otherwise fails closed to physical. */
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'M',0x32,0x27)==0x27);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'M',0x32,0)==0x32);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'W',0x11,0x200)==0x11);
  return 0;
}
''', encoding="utf-8")
    subprocess.run(
        ["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror",
         "-I", str(ROOT), str(src), "-o", str(binary)],
        check=True, capture_output=True, text=True
    )
    subprocess.run([str(binary)], check=True, capture_output=True, text=True)


def test_runtime_maps_ini_letter_names_without_touching_directinput():
    assert 'MapVirtualKeyExA(' in VIRTUAL
    assert 'GetKeyboardLayout(0)' in VIRTUAL
    assert 'GetPrivateProfileStringA("Input","KeyboardLayout","Physical"' in VIRTUAL
    assert "SPIDER_KEYBOARD_AZERTY" in VIRTUAL
    assert "SpiderKeyboard_MapLetter(" in VIRTUAL
    assert "decode_key(value,&code)" in VIRTUAL
    assert "letter_scan(key[0],dik_names[i].scan)" in VIRTUAL
    assert "physical DirectInput" in VIRTUAL
    assert "original menu key labels may remain QWERTY" in VIRTUAL
    assert 'keyboard_layout_mode=SPIDER_KEYBOARD_PHYSICAL;' in VIRTUAL


def test_game_settings_stay_raw_and_other_devices_unchanged():
    assert 'set_word(action_offsets[i],code);' in VIRTUAL
    assert "joystick_offsets[7]={0x50,0x54,0x58,0x5C,0x60,0x64,0x68}" in VIRTUAL
    assert "0x27u" in HEADER
    assert "virtual mode ENABLED" in VIRTUAL


def test_cheats_native_key_name_remap_is_narrow_and_signature_guarded():
    names = (ROOT / "keyboard_names.c").read_text(encoding="utf-8")
    build = (ROOT / "build.sh").read_text(encoding="utf-8")
    assert "keyboard_names.c" in build
    assert '#include "keyboard_names.h"' in VIRTUAL
    assert "SpiderKeyNames_Install(keyboard_layout_mode,keyboard_layout_hkl,report);" in VIRTUAL
    assert "0x0048E048u" in names and "0x00501550u" in names
    assert "0x00562FF4u" in names
    assert "original_text_path_verified()" in names
    assert "memcmp(" in names
    assert "VirtualProtect(" in names
    assert "MapVirtualKeyExA('M',MAPVK_VK_TO_VSC,hkl)==0x27" in names
    for entry in ("table[0x10]=old_a", "table[0x1E]=old_q",
                  "table[0x11]=old_z", "table[0x2C]=old_w",
                  "table[0x27]=old_m", "table[0x32]=old_comma"):
        assert entry in names
    assert "GetDeviceData" not in names
    assert "DirectInput8Create" not in names
