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


def test_generic_ps3_logical_selection_is_explicit_unique_and_native_preserving():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    header=(ROOT/"sdl3_gamepad_probe.h").read_text(encoding="utf-8")
    assert "static uint32_t generic_selected_id=0;" in probe
    assert "static uint16_t generic_selected_vendor=0,generic_selected_product=0;" in probe
    assert "SDL3-GENERIC: SELECTED explicit modal Start" in probe
    assert "spider_sdl3_selected_model_unique(opened,num_pads," in probe
    assert "spider_reconnect_kind(vendor,product)==SPIDER_RECONNECT_NONE" in probe
    assert "selection.active=requested_id;" in probe
    assert "generic_selected_id=requested_id;" in probe
    # The generic branch comes before the legacy native owner branch;
    # generic selection NEVER writes selection.active.
    pending=probe.split("if(setup_owner_pending) {",1)[1].split("sdl3_trace_native_owner(now);",1)[0]
    assert pending.index("generic_selected_id=requested_id;") < pending.index("selection.active=requested_id;")
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" in pending
    assert "generic_selected_id=0;" in pending
    assert "SETUP RETURN REFUSED stale/ambiguous" in pending
    assert "int SpiderSDL3Probe_GenericSelectedSample(" in header
    generic=probe.split("int SpiderSDL3Probe_GenericSelectedSample(",1)[1].split(
        "/* SDL3 exposes each trigger",1)[0]
    assert "sdl3_game_thread!=GetCurrentThreadId()" in generic
    assert "spider_sdl3_selected_model_unique(" in generic
    assert "sdl_connected(pads[i].handle)" in generic
    assert "spider_reconnect_kind(generic_selected_vendor," in generic
    assert "sample->axis[j]=sdl_axis(pads[i].handle,(int)j);" in generic
    assert "SPIDER_SDL3_AXIS_RIGHTX" in (ROOT/"sdl3_controller_profile.h").read_text(encoding="utf-8")
    assert "SPIDER_SDL3_AXIS_RIGHTY" in (ROOT/"sdl3_controller_profile.h").read_text(encoding="utf-8")
    for dangerous in ("SendInput(", "VirtualProtect(", "IDirectInputDevice8_", "WriteProcessMemory("):
        assert dangerous not in generic
    trace=probe.split("static void sdl3_generic_semantic_trace_on_flip(",1)[1].split(
        "void SpiderSDL3Probe_OnFlip(",1)[0]
    assert "spider_sdl3_universal_step(" in trace
    assert "spider_sdl3_profile_valid(profile)" in trace
    assert "spider_sdl3_profile_neutral(" not in trace  # step owns the gate
    assert "SDL3-GENERIC: SEMANTIC ARMED" in trace
    assert "SDL3-GENERIC: SEMANTIC id=" in trace
    assert "[READ-ONLY; original game input unchanged]" in trace
    assert "generic_trace_lines>=48u" in trace
    assert "memcmp(&generic_profile_snapshot,profile,sizeof(*profile))" in trace
    for dangerous in ("SendInput(", "VirtualProtect(", "IDirectInputDevice8_", "WriteProcessMemory("):
        assert dangerous not in trace
    assert "sdl3_generic_semantic_trace_on_flip();" in probe
    # The pre-existing real native owner is still strict: unknown PS3
    # START must not claim DI or replace selection.active.
    assert "SDL3-OWNER: START REFUSED unsupported native controller" in probe



