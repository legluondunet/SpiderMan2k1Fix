"""3D SDL3 selected semantic input preparation, no native game writes.

The original game's DirectInput data representation is NOT the SDL3 axis
representation. A future live bridge must have exactly one game-input owner.
"""
from pathlib import Path
import subprocess
import shutil
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_portable_semantic_3d_gate_and_transitions(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"gameplay_plan.c"
    out=tmp_path/"gameplay_plan"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_gameplay_plan.h"
      int main(void) {
        SpiderSDL3GameplayPlan state={0};
        SpiderSDL3ControllerProfile sony,xbox;
        SpiderSDL3ControllerSample sample={0};
        uint32_t press=999,release=999;
        assert(spider_sdl3_profile_default(&sony,0x054c,0x09cc));
        assert(spider_sdl3_profile_default(&xbox,0x045e,0x028e));
        /* Reject background, menu and unmatched native owner. */
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,0,1,&press,&release)==SPIDER_PLAN_REJECTED);
        assert(!press && !release && !state.armed);
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,0,&press,&release)==SPIDER_PLAN_REJECTED);
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_REJECTED);
        /* Opening in 3D with a held A never inherits that held press. */
        sample.buttons=1u;
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        assert(!state.armed && !press);
        sample.buttons=0;
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        assert(state.armed && !press);
        sample.buttons=1u;
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(press==(1u<<SPIDER_SDL3_ACTION_JUMP) && !release);
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(!press && !release);
        /* Trigger hysteresis; do not confuse RT with LT. */
        sample.buttons=0;
        sample.axis[SPIDER_SDL3_AXIS_LT]=32767;
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(!(press&(1u<<SPIDER_SDL3_ACTION_SWING)));
        sample.axis[SPIDER_SDL3_AXIS_LT]=0;
        sample.axis[SPIDER_SDL3_AXIS_RT]=32767;
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(press==(1u<<SPIDER_SDL3_ACTION_SWING));
        sample.axis[SPIDER_SDL3_AXIS_RT]=6000;
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(!press && !release);
        sample.axis[SPIDER_SDL3_AXIS_RT]=4999;
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          1,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(release==(1u<<SPIDER_SDL3_ACTION_SWING));
        /* Selection handoff cannot inject a different controller held
         * action before its own neutral sample, even with same model. */
        sample.buttons=1u;
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          2,0x054c,0x09cc,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        assert(!press && !release && !state.armed);
        /* Native DI owner mismatch => disarm, no release injected. */
        assert(spider_sdl3_gameplay_plan_step(&state,&sony,&sample,
          2,0x054c,0x09cc,1,0,&press,&release)==SPIDER_PLAN_REJECTED);
        sample.buttons=0;
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        assert(state.instance==3);
        /* Saved remap (Swing->LT) is evaluated purely as SDL3 semantics. */
        assert(spider_sdl3_profile_assign(&xbox,SPIDER_SDL3_ACTION_SWING,
          spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_LT)));
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_READY);
        sample.axis[SPIDER_SDL3_AXIS_RT]=32767;
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(!press);
        sample.axis[SPIDER_SDL3_AXIS_RT]=0;
        sample.axis[SPIDER_SDL3_AXIS_LT]=32767;
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(press==1u<<SPIDER_SDL3_ACTION_SWING);
        /* Alt+Tab, pause or 2D transition resets held state without
         * manufacturing any keyboard/game events. */
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,0,1,&press,&release)==SPIDER_PLAN_REJECTED);
        assert(!state.armed && !press && !release);
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        assert(!press && !release);
        sample.axis[SPIDER_SDL3_AXIS_LT]=0;
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        assert(state.armed);
        /* A profile remap on this same pad must not inherit a held key. */
        sample.buttons=1u; /* south: Jump before remapping */
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(press==(1u<<SPIDER_SDL3_ACTION_JUMP));
        /* Existing Kick on east swaps to south. */
        assert(spider_sdl3_profile_assign(&xbox,SPIDER_SDL3_ACTION_JUMP,
          spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,1)));
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        assert(!press && !release && !state.armed);
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        sample.buttons=0;
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_WAIT_NEUTRAL);
        assert(state.armed && !press);
        sample.buttons=1u; /* south: now Kick */
        assert(spider_sdl3_gameplay_plan_step(&state,&xbox,&sample,
          3,0x045e,0x028e,1,1,&press,&release)==SPIDER_PLAN_READY);
        assert(press==(1u<<SPIDER_SDL3_ACTION_KICK) && !release);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(out)],check=True)
    subprocess.run([str(out)],check=True)


def test_stage_is_explicitly_readonly_and_ordered_after_native_handoff():
    probe=(ROOT/"sdl3_gameplay_plan_probe.c").read_text(encoding="utf-8")
    h=(ROOT/"sdl3_gameplay_plan.h").read_text(encoding="utf-8")
    game=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    sdl=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    ini=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    assert '"SDL3GameplayPlan"' in ini
    assert 'GetPrivateProfileIntA("Debug","SDL3GameplayPlan",0,ini)==1' in probe
    assert '"ExperimentalSDL3NativeOwner",0,ini)==1' in probe
    assert "sdl3_gameplay_plan_probe.c" in build
    assert "SpiderSDL3Probe_SelectedSample(" in sdl
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in probe
    assert "SpiderSDL3Probe_SetupProfile(vid,pid)" in probe
    assert "spider_sdl3_gameplay_plan_step(" in probe
    assert "GetForegroundWindow()!=hwnd" in probe
    assert "SPIDER_PLAN_WAIT_NEUTRAL" in probe
    assert "[NO NATIVE INJECTION]" in probe
    assert "SDL3-3D-PLAN: REFUSED" in probe
    assert "spider_sdl3_gameplay_plan_reset(s)" in h
    before=game.index("SpiderNativeReconnect_OnFlip(count,game_hwnd);")
    after=game.index("SpiderSDL3GameplayPlan_OnFlip(game_hwnd,")
    assert after>before
    assert "SpiderSDL3GameplayPlan_Init(ini,log_line);" in game
    assert "SpiderSDL3GameplayPlan_Init(NULL,log_line);" in game
    for forbidden in ("VirtualProtect(", "SendInput(", "WriteProcessMemory(",
                      "IDirectInputDevice8_", "memcpy((void *)(uintptr_t)",
                      "spider_ps1_write_word(", "CreateThread("):
        assert forbidden not in probe and forbidden not in h
