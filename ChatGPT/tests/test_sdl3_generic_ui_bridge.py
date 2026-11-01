"""Generic SDL3 menu/Bink policy: real C state-machine tests and wiring guards.

The only live actions tested here are NEW-PRESS 2D navigation and Bink
Return. Do NOT assert gameplay, still screens or cutscene support.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
POLICY = (ROOT / "sdl3_generic_ui_policy.h").read_text(encoding="utf-8")
UI = (ROOT / "gamepad_ui_profile.c").read_text(encoding="utf-8")
PROBE = (ROOT / "sdl3_gamepad_probe.c").read_text(encoding="utf-8")


def test_generic_ui_engine_is_bounded_and_not_directinput_owner():
    assert "SpiderSDL3Probe_GenericSelectedSample(&id,&vid,&pid,&sample)" in UI
    assert "sdl3_generic_ui_on_flip(hwnd,menu_2d,gameplay_3d," in UI
    assert "if(sdl3_generic_ui_on_flip(hwnd,menu_2d,gameplay_3d," in UI
    assert "spider_sdl3_generic_ui_step(&generic_ui,id,ctx,&sample)" in UI
    assert "spider_sdl3_generic_ui_reset(&generic_ui)" in UI
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3GenericMenu",1,ini)' in UI
    assert "SPIDER_GENERIC_UI_BINK" in UI
    assert "SPIDER_GENERIC_UI_MENU" in UI
    assert "SPIDER_GENERIC_UI_NONE" in UI
    assert "begin_scancode(scan," in UI
    assert "release_active_key();" in UI
    assert "if(device)disable_profile(" in UI
    assert "sdl3_ui_instance=0;" in UI
    assert "SDL3-GENERIC-UI: PRESS" in UI
    assert "SDL3-GENERIC-UI: ARMED" in UI
    assert "KEYEVENTF_EXTENDEDKEY" in UI
    assert "in.ki.wScan=(WORD)(scan&0x7Fu);" in UI
    assert "in.ki.wScan=(WORD)(active_scan&0x7Fu);" in UI
    assert "KEYEVENTF_KEYUP" in UI
    assert "generic_selected_id=requested_id;" in PROBE
    assert "No 3D gameplay" in POLICY


@pytest.mark.skipif(not (shutil.which("cc") or shutil.which("gcc")),
                    reason="No host C compiler available")
def test_generic_ui_policy_c_real_neutral_edges_context_gates():
    # Compile and execute the actual portable C policy, not a reimplementation
    # in Python. The CI runner also compiles Windows x86 integration.
    program = r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "sdl3_generic_ui_policy.h"
#define BIT(n) (1u<<(n))
int main(void) {
    SpiderSDL3GenericUIState state={0};
    SpiderSDL3ControllerSample sample={0};
    unsigned r;
    /* Must see a neutral frame before first press. */
    sample.buttons=BIT(0);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=BIT(0);
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_ENTER);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    /* Another physical button is suppressed until all controls neutral. */
    sample.buttons=BIT(1);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=BIT(1);
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_ESCAPE);
    /* A held menu confirm may never skip the very next Bink video. */
    sample.buttons=BIT(0);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample));
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample));
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample));
    sample.buttons=BIT(0);
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample)==SPIDER_GENERIC_DIK_ENTER);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample));
    /* Bink has NO back/arrow/menu/Start synthesis. */
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample));
    sample.buttons=BIT(1);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample));
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample));
    sample.buttons=BIT(6);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_BINK,&sample));
    /* No input in original 3D; every context change resets arming. */
    sample.buttons=BIT(0);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_NONE,&sample));
    sample.buttons=BIT(0);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=BIT(11);
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_UP);
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=BIT(12);
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_DOWN);
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=BIT(13);
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_LEFT);
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=BIT(14);
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_RIGHT);
    sample.buttons=BIT(11)|BIT(14);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    /* Analog left stick mapped to menu directions with deadzone. */
    sample.axis[SPIDER_SDL3_AXIS_LEFTX]=-27000;
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_LEFT);
    sample.axis[SPIDER_SDL3_AXIS_LEFTX]=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.axis[SPIDER_SDL3_AXIS_LEFTY]=27000;
    assert(spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_DOWN);
    sample.axis[SPIDER_SDL3_AXIS_LEFTY]=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    /* Diagonal/ambiguous input is consumed, not guessed or repeated. */
    sample.axis[SPIDER_SDL3_AXIS_LEFTX]=27000;
    sample.axis[SPIDER_SDL3_AXIS_LEFTY]=-27000;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.axis[SPIDER_SDL3_AXIS_LEFTX]=0;
    sample.axis[SPIDER_SDL3_AXIS_LEFTY]=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    /* Extra unrelated shoulder, Guide or Start invalidates combo. */
    sample.buttons=BIT(0)|BIT(9);
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    /* Trigger prevents face action. */
    sample.buttons=BIT(0);
    sample.axis[SPIDER_SDL3_AXIS_RT]=16000;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=0;sample.axis[SPIDER_SDL3_AXIS_RT]=0;
    assert(!spider_sdl3_generic_ui_step(&state,3,SPIDER_GENERIC_UI_MENU,&sample));
    /* Identity switch MUST rearm; no stale press from another pad. */
    sample.buttons=BIT(0);
    assert(!spider_sdl3_generic_ui_step(&state,4,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=0;
    assert(!spider_sdl3_generic_ui_step(&state,4,SPIDER_GENERIC_UI_MENU,&sample));
    sample.buttons=BIT(0);
    assert(spider_sdl3_generic_ui_step(&state,4,SPIDER_GENERIC_UI_MENU,&sample)==SPIDER_GENERIC_DIK_ENTER);
    spider_sdl3_generic_ui_reset(&state);
    assert(!state.id && !state.armed && !state.awaiting_neutral);
    r=spider_sdl3_generic_ui_step(&state,0,SPIDER_GENERIC_UI_MENU,&sample);
    assert(!r && !state.armed);
    return 0;
}
"""
    compiler = shutil.which("cc") or shutil.which("gcc")
    with tempfile.TemporaryDirectory() as temp:
        source = Path(temp) / "generic_ui.c"
        executable = Path(temp) / "generic_ui"
        source.write_text(program, encoding="utf-8")
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra",
                        "-Werror", "-I", str(ROOT), str(source),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
