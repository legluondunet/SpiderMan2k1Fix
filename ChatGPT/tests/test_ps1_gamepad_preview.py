"""PS1 gameplay preview must be reversible and must never silently change saves.

This candidate is a manual opt-in only until native joystick menu locking,
Start/SHARE replacement and all scene contexts are separately validated.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
MAP=(ROOT/"ps1_gamepad_mapping.h").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
PAD=(ROOT/"gamepad_ui_profile.c").read_text()
HEADER=(ROOT/"gamepad_ui_profile.h").read_text()
VALIDATOR=(ROOT/"ini_validation.h").read_text()


def test_mapping_matches_approved_ps1_layout():
    assert "0x50u,0x54u,0x58u,0x5Cu,0x60u,0x64u,0x68u" in MAP
    assert "1u,2u,0u,3u,4u,5u,7u" in MAP
    assert "SPIDER_PS1_CFG_SIZE=252" in MAP
    assert "SPIDER_PS1_BUTTON_COUNT=7" in MAP


def test_profile_requires_opt_in_and_known_pad_before_native_bootstrap():
    assert 'GetPrivateProfileIntA("Input","ControllerPS1Preview",0,ini)==1' in PAD
    assert '"ControllerPS1Preview"' in VALIDATOR
    assert 'SpiderGamepadUI_PreparePS1(early)' in CORE
    assert 'SpiderCfg_EnablePS1Preview()' in CORE
    assert 'SpiderCfg_BootstrapGameRead();' in CORE
    assert 'SpiderCfg_PS1BootstrapLoaded()' in CORE
    assert 'SpiderGamepadUI_ActivatePS1();' in CORE
    assert 'GetForegroundWindow()!=hwnd' in PAD
    assert 'd.found!=1u || d.ds4_count!=1u' in PAD
    assert 'if(!detected)' in PAD
    # No options default should silently enable an unvalidated fixed profile.
    generator=CFG.split('static int SpiderCfg_WriteDefaultIniAt',1)[1]
    assert '"ControllerPS1Preview=1' not in generator
    assert "ps1_preview_requested" in PAD


def test_user_joystick_bindings_preserved_on_any_virtual_save():
    assert 'spider_ps1_snapshot(cfg,CFG_SIZE,original_joystick)' in CFG
    assert 'spider_ps1_apply(cfg,CFG_SIZE)' in CFG
    assert 'spider_ps1_restore(saved_cfg,CFG_SIZE,original_joystick)' in CFG
    assert 'ps1_preview_active?(DWORD)original_joystick[i]' in CFG
    assert "if(ps1_preview_active && opens>opened && reads>served)" in CFG
    assert 'InterlockedExchange(&ps1_bootstrap_loaded,1)' in CFG


def test_share_menu_uses_separate_context_and_legacy_options_does_not_fire():
    assert '0x00682950u' in CORE
    assert 'native_pause_active' in PAD
    assert 'state.rgbButtons[8]' in PAD
    assert 'share_pressed && !previous_share' in PAD
    assert 'ps1_preview_active && (gameplay_allowed || native_pause_active)' in PAD
    assert '!ps1_preview_active && gameplay_allowed' in PAD
    assert 'begin_scancode(0x01u,"Escape");' in PAD
    assert 'previous_share=share_pressed;' in PAD
    assert 'void SpiderGamepadUI_ActivatePS1(void)' in PAD
    assert 'void SpiderGamepadUI_ActivatePS1(void);' in HEADER
    # No unproven global COM hooks or unsigned native Start table patch.
    assert 'IDirectInputDevice8_GetDeviceData(' not in PAD
    assert 'VirtualProtect(' not in PAD
    assert 'gamepad_start_native.c' not in CORE


def test_host_compiled_ps1_snapshot_apply_restore_is_byte_exact(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"check.c"
    target=tmp_path/"check"
    source.write_text(textwrap.dedent(r"""
        #include <stdint.h>
        #include <string.h>
        #include "ps1_gamepad_mapping.h"
        int main(void) {
          uint8_t original[SPIDER_PS1_CFG_SIZE],runtime[SPIDER_PS1_CFG_SIZE];
          uint32_t snapshot[SPIDER_PS1_BUTTON_COUNT];
          size_t i;
          for(i=0;i<SPIDER_PS1_CFG_SIZE;i++)
            original[i]=(uint8_t)((i*33u+17u)&0xffu);
          /* Deliberately non-default player preferences, 7 distinct actions. */
          for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
            spider_ps1_write_word(original,spider_ps1_offsets[i],(uint32_t)(13u+i));
          memcpy(runtime,original,sizeof(runtime));
          if(!spider_ps1_snapshot(runtime,sizeof(runtime),snapshot))return 1;
          if(!spider_ps1_apply(runtime,sizeof(runtime)))return 2;
          for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
            if(spider_ps1_read_word(runtime,spider_ps1_offsets[i])
               !=spider_ps1_buttons[i])return 3;
          if(!spider_ps1_restore(runtime,sizeof(runtime),snapshot))return 4;
          if(memcmp(original,runtime,sizeof(runtime)))return 5;
          if(spider_ps1_apply(runtime,sizeof(runtime)-1u))return 6;
          if(spider_ps1_snapshot(runtime,sizeof(runtime)-1u,snapshot))return 7;
          if(spider_ps1_restore(runtime,sizeof(runtime)-1u,snapshot))return 8;
          if(memcmp(original,runtime,sizeof(runtime)))return 9;
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(target)],check=True)
    subprocess.run([str(target)],check=True)
