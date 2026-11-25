"""Regression suite for the fixed, read-only PS1-inspired SDL3 gamepad UI.

The old per-model action editor/raw calibration modal has been retired.
SDL3's hardware GUID mappings are distinct and must continue to load.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

def load(path):
    return (ROOT / path).read_text(encoding="utf-8")

@pytest.mark.skipif(not (shutil.which("cc") or shutil.which("gcc")),
                    reason="C compiler unavailable")
def test_immutable_factory_actions_match_ps1_and_xbox_equivalents(tmp_path):
    source=tmp_path/"fixed.c"
    exe=tmp_path/"fixed"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_controller_profile.h"
      static void check(uint16_t v,uint16_t p) {
        SpiderSDL3ControllerProfile x={0};
        assert(spider_sdl3_profile_default(&x,v,p));
        assert(x.actions[SPIDER_SDL3_ACTION_JUMP].kind==SPIDER_SDL3_BIND_BUTTON);
        assert(x.actions[SPIDER_SDL3_ACTION_JUMP].index==0);   /* Cross/A */
        assert(x.actions[SPIDER_SDL3_ACTION_KICK].index==1);   /* Circle/B */
        assert(x.actions[SPIDER_SDL3_ACTION_PUNCH].index==2);  /* Square/X */
        assert(x.actions[SPIDER_SDL3_ACTION_WEB].index==3);    /* Triangle/Y */
        assert(x.actions[SPIDER_SDL3_ACTION_AIM].index==9);    /* L1/LB */
        assert(x.actions[SPIDER_SDL3_ACTION_ZIPLINE].index==10);/* R1/RB */
        assert(x.actions[SPIDER_SDL3_ACTION_SWING].kind==SPIDER_SDL3_BIND_TRIGGER);
        assert(x.actions[SPIDER_SDL3_ACTION_SWING].index==SPIDER_SDL3_AXIS_RT);
        assert(x.actions[SPIDER_SDL3_ACTION_PAUSE].index==4); /* Share/Back */
        assert(x.context[SPIDER_SDL3_CONTEXT_CONFIRM].index==0);
        assert(x.context[SPIDER_SDL3_CONTEXT_BACK].index==1);
        assert(x.context[SPIDER_SDL3_CONTEXT_SKIP_VIDEO].index==0);
        assert(x.context[SPIDER_SDL3_CONTEXT_NEXT_STILL].index==0);
        assert(x.context[SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE].index==0);
      }
      int main(void) {
        check(0x054c,0x05c4); /* DualShock 4 */
        check(0x045e,0x028e); /* Xbox 360 */
        check(0x054c,0x0268); /* PS3 if SDL3 maps it */
        check(0x1234,0x5678); /* manufacturer neutral */
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([shutil.which("cc") or shutil.which("gcc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(source),"-o",str(exe)],capture_output=True,text=True,check=True)
    subprocess.run([str(exe)],check=True)

def test_runtime_no_longer_reads_or_writes_per_model_action_profiles():
    probe=load("sdl3_gamepad_probe.c")
    build=load("build.sh")
    ui=load("sdl3_controller_setup.c")
    getter=probe.split("static SpiderSDL3ControllerProfile *spider_sdl3_get_profile(",1)[1]
    getter=getter.split("void SpiderSDL3Probe_Init(",1)[0]
    assert "return spider_sdl3_bank_get(&bindings_bank,vid,pid);" in getter
    assert "SpiderSDL3ProfileStore_" not in probe
    assert "sdl3_controller_profile_store.c" not in build
    assert "sdl3_raw_wizard.c" not in build
    assert "SpiderSDL3Probe_SetupCommit" not in probe
    assert "SpiderSDL3Probe_SetupCommit" not in load("sdl3_gamepad_probe.h")
    for forbidden in ("CreateFileA(", "WriteFile(", "WritePrivateProfileString",
                      "SpiderSDL3ProfileStore_Save", "spider_sdl3_capture_feed",
                      "SDL3-RAW-WIZARD", "Restore defaults",
                      "Capture selected action"):
        assert forbidden not in ui

def test_fixed_controller_guide_is_native_and_draws_all_actions():
    ui=load("sdl3_controller_setup.c")
    game=load("spiderman2k1fix.c")
    assert "static BOOL draw_native_guide(HDC dc,const RECT *content)" in ui
    assert "static void draw_gamepad(HDC dc)" in ui
    assert "shell_path(dc,0,dark,metal)" in ui
    assert "PolyBezierTo(dc,p," in ui
    assert "face_button(dc,390,202,3)" in ui
    assert "face_button(dc,413,225,1)" in ui
    assert "face_button(dc,390,248,0)" in ui
    assert "face_button(dc,367,225,2)" in ui
    assert "draw_leaders(dc);" in ui
    route=load("ps1_joystick_readonly_menu.c")
    probe=load("sdl3_gamepad_probe.c")
    assert "SDL_GetGamepadButtonLabel" in probe
    read=probe.split("int SpiderSDL3Probe_SelectedGuideLabels(",1)[1].split("int SpiderSDL3Probe_SelectedMappedSample(",1)[0]
    assert "pads[i].id!=generic_selected_id" in read
    assert "sdl_connected(pads[i].handle)" in read
    assert "sdl_button_label(pads[i].handle,(int)j)" in read
    assert "SpiderSDL3ControllerSetup_RefreshGuide();" in route
    for pos,action in ((2,"PUNCH"),(3,"WEB"),(1,"KICK"),(0,"JUMP")):
        assert '"%s '+action+'",SpiderSDL3ControllerSetup_GuideButton('+str(pos)+')' in route
    assert "static const unsigned ps[4]={5,6,7,8},xb[4]={1,2,3,4}" in ui
    assert "labels[0]==2 && labels[1]==1 && labels[2]==4 && labels[3]==3" in ui
    assert 'positions[]={"SOUTH","EAST","WEST","NORTH"}' in ui
    assert 'labels[0].s="L1 AIM"' in route
    assert 'labels[0].s="LB AIM"' in route
    assert 'labels[0].s="L AIM"' in route
    assert '{390,97,punch}' in route
    assert "stick(dc,238,215);dpad(dc,279,247,22);" in ui
    assert "dpad(dc,238,215,29);stick(dc,282,260);" in ui
    import re
    targets={"aim":(225,162),"swing":(408,136),"zip":(418,162),
             "move":(210,215),"move_stick":(264,260),"pause":(286,178),
             "xbox_stick":(220,215),"xbox_dpad":(258,247),"xbox_pause":(290,179),
             "web":(401,202),"kick":(425,225),"jump":(400,257),"punch":(367,213)}
    assert "disc(dc,390,225,36," in ui
    assert "pts[n-1].y,2," in ui
    assert "PolyBezierTo(dc,q,3)" in ui
    for name,target in targets.items():
        line=re.search(r"POINT "+name+r"\[\]=\{(.*?)\};",ui).group(1)
        coords=[tuple(map(int,p)) for p in re.findall(r"\{(\d+),(\d+)\}",line)]
        assert coords[-1]==target
        assert all(0<=x<=640 and 80<=y<=440 for x,y in coords)
        assert "LEAD("+name+");" in ui
    assert "0x00458700u" in route  # original Mess_DrawText
    assert "CreateFontA" not in ui
    assert "draw_gamepad(dc);" in ui
    assert "RoundRect(dc,54,104" not in ui
    assert "SetWindowExtEx(dc,640,480,NULL);" in ui
    assert "SetViewportOrgEx(dc,content->left,content->top,NULL);" in ui
    assert "IDirectDrawSurface7_GetDC(surface,&dc)" in ui
    assert "IDirectDrawSurface7_ReleaseDC(surface,dc)" in ui
    assert "SpiderSDL3ControllerSetup_DrawToSurface(borderless_back,&content)" in game
    assert game.index("SpiderSDL3ControllerSetup_DrawToSurface(borderless_back,&content)") < (
        game.index("f9_capture_completed_menu_flip(count,f9_still_ready)"))
    assert "CreateWindow" not in ui
    assert "SetTimer(" not in ui
    assert "IsDialogMessage" not in ui
    assert "Start  -  Select pad" not in ui

def test_guide_reuses_original_joystick_menu_without_focus_takeover():
    ui=load("sdl3_controller_setup.c")
    route=load("ps1_joystick_readonly_menu.c")
    bridge=load("gamepad_ui_profile.c")
    assert "SpiderSDL3ControllerSetup_OnNativeEnter();" in route
    assert "SpiderSDL3_InstallGenericReadOnlyJoystick(ps1_menu_logger)" in route
    assert "SpiderSDL3_InstallGenericReadOnlyJoystick(logger)" in bridge
    assert "SpiderSDL3_InstallGenericReadOnlyJoystick(logger)" in bridge
    assert "SpiderSDL3_InstallDirectJoystickEntry(ui_log)" in bridge
    assert "SPIDER_NATIVE_CONTROLLER_CONFIG_VA)(native_mode);" in route
    assert "const int is_joystick=(native_mode==0);" in route
    assert "Keyboard selected (mode=1); original game configuration preserved" in route
    assert "SPIDER_NATIVE_JOYSTICK_CONTEXT" in ui
    assert "guide_frame" in ui
    assert "SpiderSDL3ControllerSetup_OnNativeExit();" in route
    assert "SpiderSDL3ControllerSetup_OnNativeFrame();" in route
    assert "if(guide_hide_rows)return 0;" in route
    assert "if(!SpiderSDL3ControllerSetup_IsNativeActive())" in route
    assert "if(SpiderSDL3ControllerSetup_IsNativeActive())return 0;" in route
    for callsite in ("0x0050CFBFu", "0x0050CFCAu", "0x00440486u",
                     "0x0050D338u", "0x0050D3DBu"):
        assert callsite in route
    assert "memcmp(site,guide_sites[i].bytes,5u)!=0" in route
    assert "guide_frame=0;" in ui
    assert "guide_requested=0;" in ui
    assert "SpiderSDL3ControllerSetup_Open();" not in route
    assert "EnableWindow(" not in ui
    assert "SetForegroundWindow(" not in ui
    assert "SpiderSDL3Universal_BeforeModal" not in ui
    assert "SpiderSDL3Probe_SetupModalReturn(" not in ui
    assert "CreateWindowExA(" not in ui
    assert "SendInput(" not in ui
    assert "WritePrivateProfileString(" not in ui

def test_guid_hardware_recognition_is_separate_from_action_profiles():
    probe=load("sdl3_gamepad_probe.c")
    assert "SDL_AddGamepadMappingsFromFile" in probe
    assert "SDL3-gamecontrollerdb.txt" in probe
    assert "SDL3-mapping-*.txt" in probe
    assert "SDL3-MAPPINGS: persisted per-GUID user mappings" in probe
    assert "SpiderSDL3Probe_RawRegisterAndSave" in probe
    assert "spider_sdl3_profile_default(" in load("sdl3_controller_profile.h")
