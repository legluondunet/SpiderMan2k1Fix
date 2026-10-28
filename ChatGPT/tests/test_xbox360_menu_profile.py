"""Xbox 360-compatible controller: A/B in 2D, strict identity, Sony PS1 isolation.

The user-supplied 2026-10-28 Wine log from the Holtek USB 045E:028E
already proves native axis/POV menu movement and native device rebind.
It does NOT prove any particular button index; A=0/B=1 are a
logging-instrumented candidate and must be confirmed on hardware.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
UI=(ROOT/"gamepad_ui_profile.c").read_text()
XBOX=(ROOT/"xbox360_menu_policy.h").read_text()
IDENTITY=(ROOT/"native_joystick_reconnect_policy.h").read_text()
VALIDATE=(ROOT/"ini_validation.h").read_text()
TEMPLATE=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_xbox360_policy_compiled_and_edge_safe(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc: return
    src=tmp_path/"test_pad.c"
    exe=tmp_path/"test_pad"
    src.write_text(textwrap.dedent(r"""
    #include <stdint.h>
    #include <assert.h>
    #include "native_joystick_reconnect_policy.h"
    #include "xbox360_menu_policy.h"
    int main(void) {
       unsigned char physical[32]={0};
       uint32_t held=0,old=0;
       assert(spider_reconnect_xbox360(0x045e,0x028e));
       assert(!spider_reconnect_xbox360(0x054c,0x09cc));
       assert(!spider_reconnect_xbox360(0x045e,0x0719)); /* receiver not pad */
       assert(spider_reconnect_sony_ds4(0x054c,0x09cc));
       assert(spider_reconnect_kind(0x045e,0x028e)==SPIDER_RECONNECT_XBOX360);
       assert(spider_reconnect_kind(0x054c,0x09cc)==SPIDER_RECONNECT_DS4);
       assert(SPIDER_XBOX360_BUTTONS_TO_TRACE==10u);
       assert(!spider_xbox360_held_buttons(0));
       assert(spider_xbox360_held_buttons(physical)==0);
       physical[0]=0x80; /* candidate A down */
       held=spider_xbox360_held_buttons(physical);
       assert(held==1);
       assert(spider_xbox360_menu_edges(held,old)==SPIDER_XBOX360_ENTER);
       old=held;
       assert(spider_xbox360_menu_edges(held,old)==0); /* held isn't retriggered */
       physical[0]=0; held=spider_xbox360_held_buttons(physical);
       assert(!spider_xbox360_menu_edges(held,old)); /* release isn't keydown */
       old=held;
       physical[1]=0x80;held=spider_xbox360_held_buttons(physical);
       assert(spider_xbox360_menu_edges(held,old)==SPIDER_XBOX360_BACK);
       old=held;
       physical[0]=0x80;held=spider_xbox360_held_buttons(physical);
       assert(spider_xbox360_menu_edges(held,old)==SPIDER_XBOX360_ENTER);
       old=0;
       assert(spider_xbox360_menu_edges(held,old)==SPIDER_XBOX360_BACK); /* B wins */
       physical[0]=0;physical[1]=0;physical[7]=0x80;
       held=spider_xbox360_held_buttons(physical);
       assert(held==(1u<<7));
       assert(spider_xbox360_menu_edges(held,0)==0); /* no synthetic START */
       return 0;
    }"""),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_auto_detects_verified_xbox_without_sony_ps1_preview():
    for expected in (
       '#include "xbox360_menu_policy.h"',
       '#include "native_joystick_reconnect_policy.h"',
       'else if(!_stricmp(choice,"Xbox360"))profile=SPIDER_PROFILE_XBOX360;',
       'else if(spider_reconnect_xbox360(vid,pid))',
       'choose_xbox=d.found==1u && d.xbox_count==1u',
       '(profile==SPIDER_PROFILE_AUTO || profile==SPIDER_PROFILE_XBOX360)',
       'hr=IDirectInput8_CreateDevice(di,choose_ds4?&d.ds4_instance:&d.xbox_instance,&device,NULL);',
       'detected_kind=choose_ds4?SPIDER_RECONNECT_DS4:SPIDER_RECONNECT_XBOX360;',
       'if(detected && detected_kind==SPIDER_RECONNECT_XBOX360)',
       'if(!detected || detected_kind!=SPIDER_RECONNECT_DS4 || !ps1_preview_requested)return;',
       'if(detected_kind==SPIDER_RECONNECT_DS4 && ps1_preview_requested && !ps1_preview_active)',
       'if(detected_kind==SPIDER_RECONNECT_XBOX360)',
       'unsigned action=spider_xbox360_menu_edges(held,xbox_previous_mask);',
       'xbox_previous_mask=held;',
       'GAMEPAD-XBOX360: BUTTON-EDGE',
       'GAMEPAD-UI: PROFILE Xbox360 EXPERIMENTAL ACTIVE',
       "int ui_allowed=(menu_2d && !gameplay_3d) || bink_active ||",
       'begin_scancode(0x01u,"Escape");',
       'begin_scancode(0x1Cu,"Return");',
    ):
        assert expected in UI,expected
    assert "MouseEnabled=0" in TEMPLATE
    assert '"Xbox360"' in VALIDATE
    assert '"; Xbox360 (045E:028E etc.) A/B menu bridge is experimental.' in TEMPLATE
    assert "d.found==1u" in UI
    assert "if(!choose_ds4 && !choose_xbox)" in UI


def test_stays_on_original_input_and_uses_one_secondary_device():
    assert "IDirectInput8_CreateDevice(di,choose_ds4?&d.ds4_instance:&d.xbox_instance,&device,NULL);" in UI
    assert "IDirectInputDevice8_SetDataFormat(device,&c_dfDIJoystick2)" in UI
    assert "DISCL_FOREGROUND|DISCL_NONEXCLUSIVE" in UI
    assert "SpiderPS1Mouse_OnFlip(hwnd);" in UI
    assert "spider_ds4_hotplug_accept_buttons(&hotplug,held)" in UI
    assert "xbox_button_trace_count<32u" in UI
    xbox_branch=UI.split("if(detected_kind==SPIDER_RECONNECT_XBOX360) {",1)[-1].split("pressed=(state.rgbButtons[1]",1)[0]
    assert "SpiderCfg_EnablePS1Preview" not in xbox_branch
    assert "VirtualProtect" not in xbox_branch
    assert "IDirectInput8_CreateDevice" not in xbox_branch
    assert "IDirectInputDevice8_Release" not in xbox_branch
    assert "GetDeviceState" not in xbox_branch
    assert "return;" in xbox_branch  # no fallthrough to DS4 key mapping
