"""Fail-closed original PCShell joystick-only read-only experiment.

Machine bytes are from original 32-bit PCShell functions mapped by
krystalgamer/spidey-decomp (names.json, PCShell.cpp and per-function
binary excerpts). They MUST match the exact supported retail SpideyPC.exe
at runtime, or the whole experiment is refused.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
HEADER=(ROOT/"ps1_joystick_readonly_menu.h").read_text()
SOURCE=(ROOT/"ps1_joystick_readonly_menu.c").read_text()
BRIDGE=(ROOT/"gamepad_ui_profile.c").read_text()
INI=(ROOT/"ini_validation.h").read_text()
CONFIG=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_detected_supported_ds4_always_gets_native_joystick_viewer():
    assert 'void SpiderGamepadUI_ArmPS1ReadOnly(void)' in BRIDGE
    assert 'SpiderGamepadUI_ArmPS1ReadOnly();' in (ROOT/'spiderman2k1fix.c').read_text()
    assert 'void SpiderGamepadUI_ArmPS1ReadOnly(void);' in (ROOT/'gamepad_ui_profile.h').read_text()
    assert 'profile==SPIDER_PROFILE_OFF' in BRIDGE
    assert 'detected_kind!=SPIDER_RECONNECT_DS4' in BRIDGE
    assert 'SpiderPS1_InstallReadOnlyJoystick(log_message)' in BRIDGE
    assert 'ControllerPS1ReadOnlyJoystick' in INI  # backwards-compatible INI validation
    assert 'ControllerPS1ReadOnlyJoystick=1' not in CONFIG
    assert 'ps1_readonly_menu_requested' not in BRIDGE
    assert 'VirtualProtect(' not in BRIDGE


def test_native_both_footer_rows_disabled_and_rebinding_gated():
    assert "SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT=8" in HEADER
    assert "SPIDER_PS1_JOYSTICK_RESTORE_ROW_SLOT=6" in HEADER
    assert "SPIDER_PS1_JOYSTICK_EMPTY_ROW_SLOT=7" in HEADER
    for original_va in ("0x0050D28Cu","0x0050D4A9u","0x0050D51Cu",
                        "0x0050D6A6u","0x0050D7ADu","0x0050D3FAu",
                        "0x0050D2C8u","0x0050D2D8u"):
        assert original_va in HEADER
    for original_bytes in (
        "0xE8u,0x23u,0x2Du,0xF3u,0xFFu",
        "0xE8u,0x13u,0x2Du,0xF3u,0xFFu",
    ):
        assert original_bytes in HEADER
    assert "const uintptr_t start=0x0050D210u" not in SOURCE
    assert "spider_find_native_restore_row" not in SOURCE
    assert "memcpy(live_sites,spider_ps1_readonly_sites,sizeof(live_sites));" in SOURCE
    assert "memcmp(location,p->original,p->size)!=0" in SOURCE
    assert "VirtualQuery(location,&mbi,sizeof(mbi))" in SOURCE
    assert "VirtualProtect(address,n,PAGE_EXECUTE_READWRITE,&old)" in SOURCE
    assert "FlushInstructionCache(GetCurrentProcess(),address,n)" in SOURCE
    assert "if(installed!=SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT)" in SOURCE
    assert "while(installed>0u)" in SOURCE
    assert "for(i=SPIDER_PS1_JOYSTICK_RESTORE_ROW_SLOT;" in SOURCE
    assert "i<=SPIDER_PS1_JOYSTICK_EMPTY_ROW_SLOT;i++)" in SOURCE
    assert "spider_joystick_restore_row_addentry" in SOURCE
    assert "static void __fastcall spider_joystick_restore_row_addentry(" in SOURCE
    assert "typedef void (__thiscall *SpiderCMenuAddEntry)(void *,const char *);" in SOURCE
    assert "0x00AC129Cu!=0u" in SOURCE
    assert "((SpiderCMenuAddEntry)(uintptr_t)0x0043FFF0u)(native_menu,label);" in SOURCE
    assert "replacements[i][0]=0xE8u;" in SOURCE
    assert "0x0050D852u-(p->virtual_address+5u)" in SOURCE
    assert "memcpy(replacements[4],nop5,5u)" in SOURCE
    assert "spider_joystick_restore_denied" in SOURCE
    assert "native capture entry blocked for Joystick only" in SOURCE
    assert "nt->OptionalHeader.ImageBase!=0x00400000u" in SOURCE
    # Keyboard configuration remains editable and its Restore Default row is preserved
    assert "Keyboard restore preserved" in SOURCE
    assert "0x0050A4A0u" not in HEADER
    assert "0x0050A4E0u" not in HEADER


def test_native_ps1_labels_are_the_actual_controller_indices():
    for number,name in {0:"SQUARE",1:"CROSS",2:"CIRCLE",
                        3:"TRIANGLE",4:"L1",5:"R1",7:"R2"}.items():
        assert f'case {number}u: return "{name}";' in HEADER
    assert "SPIDER_PS1_JOYSTICK_LABEL_CAPACITY=32" in HEADER
    assert "spider_ps1_native_button_label(button)" in SOURCE
    assert "return snprintf(destination,SPIDER_PS1_JOYSTICK_LABEL_CAPACITY" in SOURCE
    assert '"button %u",button)' in SOURCE


def test_host_compiled_label_mapping_and_factory_signature(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    program=tmp_path/"menu.c"
    executable=tmp_path/"menu"
    program.write_text(textwrap.dedent(r"""
        #include <string.h>
        #include "ps1_joystick_readonly_menu.h"
        int main(void) {
          static const char* expected[]={
            "SQUARE","CROSS","CIRCLE","TRIANGLE","L1","R1",0,"R2"
          };
          unsigned i;
          for(i=0;i<8;i++){
            const char *name=spider_ps1_native_button_label(i);
            if(name!=expected[i] && (!name||!expected[i]||
               strcmp(name,expected[i])!=0))return 1;
          }
          if(spider_ps1_native_button_label(31))return 2;
          if(spider_ps1_readonly_sites[0].virtual_address!=0x50D28Cu)return 3;
          if(spider_ps1_readonly_sites[1].virtual_address!=0x50D4A9u)return 4;
          if(spider_ps1_readonly_sites[2].virtual_address!=0x50D51Cu)return 5;
          if(spider_ps1_readonly_sites[3].virtual_address!=0x50D6A6u)return 6;
          if(spider_ps1_readonly_sites[4].virtual_address!=0x50D7ADu)return 7;
          if(spider_ps1_readonly_sites[5].virtual_address!=0x50D3FAu)return 8;
          if(spider_ps1_readonly_sites[3].size!=6u)return 9;
          if(spider_ps1_readonly_sites[5].size!=6u)return 10;
          if(spider_ps1_readonly_sites[6].virtual_address!=0x50D2C8u)return 11;
          if(spider_ps1_readonly_sites[7].virtual_address!=0x50D2D8u)return 12;
          if(spider_ps1_readonly_sites[6].size!=5u)return 13;
          if(spider_ps1_readonly_sites[7].size!=5u)return 14;
          if(SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT!=8u)return 15;
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(program),"-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
