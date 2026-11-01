"""Universal SDL3 foundation: hardware-neutral semantics and raw-pad visibility.

No physical gamepad or Spider-Man executable is required for these tests.
A live 3D bridge and a raw-axis mapping wizard remain separate future stages.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_unmapped_joysticks_are_reported_without_unsafe_native_claim():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    assert '"SDL_GetJoysticks"' in probe
    assert '"SDL_AddGamepadMappingsFromFile"' in probe
    assert 'SDL3-gamecontrollerdb.txt' in probe
    assert "SDL3-MAPPINGS: external model database" in probe
    assert '"SDL_IsGamepad"' in probe
    assert '"SDL_GetJoystickNameForID"' in probe
    assert "SDL3-RAW: unmapped joystick" in probe
    assert "raw mapping wizard required" in probe
    assert "raw_last_ids" in probe
    assert '"SDL_OpenJoystick"' in probe
    assert '"SDL_AddGamepadMapping"' in probe
    assert "SDL3-mapping-%s.txt" in probe


def test_hardware_neutral_action_policy(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"universal.c"
    binary=tmp_path/"universal"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_universal_actions.h"
      int main(void) {
        SpiderSDL3ControllerProfile a,b;
        SpiderSDL3ControllerSample sample={0};
        SpiderSDL3UniversalActions state={0};
        SpiderSDL3UniversalFrame frame;
        /* Completely unknown USB IDs get standard SDL semantic defaults. */
        assert(spider_sdl3_profile_default(&a,0x1234u,0x9876u));
        assert(spider_sdl3_profile_default(&b,0x4567u,0x1122u));
        frame=spider_sdl3_universal_step(&state,&a,&sample,51,1,1);
        assert(frame.valid && !frame.pressed && state.armed);
        sample.buttons=1u<<0u; /* SOUTH -> Jump on any model. */
        frame=spider_sdl3_universal_step(&state,&a,&sample,51,1,1);
        assert(frame.pressed==1u && frame.held==1u);
        frame=spider_sdl3_universal_step(&state,&a,&sample,51,1,1);
        assert(frame.held==1u && !frame.pressed);
        sample.buttons=0;
        frame=spider_sdl3_universal_step(&state,&a,&sample,51,1,1);
        assert(frame.released==1u && frame.held==0u);
        /* A profile binding swap changes semantics without VID filtering. */
        assert(spider_sdl3_profile_assign(&b,SPIDER_SDL3_ACTION_JUMP,
            spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,2)));
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.valid && state.armed && !frame.held);
        sample.buttons=1u<<2u;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.pressed==1u);
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,0,1);
        assert(!frame.valid && !frame.pressed && !state.armed);
        /* Returning with a held button never produces a ghost Jump. */
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.valid && !frame.pressed && !state.armed);
        sample.buttons=0;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(state.armed && !frame.pressed);
        /* Trigger hysteresis: RT Swing ON >= 11000; OFF below 5000. */
        sample.axis[SPIDER_SDL3_AXIS_RT]=15000;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.pressed==(1u<<SPIDER_SDL3_ACTION_SWING));
        sample.axis[SPIDER_SDL3_AXIS_RT]=7000;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.held&(1u<<SPIDER_SDL3_ACTION_SWING));
        sample.axis[SPIDER_SDL3_AXIS_RT]=4000;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.released==(1u<<SPIDER_SDL3_ACTION_SWING));
        return 0;
      }
    """), encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_raw_sdl_gamepad_mapping_builder(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"rawmap.c"
    binary=tmp_path/"rawmap"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_raw_mapping.h"
      int main(void) {
        const char *guid="341a3608000000000000504944564944";
        SpiderSDL3RawMapping map={0};
        char buf[512],small[12];
        map.input[SPIDER_RAW_MAP_A].kind=SPIDER_RAW_BUTTON;
        map.input[SPIDER_RAW_MAP_A].index=1;
        map.input[SPIDER_RAW_MAP_B].kind=SPIDER_RAW_BUTTON;
        map.input[SPIDER_RAW_MAP_B].index=2;
        map.input[SPIDER_RAW_MAP_LEFTX].kind=SPIDER_RAW_AXIS;
        map.input[SPIDER_RAW_MAP_LEFTX].index=0;
        map.input[SPIDER_RAW_MAP_LEFTY].kind=SPIDER_RAW_AXIS;
        map.input[SPIDER_RAW_MAP_LEFTY].index=1;
        map.input[SPIDER_RAW_MAP_DPUP].kind=SPIDER_RAW_HAT;
        map.input[SPIDER_RAW_MAP_DPUP].index=0;
        map.input[SPIDER_RAW_MAP_DPUP].hat_value=1;
        assert(spider_sdl3_raw_build(guid,"Generic Gamepad",&map,buf,sizeof(buf)));
        assert(!strcmp(buf,
          "341a3608000000000000504944564944,Generic Gamepad,"
          "a:b1,b:b2,dpup:h0.1,leftx:a0,lefty:a1,"));
        assert(!spider_sdl3_raw_build(guid,"bad,name",&map,buf,sizeof(buf)));
        assert(!buf[0]);
        assert(!spider_sdl3_raw_build("not-a-guid","Pad",&map,buf,sizeof(buf)));
        assert(!buf[0]);
        assert(!spider_sdl3_raw_build(guid,"Pad",&map,small,sizeof(small)));
        assert(!small[0]);
        map.input[SPIDER_RAW_MAP_DPUP].hat_value=3;
        assert(!spider_sdl3_raw_build(guid,"Pad",&map,buf,sizeof(buf)));
        assert(!buf[0]);
        map.input[SPIDER_RAW_MAP_DPUP].hat_value=1;
        map.input[SPIDER_RAW_MAP_A].kind=SPIDER_RAW_NONE;
        map.input[SPIDER_RAW_MAP_A].index=0;
        assert(!spider_sdl3_raw_build(guid,"Pad",&map,buf,sizeof(buf)));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_keyboard_calibration_wizard_is_built_and_keeps_gameplay_unchanged():
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    setup=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    wizard=(ROOT/"sdl3_raw_wizard.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    assert "sdl3_raw_wizard.c" in build
    assert "SPIDER_UI_RAW=1008, SPIDER_UI_TEST=1009" in setup
    assert "SpiderSDL3RawWizard_Open(ui->hwnd,setup_report)" in setup
    assert "setup_toggle_sdl3_test(ui)" in setup
    assert "Test SDL3 inputs" in setup
    assert "Calibrate SDL3 joystick" in setup
    assert "F9/F10 untouched" in setup
    assert "if(vk==VK_F9)" not in setup
    assert "if(vk==VK_F10)" not in setup
    assert "if(vk==VK_F8)" not in setup
    assert "SDL3 TEST: buttons=" in setup
    assert "hardware recognition / recalibration" in wizard.lower()
    assert "SpiderSDL3Probe_RawListAll" in wizard
    assert "SpiderSDL3Probe_RawListAll" in probe
    assert "SpiderSDL3Probe_RawName" in wizard
    assert "spider_raw_ui_thread()" in probe
    assert "ALREADY MAPPED, recalibrating" in wizard
    assert "Mapped - recalibrate" in wizard
    assert "mapped devices CAN be recalibrated" in wizard
    assert "current-sdl-mapping=" in wizard
    assert "SDL3-RAW-WIZARD: step=" in wizard
    assert "n=SpiderSDL3Probe_RawListAll(ids,NULL" in probe
    assert "wizard_neutral" in wizard
    assert "wizard_finish(w)" in wizard
    assert "SDL3 CALIBRATION SAVED" in wizard
    assert "if(msg.wParam==VK_ESCAPE)" in wizard
    assert "if(msg.wParam=='S'" in wizard
    assert "SDL3-RAW-WIZARD: completed and persisted" in wizard
    assert "SpiderSDL3Probe_RawList" in probe
    assert "SpiderSDL3Probe_RawRead" in probe
    assert "SpiderSDL3Probe_RawRegisterAndSave" in probe
    assert "SDL3-mapping-%s.txt" in probe
    assert "spider_raw_local_path" in probe
    assert "LOCALAPPDATA" in probe
    assert "MoveFileExA(temp,file,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)" in probe
    assert "SDL_AddGamepadMapping refused mapping; nothing saved" in probe
    assert "SDL3-MAPPINGS: persisted per-GUID user mappings loaded=" in probe
    assert "sdl3-gamepad" not in wizard

def test_mapped_gamepad_can_enter_recalibration_without_changing_game_owner():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    wizard=(ROOT/"sdl3_raw_wizard.c").read_text(encoding="utf-8")
    setup=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    assert "SpiderSDL3Probe_RawListAll(ids,NULL" in probe
    assert "SpiderSDL3Probe_RawListAll(w->ids,w->already_mapped,16u)" in wizard
    assert "current-sdl-mapping=%s" in wizard
    assert "Mapped - recalibrate" in wizard
    assert "Unmapped - recognize" in wizard
    assert "SPIDER_UI_RAW" in setup
    assert "universal 3D game bridge NOT active" in setup
    assert "Press F9 to exit" not in setup
    assert "SDL3-3D-LIVE" not in wizard
