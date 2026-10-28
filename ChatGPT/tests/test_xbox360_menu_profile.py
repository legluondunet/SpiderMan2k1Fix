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
       assert(spider_xbox360_menu_edges(held,0)==SPIDER_XBOX360_ENTER); /* Start confirms menu */
       physical[7]=0;physical[6]=0x80;
       held=spider_xbox360_held_buttons(physical);
       assert(spider_xbox360_menu_edges(held,0)==SPIDER_XBOX360_BACK); /* Back exits menu */
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


def test_native_joystick_menu_is_readonly_and_xbox_pause_fail_closed():
    native_cfg=(ROOT/"virtual_spidey_cfg.c").read_text()
    ui=UI
    source_block=ui.split("static int xbox_pause_button_free_of_native_actions(",1)[1].split("static BOOL CALLBACK enumerate_gamepad(",1)[0]
    assert "GetPrivateProfileStringA(\"Joystick\",native_actions[i],fallback," in source_block
    assert "if(!parse_native_button(value,&button) || button==requested_button)" in source_block
    assert "GAMEPAD-XBOX360: PAUSE BLOCKED physical BUTTON%u" in source_block
    assert "xbox_pause_button_free_of_native_actions(pause_button_index)" in ui
    assert "(gameplay_3d || native_pause_active) && !bink_active" in ui
    assert "pause_requested=(fresh & ((1u<<6)|(1u<<7)))!=0" in ui
    assert 'GAMEPAD-XBOX360: 3D MENU Back/Start -> Escape' in ui
    assert '"[Joystick]"' not in source_block  # no writes from guard
    # The optional new Xbox RAM overlay must be explicitly requested;
    # the default old Xbox menu bridge must not alter gameplay mapping.
    assert "SpiderCfg_EnableXbox360Preview" in native_cfg
    assert "xbox360_preview_active" in native_cfg
    assert 'GetPrivateProfileIntA("Input","ControllerXbox360Preview",0,ini)==1' in ui
    assert 'ControllerXbox360Preview=0' in native_cfg
    assert "spider_xbox360_apply_gameplay" not in native_cfg
    assert "load_joystick();" in native_cfg
    assert "if(!save_joystick()) goto fail;" in native_cfg


def test_native_button_parser_accepts_game_unbound_none_without_accepting_unknown_indices(tmp_path):
    """Compile and execute the actual parser from gamepad_ui_profile.c.

    The native original game uses 0x4000 for NONE; a prior Xbox pause
    run incorrectly classified WEB=0x00004000 as invalid. The guard must
    keep rejecting unrecognized numbers and still block a physical
    Back/Start button if another saved gameplay action uses it.
    """
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc: return
    marker="static int parse_native_button(const char *value,unsigned *button) {"
    assert UI.count(marker)==1
    parser=marker+UI.split(marker,1)[1].split("/* Before mapping OPTIONS",1)[0]
    src=tmp_path/"native_button_parser.c"
    exe=tmp_path/"native_button_parser"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <stdio.h>
      #include <stdlib.h>
      #include <strings.h>
      #define _strnicmp strncasecmp
    """)+parser+textwrap.dedent(r"""
      int main(void) {
        unsigned n=999;
        assert(parse_native_button("0x00004000",&n) && n==0x4000u);
        assert(parse_native_button("16384",&n) && n==0x4000u);
        assert(parse_native_button("BUTTON7",&n) && n==7u);
        assert(parse_native_button("BUTTON6",&n) && n==6u);
        assert(parse_native_button("BUTTON9",&n) && n==9u);
        assert(!parse_native_button("BUTTON128",&n));
        assert(!parse_native_button("0x4001",&n));
        assert(!parse_native_button("NONE",&n));
        assert(!parse_native_button("",&n));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_xbox_readonly_screen_does_not_advertise_editing_as_pause_workaround():
    xbox_guard=UI.split("static int xbox_pause_button_free_of_native_actions(",1)[1].split(
        "static BOOL CALLBACK enumerate_gamepad(",1)[0]
    assert "Joystick is read-only" in xbox_guard
    assert "use the game's editable Joystick menu" not in xbox_guard
    assert "requested_button,native_actions[i],value);" in xbox_guard
