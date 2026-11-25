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

def test_fixed_controller_guide_draws_real_pad_and_all_actions():
    ui=load("sdl3_controller_setup.c")
    assert "static void guide_paint(" in ui
    assert "RoundRect(dc,244,135,537,301" in ui
    assert "Ellipse(dc,183,146,365,338)" in ui
    for text in ("CROSS  / A", "CIRCLE / B", "SQUARE / X",
                 "TRIANGLE / Y", "L1 / LB", "R1 / RB",
                 "R2 / RT", "Share / Back", "Jump", "Kick",
                 "Punch", "Web", "Aim", "Zipline", "Swing", "Pause"):
        assert text in ui
    assert "SetTimer(" not in ui
    assert "SpiderSDL3Probe_SetupRead" not in ui
    assert "SpiderSDL3Probe_SetupPump()" not in ui
    assert 'guide_text(dc,300,338,"Start  -  Select pad*")' in ui
    assert 'Only needed when several controllers' in ui

def test_guide_remains_focus_safe_and_keyboard_config_unmodified():
    ui=load("sdl3_controller_setup.c")
    route=load("ps1_joystick_readonly_menu.c")
    assert "SpiderSDL3Universal_BeforeModal(owner);" in ui
    assert "spider_sdl3_setup_restore_focus(" in ui
    assert "ui->closing_foreground=GetForegroundWindow();" in ui
    assert "if(parent_enabled && IsWindow(owner))EnableWindow(owner,TRUE);" in ui
    assert "SpiderSDL3Probe_SetupModalReturn(0,0,0);" in ui
    assert "if(has_quit)PostQuitMessage(quit_code);" in ui
    assert "GetForegroundWindow()!=owner" not in ui
    assert "if(wp==VK_ESCAPE || wp==VK_RETURN)" in ui
    assert "const int is_joystick=(native_mode==0);" in route
    assert "if(is_joystick && SpiderSDL3ControllerSetup_Enabled())" in route
    assert "SpiderSDL3ControllerSetup_Open();" in route
    assert "SPIDER_NATIVE_CONTROLLER_CONFIG_VA)(native_mode);" in route
    assert "Keyboard selected (mode=1); original game configuration preserved" in route

def test_guid_hardware_recognition_is_separate_from_action_profiles():
    probe=load("sdl3_gamepad_probe.c")
    assert "SDL_AddGamepadMappingsFromFile" in probe
    assert "SDL3-gamecontrollerdb.txt" in probe
    assert "SDL3-mapping-*.txt" in probe
    assert "SDL3-MAPPINGS: persisted per-GUID user mappings" in probe
    assert "SpiderSDL3Probe_RawRegisterAndSave" in probe
    assert "spider_sdl3_profile_default(" in load("sdl3_controller_profile.h")
