"""Safety and reversible behavior of the opt-in shared-axis Xbox RT Swing bridge."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
POLICY=(ROOT/"xbox360_trigger_swing_policy.h").read_text(encoding="utf-8")
UI=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
CFG=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
API=(ROOT/"virtual_spidey_cfg.h").read_text(encoding="utf-8")
VAL=(ROOT/"ini_validation.h").read_text(encoding="utf-8")


def test_axis_hysteresis_only_negative_direction_engages(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"trigger.c"
    exe=tmp_path/"trigger"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "xbox360_trigger_swing_policy.h"
      int main(void) {
        int held=0;
        assert(!spider_xbox360_rt_swing_held(held,32768));
        assert(!spider_xbox360_rt_swing_held(held,65408)); /* LT full */
        assert(!spider_xbox360_rt_swing_held(held,43520)); /* LT partial */
        assert(!spider_xbox360_rt_swing_held(held,24000)); /* dead zone */
        held=spider_xbox360_rt_swing_held(held,17024);     /* RT -> on */
        assert(held);
        assert(spider_xbox360_rt_swing_held(held,25000));  /* hysteresis */
        assert(!spider_xbox360_rt_swing_held(held,28000));
        assert(!spider_xbox360_rt_swing_held(held,32768)); /* neutral */
        assert(spider_xbox360_rt_swing_held(0,128));       /* RT full */
        assert(!spider_xbox360_rt_swing_held(1,65408));   /* LT release */
        assert(!spider_xbox360_rt_swing_held(0,-1));
        assert(!spider_xbox360_rt_swing_held(1,70000));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                   "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_rtswing_disabled_by_default_and_requires_verified_startup_or_native_hotplug_xbox_mapping():
    assert 'ControllerXbox360RTSwing=0' in CFG
    assert '"ControllerXbox360RTSwing"' in VAL
    assert 'GetPrivateProfileIntA("Input","ControllerXbox360RTSwing",0,ini)==1' in UI
    assert 'int native_canonical=SpiderNativeReconnect_CanonicalXboxMapping();' in UI
    assert '(!xbox360_preview_active && !native_canonical)' in UI
    assert 'scan=native_canonical?SpiderCfg_Xbox360SwingScanLive():' in UI
    assert 'SpiderXbox360_SetRTAlias(xbox_rt_swing_requested &&' in UI
    assert 'detected_kind!=SPIDER_RECONNECT_XBOX360 || !gameplay_ok)' in UI
    assert 'SpiderCfg_Xbox360SwingScan()' in UI
    assert 'unsigned SpiderCfg_Xbox360SwingScan(void)' in CFG
    assert 'unsigned SpiderCfg_Xbox360SwingScan(void);' in API
    assert 'if(!ready || !SpiderCfg_PS1BootstrapLoaded())return 0;' in CFG
    assert 'if(!xbox360_preview_active) {LeaveCriticalSection(&guard);return 0;}' in CFG


def test_keydown_and_keyup_are_separate_from_one_frame_menu_escape():
    assert 'static void xbox_rt_swing_release(const char *why)' in UI
    assert 'static void xbox_rt_swing_update(long axis,int gameplay_ok)' in UI
    update=UI.split('static void xbox_rt_swing_update(',1)[1].split(
        'static void disable_profile_with_policy(',1)[0]
    assert 'if(xbox_rt_swing_down_scan)return;' in update
    assert 'spider_xbox360_rt_swing_held(' in update
    assert 'KEYEVENTF_SCANCODE;' in update
    assert 'xbox_rt_swing_down_scan=scan;' in update
    release=UI.split('static void xbox_rt_swing_release(',1)[1].split(
        'static void xbox_rt_swing_update(',1)[0]
    assert 'KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;' in release
    assert 'xbox_rt_swing_down_scan=0;' in release
    assert 'SpiderNativeReconnect_LogSyntheticMenuKey' not in update
    assert 'begin_scancode' not in update


def test_forced_keyup_on_focus_pause_input_failure_and_context_change():
    assert 'xbox_rt_swing_release("focus lost");' in UI
    assert 'xbox_rt_swing_release("secondary controller disabled");' in UI
    assert 'xbox_rt_swing_release("secondary DirectInput read failure");' in UI
    assert 'xbox_rt_swing_release("controller reconnect/edge quarantine");' in UI
    assert 'xbox_rt_swing_update((long)state.lZ,' in UI
    assert 'gameplay_3d && !menu_2d && !bink_active && !native_pause_active &&' in UI
    assert '!pause_requested);' in UI
    assert 'xbox_rt_swing_release("active device is not Xbox 360");' in UI


def test_only_unique_safe_game_owned_keyboard_swing_can_be_injected():
    scanner=CFG.split('unsigned SpiderCfg_Xbox360SwingScan(void) {',1)[1].split(
        'void SpiderCfg_CancelXbox360Preview(',1)[0]
    assert 'scan=(unsigned)get_word(60);' in scanner
    assert 'scan<2u || scan>0x7fu' in scanner
    assert 'scan==0x1cu' in scanner
    assert 'get_word(action_offsets[i])==scan' in scanner
    assert 'EnterCriticalSection(&guard)' in scanner
    assert 'LeaveCriticalSection(&guard)' in scanner
    assert 'set_word(' not in scanner
    assert 'WritePrivateProfileString' not in scanner


def test_hotplug_uses_live_authenticated_keyboard_scan_and_canonical_native_xbox():
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    head=(ROOT/"native_joystick_reconnect.h").read_text(encoding="utf-8")
    assert "int SpiderNativeReconnect_CanonicalXboxMapping(void)" in native
    assert "int SpiderNativeReconnect_CanonicalXboxMapping(void);" in head
    assert "if(!requested || !initialized || !swap_mapping_enabled" in native
    assert "spider_native_swap_read((const uint8_t *)table," in native
    assert "spider_native_swap_preset(buttons)==kind;" in native
    assert "spider_native_reconnect_canonical_mapping(SPIDER_RECONNECT_XBOX360)" in native
    assert "game_device_slot(),NULL,NULL" in native
    assert "unsigned SpiderCfg_Xbox360SwingScanLive(void)" in CFG
    assert "unsigned SpiderCfg_Xbox360SwingScanLive(void);" in API
    block=CFG.split('unsigned SpiderCfg_Xbox360SwingScanLive(void) {',1)[1].split(
        'void SpiderCfg_CancelXbox360Preview(',1)[0]
    assert "const uintptr_t table=0x00568398u;" in block
    assert "VirtualQuery((const void *)table,&mbi,sizeof(mbi))" in block
    assert "mask!=masks[i]" in block
    assert "scans[i]==scan" in block
    assert "set_word(" not in block
    assert "WritePrivateProfileString" not in block
    assert "SendInput(" not in block
    assert "if(!xbox360_preview_active)" not in block
