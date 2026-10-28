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


def test_ps1_readonly_menu_is_an_explicit_opt_in_only():
    assert '"ControllerPS1ReadOnlyJoystick"' in INI
    assert 'GetPrivateProfileIntA("Input","ControllerPS1ReadOnlyJoystick",0,ini)==1' in BRIDGE
    assert 'if(!detected || detected_kind!=SPIDER_RECONNECT_DS4 || !ps1_preview_requested)return;' in BRIDGE
    assert "SpiderPS1_InstallReadOnlyJoystick(log_message)" in BRIDGE
    assert 'JOYSTICK-READONLY REFUSED' in SOURCE
    assert 'JOYSTICK-READONLY EXPERIMENT ACTIVE' in SOURCE
    assert 'ControllerPS1ReadOnlyJoystick=1' not in CONFIG
    assert 'VirtualProtect(' not in BRIDGE


def test_only_original_verified_joystick_paths_are_changed():
    assert "SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT=6" in HEADER
    for original_va in ("0x0050D28Cu","0x0050D4A9u","0x0050D51Cu",
                        "0x0050D6A6u","0x0050D7ADu","0x0050D3FAu"):
        assert original_va in HEADER
    assert "0x0050D852u-(p->virtual_address+5u)" in SOURCE
    assert "replacements[0][0]=0xE8u" in SOURCE
    assert "replacements[i][0]=0xE9u" in SOURCE
    assert "replacements[1][0]=0xE8u;" in SOURCE
    assert "spider_joystick_restore_denied" in SOURCE
    assert "GAMEPAD-READONLY: RESTORE-DEFAULT intercepted" in SOURCE
    assert "spider_ps1_readonly_sites[1].virtual_address+5u" in SOURCE
    assert "memcpy(&replacements[1][1],&rel,sizeof(rel));" in SOURCE
    assert "memcpy(replacements[1],nop5,5u)" not in SOURCE
    assert "void __cdecl spider_joystick_restore_denied(void)" in SOURCE
    assert "memcpy(replacements[4],nop5,5u)" in SOURCE
    assert "memcmp(location,p->original,p->size)!=0" in SOURCE
    assert "VirtualQuery(location,&mbi,sizeof(mbi))" in SOURCE
    assert "VirtualProtect(address,n,PAGE_EXECUTE_READWRITE,&old)" in SOURCE
    assert "FlushInstructionCache(GetCurrentProcess(),address,n)" in SOURCE
    assert "if(installed!=SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT)" in SOURCE
    assert "while(installed>0u)" in SOURCE
    assert "VirtualAlloc(NULL,sizeof(code)" in SOURCE
    assert "PAGE_EXECUTE_READ,&old" in SOURCE
    assert "capture_thunk+11" in SOURCE
    assert "capture_thunk+22" in SOURCE
    assert "0x0050D872u-" in SOURCE
    assert "0x0050D400u-" in SOURCE
    assert "0x9Cu,0x12u,0xACu,0x00u" in SOURCE
    assert "replacement[5]=0x90u;" in SOURCE
    assert "native capture entry blocked for Joystick only" in SOURCE
    assert "nt->OptionalHeader.ImageBase!=0x00400000u" in SOURCE
    assert "int SpiderPS1_InstallReadOnlyJoystick(void (*logger)(const char *))" in SOURCE
    # Keyboard Configuration's original setter and update function
    # must NOT be replaced by a global keyboard/controller hook.
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
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(program),"-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
