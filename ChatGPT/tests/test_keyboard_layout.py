"""Only Auto/Off is user-facing; validate scan-map consistency on the host.
Windows and Wine CHEATS/menu behavior still require hands-on game testing.
"""
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
VIRTUAL = (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
HEADER = (ROOT / "keyboard_layout.h").read_text(encoding="utf-8")
NAMES = (ROOT / "keyboard_names.c").read_text(encoding="utf-8")

def test_complete_layout_mapping_off_and_safe_fallback(tmp_path):
    if not shutil.which("gcc"):
        pytest.skip("host C compiler unavailable")
    src = tmp_path / "layout.c"
    binary = tmp_path / "layout"
    src.write_text(r'''
#include <assert.h>
#include "keyboard_layout.h"
int main(void) {
  unsigned host[26], i;
  unsigned char active[26]={0};
  /* US/QWERTY is an identity transform. */
  for(i=0;i<26;i++)host[i]=spider_us_letter_scans[i];
  assert(SpiderKeyboard_ValidateMap(host,active)==1);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'M',0x32,active,1)==0x32);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_OFF,'M',0x32,active,1)==0x32);
  /* All letters on standard French AZERTY, including physical M/semicolon. */
  host['A'-'A']=0x10;host['Q'-'A']=0x1e;
  host['Z'-'A']=0x11;host['W'-'A']=0x2c;
  host['M'-'A']=0x27;
  assert(SpiderKeyboard_ValidateMap(host,active)==1);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'A',0x1e,active,1)==0x10);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'Q',0x10,active,1)==0x1e);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'Z',0x2c,active,1)==0x11);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'W',0x11,active,1)==0x2c);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'M',0x32,active,1)==0x27);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_OFF,'M',0x32,active,1)==0x32);
  /* A different valid layout, QWERTZ, must not require a hard-coded mode. */
  for(i=0;i<26;i++)host[i]=spider_us_letter_scans[i];
  host['Y'-'A']=0x2c;host['Z'-'A']=0x15;
  assert(SpiderKeyboard_ValidateMap(host,active)==1);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'Z',0x2c,active,1)==0x15);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'Y',0x15,active,1)==0x2c);
  /* Never use an incomplete, extended or duplicate layout map. */
  host['A'-'A']=host['B'-'A'];
  assert(SpiderKeyboard_ValidateMap(host,active)==0);
  assert(SpiderKeyboard_MapLetter(SPIDER_KEYBOARD_AUTO,'M',0x32,active,0)==0x32);
  host['A'-'A']=0x140;
  assert(SpiderKeyboard_ValidateMap(host,active)==0);
  host['A'-'A']=0;
  assert(SpiderKeyboard_ValidateMap(host,active)==0);
  return 0;
}
''', encoding="utf-8")
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(binary)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True,capture_output=True,text=True)

def test_autodetection_runtime_and_deprecated_ini_migration():
    assert 'GetPrivateProfileStringA("Input","KeyboardLayout","Off"' in VIRTUAL
    assert 'keyboard_layout_mode=SPIDER_KEYBOARD_OFF;' in VIRTUAL
    assert 'keyboard_layout_mode=SPIDER_KEYBOARD_AUTO;' in VIRTUAL
    assert "MapVirtualKeyExA('A'+i,MAPVK_VK_TO_VSC,keyboard_layout_hkl)" in VIRTUAL
    assert "SpiderKeyboard_ValidateMap(mapped,keyboard_scans)" in VIRTUAL
    assert "SpiderKeyboard_MapLetter(keyboard_layout_mode,letter" in VIRTUAL
    assert 'WritePrivateProfileStringA("Input","KeyboardLayout",effective,ini_file)' in VIRTUAL
    assert "SpiderKeyNames_Install(keyboard_layout_mode,keyboard_layout_hkl," in VIRTUAL
    assert "SPIDER_KEYBOARD_AZERTY" not in VIRTUAL
    assert "SPIDER_KEYBOARD_QWERTY" not in VIRTUAL

def test_cheats_table_is_guarded_and_reuses_original_letter_names():
    build = (ROOT / "build.sh").read_text(encoding="utf-8")
    assert "keyboard_names.c" in build
    assert "0x0048E048u" in NAMES and "0x00501550u" in NAMES
    assert "0x00562FF4u" in NAMES
    assert "original_text_path_verified()" in NAMES
    assert "memcmp(" in NAMES
    assert "VirtualProtect(" in NAMES
    assert "spider_us_letter_scans[i]" in NAMES
    assert "replacement[letter_scans[i]]=original[spider_us_letter_scans[i]];" in NAMES
    assert "native_nonletter_name" in NAMES
    assert "GetDeviceData" not in NAMES
    assert "DirectInput8Create" not in NAMES

def test_action_controls_and_other_devices_remain_physical():
    assert 'set_word(action_offsets[i],code);' in VIRTUAL
    assert "joystick_offsets[7]={0x50,0x54,0x58,0x5C,0x60,0x64,0x68}" in VIRTUAL
    assert "virtual mode ENABLED" in VIRTUAL
