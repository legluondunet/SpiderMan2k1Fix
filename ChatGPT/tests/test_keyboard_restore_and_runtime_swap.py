"""Original PC keyboard Restore Default and explicitly gated native pad swap."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()
CFG_H=(ROOT/"virtual_spidey_cfg.h").read_text()
MENU=(ROOT/"ps1_joystick_readonly_menu.c").read_text()
NATIVE=(ROOT/"native_joystick_reconnect.c").read_text()
PROBE=(ROOT/"native_joystick_swap_probe.h").read_text()
MAIN=(ROOT/"spiderman2k1fix.c").read_text()
INI=(ROOT/"ini_validation.h").read_text()


def test_modern_keyboard_restore_changes_only_original_default_source():
    # Direct original PCINPUT_RestoreDefaultKeyboardSettings 0x50A520:
    # mov esi,0x53C87C; mov edi,0x568398; rep movsd (28 DWORDs).
    assert "int SpiderCfg_InstallModernKeyboardDefaults(void)" in CFG
    assert "int SpiderCfg_InstallModernKeyboardDefaults(void);" in CFG_H
    assert "SpiderCfg_InstallModernKeyboardDefaults();" in MAIN
    assert "0x0053C87Cu" in CFG
    defaults=CFG.split("int SpiderCfg_InstallModernKeyboardDefaults(void) {",1)[1].split("static void load_keyboard(",1)[0]
    assert "0x00568398u" not in defaults  # defaults writer NEVER touches customized mapping
    assert "0x00568398u" not in CFG  # retired hard-coded Xbox Swing scan
    for source in ("0xC8u,0xD0u,0xCBu,0xCDu,0x50u,0x4Du,0x4Bu",
                   "0x48u,0x47u,0x51u,0x49u"):
        assert source in CFG
    for letter in ("X","C","V","D","F","G"):
        assert f"letter_scan('{letter}'" in CFG
    for preflight in ("mask!=masks[i] || scan!=legacy[i]",
                      "VirtualQuery((const void *)table,&mbi,sizeof(mbi))",
                      "VirtualProtect((void *)table,0x70u,PAGE_READWRITE,&old)",
                      "VirtualProtect((void *)table,0x70u,old,&ignored)"):
        assert preflight in CFG
    assert "for(i=0;i<11u;i++)" in CFG
    # Remaining Keyboard entries (Enter/Tab/terminator) remain unchanged.
    assert "modern[i]=" in CFG
    assert "SPIDEY-KEYBOARD-DEFAULT: ACTIVE" in CFG
    assert "memcpy(replacements[2],live_sites[2].original,live_sites[2].size);" in MENU


def test_native_reconnect_never_applies_fixed_joystick_mapping_or_overrides_keyboard():
    assert "ExperimentalNativeJoystickMappingSwap=1" not in CFG
    assert "swap_mapping_enabled=0;" in NATIVE
    assert "swap_native_gameplay_mapping(" not in NATIVE
    assert "spider_ps1_buttons" not in NATIVE
    assert "spider_xbox360_buttons" not in NATIVE
    assert "spider_native_swap_preset(" not in NATIVE
    assert "SPIDER_NATIVE_ACTION_TABLE 0x00568408u" in NATIVE
    assert "if(!neutral_and_flush(candidate))" in NATIVE
    assert "InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot()" in NATIVE
    assert "spider_native_swap_read(" in NATIVE
    assert "SPIDER_NATIVE_SWAP_SLOTS=14" in PROBE
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" in NATIVE
    assert "VirtualProtect((void *)address,0x70u,PAGE_READWRITE,&old)" not in NATIVE
    assert "spider_ps1_apply(" not in CFG
    assert "spider_xbox360_apply(" not in CFG
    assert "SPIDEY-KEYBOARD-DEFAULT: ACTIVE" in CFG

def test_keyboard_and_joystick_are_independently_locked():
    assert "0x00AC129Cu!=0u" in MENU
    assert "Keyboard restore preserved" in MENU
    assert "0x0050D852u-(p->virtual_address+5u)" in MENU
    assert "replacements[3][0]=0xE9u;" in MENU
    assert "replacements[2][0]=0xE9u;" not in MENU
    assert 'for(i=SPIDER_PS1_JOYSTICK_RESTORE_ROW_SLOT;' in MENU
