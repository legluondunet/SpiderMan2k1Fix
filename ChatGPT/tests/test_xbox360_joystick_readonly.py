"""Regression: a detected Xbox pad must NOT edit original native Joystick
Configuration, while Keyboard remains modifiable and Sony PS1 behavior
is unchanged. Exact PE32 signatures still gate all write operations.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
READONLY=(ROOT/"ps1_joystick_readonly_menu.c").read_text()
LABELS=(ROOT/"ps1_joystick_readonly_menu.h").read_text()
UI=(ROOT/"gamepad_ui_profile.c").read_text()
API=(ROOT/"gamepad_ui_profile.h").read_text()
EXE=(ROOT/"spiderman2k1fix.c").read_text()


def test_verified_xbox_gets_native_joystick_only_lock_at_bootstrap_and_late_hotplug():
    assert "void SpiderGamepadUI_ArmXbox360ReadOnly(void)" in UI
    assert "void SpiderGamepadUI_ArmXbox360ReadOnly(void);" in API
    assert "SpiderGamepadUI_ArmXbox360ReadOnly();" in EXE
    assert EXE.index("SpiderCfg_BootstrapGameRead();") < EXE.index(
        "SpiderGamepadUI_ArmXbox360ReadOnly();"
    )
    assert "xbox_readonly_armed=0,xbox_readonly_attempted=0" in UI
    assert "xbox_readonly_attempted=1;" in UI
    assert "if(!SpiderXbox360_InstallReadOnlyJoystick(log_message))" in UI
    assert "detected_kind!=SPIDER_RECONNECT_XBOX360" in UI
    assert "profile==SPIDER_PROFILE_OFF" in UI
    assert "profile==SPIDER_PROFILE_DUALSHOCK4" in UI
    assert "xbox_try_readonly_menu();" in UI
    assert "if(!detected||!device)" in UI
    assert "GAMEPAD-XBOX360: JOYSTICK-READONLY REFUSED" in UI


def test_shared_readonly_patch_remains_single_install_verified_x86_and_no_keyboard_lock():
    assert "int SpiderXbox360_InstallReadOnlyJoystick(void (*logger)(const char *))" in READONLY
    assert "int SpiderPS1_InstallReadOnlyJoystick(void (*logger)(const char *))" in READONLY
    assert "return spider_install_readonly_joystick(2u,logger);" in READONLY
    assert "return spider_install_readonly_joystick(1u,logger);" in READONLY
    assert "if(readonly_installed_mode)" in READONLY
    assert "if(readonly_installed_mode==mode)return 1;" in READONLY
    assert "readonly_installed_mode=mode;" in READONLY
    assert "readonly_label_mode=mode;" in READONLY
    assert "memcmp(location,p->original,p->size)!=0" in READONLY
    assert "nt->OptionalHeader.ImageBase!=0x00400000u" in READONLY
    assert "for(i=0;i<SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT;i++)" in READONLY
    assert "while(installed>0u)" in READONLY
    assert "0x0050D872u-" in READONLY
    assert "0x0050D400u-" in READONLY
    assert "0x9Cu,0x12u,0xACu,0x00u" in READONLY
    assert "Keyboard editable" in READONLY
    assert "GAMEPAD-XBOX360: JOYSTICK-READONLY ACTIVE" in READONLY
    assert "spider_xbox360_native_button_label(button)" in READONLY
    assert "spider_ps1_native_button_label(button)" in READONLY
    assert "VirtualProtect(" not in UI
    assert "VirtualProtect(" not in EXE.split("SpiderGamepadUI_ArmXbox360ReadOnly();", 1)[0].split(
        "SpiderCfg_BootstrapGameRead();", 1)[-1]


def test_xbox_labels_only_claim_observed_a_and_b_and_fallback_to_numeric(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"labels.c"
    out=tmp_path/"labels"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <string.h>
        #include "ps1_joystick_readonly_menu.h"
        int main(void) {
          assert(!strcmp(spider_xbox360_native_button_label(0),"A"));
          assert(!strcmp(spider_xbox360_native_button_label(1),"B"));
          assert(spider_xbox360_native_button_label(2)==0);
          assert(spider_xbox360_native_button_label(7)==0);
          assert(spider_xbox360_native_button_label(8)==0);
          assert(!strcmp(spider_ps1_native_button_label(0),"SQUARE"));
          assert(!strcmp(spider_ps1_native_button_label(1),"CROSS"));
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
                    str(src),"-o",str(out)],check=True)
    subprocess.run([str(out)],check=True)
