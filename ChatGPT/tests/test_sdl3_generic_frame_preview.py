"""Phase-1 read-only frame lifecycle: neutral, owner, focus and profile boundaries.

This is a portable independent C99 model, not a working controller bridge.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_single_instance_neutral_focus_and_profile_lifecycle(tmp_path):
    program=tmp_path/"generic_frame.c"
    program.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_generic_frame_preview.h"

      int main(void) {
        SpiderSDL3GenericFramePreview state={0};
        SpiderSDL3ControllerProfile p;
        SpiderSDL3ControllerSample sample={0};
        uint32_t held=99u,fresh=99u;
        unsigned rc;
        assert(spider_sdl3_profile_default(&p,0x054cu,0x0268u));
        #define STEP(id,foreground) spider_sdl3_generic_frame_step(\
          &state,&p,&sample,id,0x054cu,0x0268u,foreground,&held,&fresh)
        rc=STEP(11u,1);
        assert(rc==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        assert(state.armed==1u && held==0 && fresh==0);
        sample.buttons=1u<<0; /* SDL3 South -> native Jump bit */
        assert(STEP(11u,1)==SPIDER_SDL3_FRAME_READY);
        assert(held==0x10 && fresh==0x10);
        assert(STEP(11u,1)==SPIDER_SDL3_FRAME_READY);
        assert(held==0x10 && fresh==0);
        sample.buttons=0u;
        assert(STEP(11u,1)==SPIDER_SDL3_FRAME_READY);
        assert(held==0 && fresh==0);
        sample.buttons=1u<<0;
        assert(STEP(11u,1)==SPIDER_SDL3_FRAME_READY);
        assert(held==0x10 && fresh==0x10);
        /* Losing focus while South is held: discard old edge state. */
        assert(STEP(11u,0)==SPIDER_SDL3_FRAME_REJECTED);
        assert(held==0 && fresh==0 && !state.armed);
        assert(STEP(11u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        assert(held==0 && fresh==0);
        sample.buttons=0;
        assert(STEP(11u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        assert(state.armed);
        /* D-pad movement may become held, not fresh button action. */
        sample.buttons=1u<<11;
        assert(STEP(11u,1)==SPIDER_SDL3_FRAME_READY);
        assert(held==1 && fresh==0);
        /* Two identical VID/PID products are distinguished by instance.
         * A newly selected instance cannot inherit the old held D-pad. */
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        assert(held==0 && fresh==0 && !state.armed);
        sample.buttons=0;
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        sample.buttons=1u<<0;
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_READY);
        assert(held==0x10 && fresh==0x10);
        /* Updating a semantic mapping requires a fresh neutral sample
         * even when the same SDL instance and same VID/PID remain. */
        assert(spider_sdl3_profile_assign(
          &p,SPIDER_SDL3_ACTION_KICK,spider_sdl3_bind(1,2)));
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        sample.buttons=0;
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        sample.buttons=1u<<2;
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_READY);
        assert(held==0x20 && fresh==0x20); /* West -> Kick */
        /* Invalid device/profile identity is fail-closed. */
        assert(spider_sdl3_generic_frame_step(
          &state,&p,&sample,12u,0x045eu,0x028eu,1,
          &held,&fresh)==SPIDER_SDL3_FRAME_REJECTED);
        assert(held==0 && fresh==0);
        /* SDL3 normalized trigger values are never negative. */
        sample.buttons=0;sample.axis[5]=-1;
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        sample.axis[5]=0;
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_WAIT_NEUTRAL);
        assert(state.armed);
        sample.axis[5]=-1;
        assert(STEP(12u,1)==SPIDER_SDL3_FRAME_REJECTED);
        assert(held==0 && fresh==0 && !state.armed);
        return 0;
      }
    """),encoding="utf-8")
    executable=tmp_path/"generic_frame"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(program),"-o",str(executable)],
                   check=True,text=True,capture_output=True)
    subprocess.run([str(executable)],check=True)


def test_implementation_remains_detached_from_original_game():
    src=(ROOT/"sdl3_generic_frame_preview.h").read_text(encoding="utf-8")
    assert "spider_sdl3_generic_engine_preview_edges(" in src
    assert "spider_sdl3_profile_neutral(sample)" in src
    assert "state->instance!=instance" in src
    assert "state->actions[i]" in src
    assert "SPIDER_SDL3_FRAME_WAIT_NEUTRAL" in src
    assert "SPIDER_SDL3_FRAME_REJECTED" in src
    for forbidden in ("windows.h","dinput.h","GetDeviceState","SendInput",
                      "VirtualProtect","WriteProcessMemory","SDL_OpenGamepad"):
        assert forbidden not in src
    for runtime in ("spiderman2k1fix.c","sdl3_gamepad_probe.c",
                    "sdl3_gameplay_live.c"):
        assert '#include "sdl3_generic_frame_preview.h"' not in (
            ROOT/runtime).read_text(encoding="utf-8")
