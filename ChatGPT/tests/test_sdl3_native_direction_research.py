"""Exact native analog-centering and POV OR semantics, without runtime hooks."""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_native_analog_gate_center_and_pov_boundaries(tmp_path):
    source=tmp_path/"gate.c"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_native_direction_research.h"
      int main(void) {
        SpiderNativeDirectionResearch g={0};
        spider_native_direction_research_reset(&g);
        assert(g.block_axes_until_center && g.pov==-1);
        assert(spider_native_direction_research_peek(&g,1000,1000)==0);
        /* At +250 the latch must NOT clear. */
        assert(spider_native_direction_research_aggregator(&g,250,0)==0);
        assert(g.block_axes_until_center);
        /* The OTHER reader does not clear even on centered axes. */
        assert(spider_native_direction_research_peek(&g,0,0)==0);
        assert(g.block_axes_until_center);
        assert(spider_native_direction_research_aggregator(&g,249,-249)==0);
        assert(!g.block_axes_until_center);
        assert(spider_native_direction_research_aggregator(&g,-251,251)==6);
        assert(spider_native_direction_research_aggregator(&g,-250,250)==0);
        /* Real native OR: POV and stick may simultaneously disagree. */
        g.pov=4500;
        assert(spider_native_direction_research_aggregator(&g,-500,500)==15);
        assert(spider_native_direction_research_pov(-1)==0);
        assert(spider_native_direction_research_pov(0)==1);
        assert(spider_native_direction_research_pov(4500)==9);
        assert(spider_native_direction_research_pov(9000)==8);
        assert(spider_native_direction_research_pov(18000)==2);
        assert(spider_native_direction_research_pov(27000)==4);
        assert(spider_native_direction_research_pov(31500)==5);
        spider_native_direction_research_reset(&g);
        assert(g.pov==-1);
        assert(spider_native_direction_research_aggregator(&g,0,500)==0);
        assert(g.block_axes_until_center);
        assert(spider_native_direction_research_aggregator(&g,0,0)==0);
        assert(!g.block_axes_until_center);
        assert(!spider_native_direction_research_aggregator(0,0,0));
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"gate"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(exe)],
                   check=True,text=True,capture_output=True)
    subprocess.run([str(exe)],check=True)

def test_research_model_unwired_and_sha_grounded():
    p=(ROOT/"sdl3_native_direction_research.h").read_text(encoding="utf-8")
    for required in ("0x0050A2BD","0x0050A375","0x0050A9B0",
                     "0x0050A9D0","0x00AC0A18","(-250,+250)",
                     "state->block_axes_until_center=0u"):
        assert required in p
    for forbidden in ("windows.h","dinput.h","VirtualProtect",
                      "WriteProcessMemory","SendInput","SDL_OpenGamepad"):
        assert forbidden not in p
    for native in ("spiderman2k1fix.c","sdl3_gamepad_probe.c",
                   "native_joystick_reconnect.c"):
        assert '#include "sdl3_native_direction_research.h"' not in (
            ROOT/native).read_text(encoding="utf-8")
