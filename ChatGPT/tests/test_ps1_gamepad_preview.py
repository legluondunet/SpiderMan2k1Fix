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
START_MAP=(ROOT/"ps1_native_start_mapping.h").read_text()
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
    assert 'choose_ds4=d.found==1u && d.ds4_count==1u' in PAD
    assert 'detected && detected_kind==SPIDER_RECONNECT_XBOX360' in PAD
    assert 'if(!detected)' in PAD
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


def test_ps1_preview_uses_cross_only_for_menu_confirm_even_with_legacy_both():
    # Original [Input] ControllerEnterButtons=Both remains honored outside
    # PS1 preview, but OPTIONS/Start is intentionally redundant in PS1.
    assert "effective_confirm=spider_ps1_menu_confirm_mask(ps1_preview_active,(unsigned)confirm);" in PAD
    assert "confirm_edges=spider_ps1_menu_confirm_edges(pressed,(unsigned)previous_buttons,effective_confirm);" in PAD
    assert "native_request,pressed & effective_confirm);" in PAD
    assert "CROSS-only 2D/Bink/pause confirmation" in PAD
    assert "legacy 2D/Bink confirmation=ControllerEnterButtons" in PAD
    assert "OPTIONS=BUTTON9 is NOT synthetic Enter in PS1" in PAD
    # Preserve the explicit 3D cutscene-only experiment as separately opt-in.
    assert 'GetPrivateProfileIntA("Input","ControllerPS1OptionsSkip",0,ini)==1' in PAD
    assert "ps1_preview_active && ps1_options_skip_requested" in PAD
    assert "ControllerPS1OptionsSkip=1" not in CFG


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


def test_pause_uses_verified_3d_menu_ribbon_when_native_pointer_is_transient():
    # The user's successful runtime log shows pause_ctx=0 despite the
    # draw hook seeing the characteristic purple menu ribbon. This is a
    # passive positive renderer observation, not a guessed native pointer.
    assert "native_pause43_visible_near_flip(LONG now_flip)" in CORE
    assert "native_esc43_supported_monitor()" in CORE
    assert "InterlockedCompareExchange(&native_pause43_banner_flip,0,0)" in CORE
    assert "now_flip-ribbon_flip<=1" in CORE
    assert "native_pause43_visible_near_flip(count)" in CORE
    assert "0x00682950u" in CORE
    assert "menu_upscale_active" in CORE
    assert "(ps1_preview_active && native_pause_active && !movie_allowed)" in PAD


def test_scripted_3d_options_skip_is_opt_in_and_natively_permission_guarded():
    assert 'GetPrivateProfileIntA("Input","ControllerPS1OptionsSkip",0,ini)==1' in PAD
    assert '"ControllerPS1OptionsSkip"' in VALIDATOR
    assert "ps1_preview_active && ps1_options_skip_requested" in PAD
    assert "gameplay_3d && !menu_2d && !movie_allowed && !native_pause_active" in PAD
    assert "pause_pressed && !previous_pause" in PAD
    assert "VirtualQuery((LPCVOID)(uintptr_t)owner,&mbi,sizeof(mbi))" in PAD
    assert "mbi.State!=MEM_COMMIT" in PAD
    assert "mbi.RegionSize-(where-base)<0x1bcu" in PAD
    assert "scene_context!=1u || skip_allowed!=1u || script_active!=1u" in PAD
    assert "script_ip==0u || *native_request!=0u" in PAD
    assert "0x006A9038u" in PAD  # SHA verified native script-owner pointer
    assert "0x0060CFC4u" in PAD  # original game CutSceneSkip request
    assert "0x4BC3E0" in PAD     # original game script cleanup
    assert "*native_request=1u;" in PAD
    # No blind Triangle keyboard translation or always-on button injection.
    assert "ControllerPS1OptionsSkip=1" not in CFG
    assert 'ps1_options_skip_requested=0' in PAD


def test_ps1_gameplay_bindings_are_fail_closed_on_bootstrap_and_virtual_writes():
    assert "SpiderCfg_CancelPS1Preview();" in CORE
    assert "spider_ps1_restore(cfg,CFG_SIZE,original_joystick)" in CFG
    assert "PS1-PREVIEW ROLLBACK native game bootstrap unverified" in CFG
    assert "if(ps1_preview_active)" in CFG
    assert "spider_ps1_apply(cfg,CFG_SIZE)" in CFG
    assert "PS1-PREVIEW protected seven virtual joystick bindings" in CFG
    assert "UI menu lock remains pending" not in CFG
    assert "the game's\n       * own live action array" in CFG


def test_start_probe_is_read_only_and_removes_old_arbitrary_pe_size_limit():
    assert "ps1_probe_native_start_tables()" in PAD
    assert "pe->OptionalHeader.SizeOfImage>0x10000000u" in PAD
    assert "pe->OptionalHeader.SizeOfImage>0x2000000u" not in PAD
    assert "pe->FileHeader.NumberOfSections>32u" in PAD
    assert "memcmp(sec[j].Name,\".data\",5)" in PAD
    assert "!(sec[j].Characteristics&IMAGE_SCN_MEM_WRITE)" in PAD
    assert "VirtualQuery((LPCVOID)here,&mbi,sizeof(mbi))" in PAD
    assert "mbi.State!=MEM_COMMIT" in PAD
    assert 'GetPrivateProfileIntA("Input","ControllerPS1DeepStartProbe",0,profile_ini)==1' in PAD
    assert '"ControllerPS1DeepStartProbe"' in VALIDATOR
    assert "64u*1024u*1024u" in PAD
    assert "const size_t max_bytes=deep?" in PAD
    assert "if(budget>max_bytes)" in PAD
    assert "while(cursor+14u*8u<=budget)" in PAD
    assert "VirtualQuery((LPCVOID)here,&mbi,sizeof(mbi))" in PAD
    assert "region_end<=here" in PAD
    assert "offset+14u*8u<=safe" in PAD
    assert "if(!mbi.RegionSize)" not in PAD  # probe gates empty regions
    assert "queried-regions=%u cap-MiB=%u" in PAD
    assert "START-TABLE candidate=" in PAD
    assert "START-TABLE probe data-sections=" in PAD
    assert "READ-ONLY; NO table edits" in PAD
    # No native memory writes to START, no native menu patch, no COM hook.
    assert "SPIDER_START_MAP_BUTTON_OFFSET" not in PAD
    assert "InterlockedExchange((volatile LONG*)(void*)" not in PAD


def test_native_start_unbind_is_explicit_unsafe_preview_only():
    # Strictly optional; no generated INI default and no write unless
    # a full .data scan returns precisely one strict original map.
    assert '"ControllerPS1DisableNativeStart"' in VALIDATOR
    assert 'GetPrivateProfileIntA("Input","ControllerPS1DisableNativeStart",0,profile_ini)==1' in PAD
    assert '"ControllerPS1DisableNativeStart=1' not in CFG
    assert "if(disable_native_start)" in PAD
    assert "!deep || truncated || sections!=1u || matches!=1u" in PAD
    assert "strict_matches!=1u || !strict_candidate" in PAD
    assert "strict_candidate!=base+0x00168408u" in PAD
    assert "spider_ps1_start_strict_factory(" in PAD
    assert "InterlockedCompareExchange(" in PAD
    assert "SPIDER_PS1_START_BUTTON_FACTORY" in PAD
    assert "SPIDER_PS1_START_BUTTON_UNBOUND" in PAD
    assert "NATIVE-START-UNBIND REFUSED" in PAD
    assert "NATIVE-START-UNBIND EXPERIMENT ACTIVE" in PAD
    assert "original BUTTON11 retained" in PAD
    assert "START-TABLE candidate=" in PAD
    assert "strict=%u truncated-regions=%u" in PAD
    assert "SPIDER_PS1_START_BUTTON_UNBOUND 0x4000u" in START_MAP
    assert "SPIDER_PS1_START_BUTTON_FACTORY 11u" in START_MAP
    assert "SPIDER_PS1_START_BUTTON_OFFSET" in START_MAP
    assert "if(button!=SPIDER_PS1_START_BUTTON_UNBOUND)return 0;" in START_MAP
    assert "if(button!=SPIDER_PS1_START_BUTTON_FACTORY)return 0;" in START_MAP
    assert "if(button!=0u)return 0;" in START_MAP


def test_ps1_runtime_logs_native_start_unbound_state_and_button11_press():
    # The user's 02:05 log proved the Start CAS happened, but the old
    # fixed SHARE text incorrectly claimed "Start still unchanged".
    assert "native Start button still unchanged" not in PAD
    assert "ps1_native_start_unbound=1;" in PAD
    assert "state.rgbButtons[11]&0x80u" in PAD
    assert "native_start_pressed && !previous_native_start" in PAD
    assert "ps1_native_start_reports++<12u" in PAD
    assert "Start-RAM-unbound=%d native-pause-current=%d" in PAD
    assert "ORIGINAL-BUTTON11 PHYSICAL PRESS" in PAD
    assert "original Start RAM unbound (runtime behavior still to verify)" in PAD
    assert "original Start mapping retained" in PAD
    assert "previous_native_start=0;" in PAD
    assert "previous_native_start=native_start_pressed;" in PAD


def test_host_compiled_native_start_verifier_rejects_drift(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"native_start.c"
    exe=tmp_path/"native_start"
    src.write_text(textwrap.dedent(r"""
        #include <stdint.h>
        #include <string.h>
        #include "ps1_native_start_mapping.h"
        static void put(unsigned char *p,uint32_t v){
          p[0]=(unsigned char)v;
          p[1]=(unsigned char)(v>>8);
          p[2]=(unsigned char)(v>>16);
          p[3]=(unsigned char)(v>>24);
        }
        int main(void) {
          static const uint32_t actions[14]={
            1u,2u,4u,8u,16u,32u,64u,128u,256u,512u,
            1024u,4096u,8192u,32768u};
          unsigned char table[SPIDER_PS1_START_MAP_BYTES];
          unsigned i;
          for(i=0;i<14u;i++){
            uint32_t b=i<4u || i==12u?0x4000u:i==11u?11u:i==13u?0u:i-4u;
            put(table+8u*i,actions[i]);
            put(table+8u*i+4u,b);
          }
          if(!spider_ps1_start_strict_factory(table,sizeof(table)))return 1;
          if(spider_ps1_start_strict_factory(table,sizeof(table)-1u))return 2;
          if(spider_ps1_start_strict_factory(0,sizeof(table)))return 3;
          put(table+SPIDER_PS1_START_BUTTON_OFFSET,0x4000u);
          if(spider_ps1_start_strict_factory(table,sizeof(table)))return 4;
          put(table+SPIDER_PS1_START_BUTTON_OFFSET,11u);
          put(table+8u*8u,1234u);
          if(spider_ps1_start_strict_factory(table,sizeof(table)))return 5;
          put(table+8u*8u,actions[8u]);
          put(table+8u*0u+4u,0u);
          if(spider_ps1_start_strict_factory(table,sizeof(table)))return 6;
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
