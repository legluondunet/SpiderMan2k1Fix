"""Early Xbox 360-compatible WIRED 045E:028E DirectInput support.

The user's hardware enumerates as Holtek with the Microsoft VID/PID.
A/B order is a hypothesis until Wine hardware logs confirm the DI indices.
The original Sony DS4 Auto/PS1 path must remain unchanged.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
IDENTITY=(ROOT/"controller_device_identity.h").read_text()
UI=(ROOT/"gamepad_ui_profile.c").read_text()
RECONNECT=(ROOT/"native_joystick_reconnect.c").read_text()
POLICY=(ROOT/"native_joystick_reconnect_policy.h").read_text()
INI=(ROOT/"ini_validation.h").read_text()
DEFAULTS=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_portable_identity_and_edge_detection(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc: return
    code=tmp_path/"xbox.c"
    exe=tmp_path/"xbox"
    code.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "controller_device_identity.h"
        #include "native_joystick_reconnect_policy.h"
        int main(void) {
          assert(spider_pad_identify(0x054c,0x05c4)==SPIDER_PAD_DS4);
          assert(spider_pad_identify(0x054c,0x09cc)==SPIDER_PAD_DS4);
          assert(spider_pad_identify(0x054c,0x0ba0)==SPIDER_PAD_DS4);
          assert(spider_pad_identify(0x045e,0x028e)==SPIDER_PAD_XBOX360);
          assert(spider_pad_identify(0x045e,0x0719)==SPIDER_PAD_NONE);
          assert(spider_pad_identify(0x045e,0x0001)==SPIDER_PAD_NONE);
          assert(spider_pad_identify(0x054c,0x028e)==SPIDER_PAD_NONE);
          assert(spider_pad_identify(0xffff,0xffff)==SPIDER_PAD_NONE);
          assert(spider_pad_identify(0x0,0x0)==SPIDER_PAD_NONE);
          assert(spider_reconnect_sony_ds4(0x054c,0x09cc));
          assert(!spider_reconnect_sony_ds4(0x045e,0x028e));
          assert(spider_reconnect_supported_pad(0x045e,0x028e));
          assert(spider_reconnect_supported_pad(0x054c,0x09cc));
          assert(!spider_reconnect_supported_pad(0x046d,0xc216));
          assert(spider_xbox_2d_edges(1,0)==1);
          assert(spider_xbox_2d_edges(2,0)==2);
          assert(spider_xbox_2d_edges(3,3)==0);
          assert(spider_xbox_2d_edges(1,2)==1);
          assert(spider_xbox_2d_edges(0,1)==0);
          return 0;
        }
    """))
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(code),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_xbox_mode_is_explicit_no_auto_regression():
    assert '"Xbox360"' in INI
    assert 'else if(!_stricmp(choice,"Xbox360"))profile=SPIDER_PROFILE_XBOX360;' in UI
    assert "(profile==SPIDER_PROFILE_AUTO || profile==SPIDER_PROFILE_DUALSHOCK4)" in UI
    assert "profile==SPIDER_PROFILE_XBOX360);" in UI
    assert "selected_pad_kind=choose_ds4?SPIDER_PAD_DS4:SPIDER_PAD_XBOX360;" in UI
    assert "selected_pad_kind=SPIDER_PAD_NONE;" in UI
    assert "ControllerProfile=Auto" in DEFAULTS
    assert "Xbox 360 needs opt-in" in DEFAULTS
    assert "experimental wired 045E:028E" in DEFAULTS
    assert "PS1 layout only on DS4" in DEFAULTS


def test_2d_xbox_actions_do_not_inherit_ps1_gameplay_mapping():
    assert "if(selected_pad_kind==SPIDER_PAD_XBOX360)" in UI
    assert "state.rgbButtons[SPIDER_XBOX_DI_A]" in UI
    assert "state.rgbButtons[SPIDER_XBOX_DI_B]" in UI
    assert "spider_xbox_2d_edges(held,(unsigned)previous_buttons)" in UI
    assert "int ui_allowed=(menu_2d && !gameplay_3d) || bink_active ||" in UI
    assert "(native_pause_active && !bink_active);" in UI
    assert 'begin_scancode(0x01u,"Escape")' in UI
    assert 'begin_scancode(0x1Cu,"Return")' in UI
    assert "selected_pad_kind==SPIDER_PAD_DS4 && ps1_preview_requested" in UI
    assert "selected_pad_kind!=SPIDER_PAD_DS4 || !ps1_preview_requested" in UI
    assert "selected_pad_kind!=SPIDER_PAD_DS4" in UI
    assert "IDirectInput8_CreateDevice(di," in UI
    assert "choose_ds4?&d.ds4_instance:&d.xbox_instance,&device,NULL" in UI
    xbox=UI.split("if(selected_pad_kind==SPIDER_PAD_XBOX360) {",1)[1].split(
        "pressed=(state.rgbButtons[1]",1)[0]
    assert "return;" in xbox
    assert "SpiderCfg_EnablePS1Preview" not in xbox
    assert "IDirectInputDevice8_SetProperty" not in xbox


def test_native_rebind_reuses_the_game_factory_and_requires_explicit_xbox_mode():
    assert "allowed_pad_kind=SPIDER_PAD_DS4;" in RECONNECT
    assert 'if(!_stricmp(choice,"Xbox360"))allowed_pad_kind=SPIDER_PAD_XBOX360;' in RECONNECT
    assert "spider_pad_identify(vid,pid)!=allowed_pad_kind" in RECONNECT
    assert "list.count!=1u || list.gamepad_count!=1u" in RECONNECT
    assert "IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL" in RECONNECT
    assert "IDirectInput8_CreateDevice(di,&list.instance,&candidate,NULL)" in RECONNECT
    assert "set_native_range(device,0u)" in RECONNECT
    assert "set_native_range(device,4u)" in RECONNECT
    assert "SpiderNativeUnplug_Installed()" in RECONNECT
    for banned in ("DirectInput8Create(", "VirtualProtect(", "ExitProcess(",
                   "IDirectInput8_Release(", "SetWindowsHookEx("):
        assert banned not in RECONNECT


def test_controller_support_does_not_modify_dinput_kernel_driver_or_exe():
    assert "045E:028E" in IDENTITY
    assert "Holtek" in IDENTITY
    assert "0x0719" not in IDENTITY.split("static inline unsigned spider_pad_identify",1)[1]
    assert "0x028eu" in IDENTITY
    assert "return SPIDER_PAD_NONE;" in IDENTITY
    assert "XInput" in IDENTITY
    assert "native_joystick_unplug.c" not in UI
