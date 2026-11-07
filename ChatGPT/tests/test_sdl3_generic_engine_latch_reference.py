"""Read-only 16-byte native action latch model from the exact retail EXE."""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_engine_original_action_latch_reference(tmp_path):
    src=tmp_path/"latch_ref.c"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <stddef.h>
      #include "sdl3_generic_engine_latch_reference.h"
      int main(void) {
        SpiderSDL3EngineLatchReference p={0};
        assert(sizeof(p)==16u);
        assert(offsetof(SpiderSDL3EngineLatchReference,event_indicator)==1u);
        assert(offsetof(SpiderSDL3EngineLatchReference,early_press)==2u);
        assert(offsetof(SpiderSDL3EngineLatchReference,held_frames)==4u);
        assert(offsetof(SpiderSDL3EngineLatchReference,released_frames)==8u);
        assert(offsetof(SpiderSDL3EngineLatchReference,since_press)==12u);
        assert(!spider_sdl3_engine_latch_reference_step(&p,0u));
        assert(p.released_frames==1u && p.since_press==1u);
        assert(spider_sdl3_engine_latch_reference_step(&p,0x10u));
        assert(p.held==1u && p.held_frames==1u);
        assert(p.event_indicator==1u && p.early_press==1u);
        assert(p.since_press==0u);
        assert(spider_sdl3_engine_latch_reference_step(&p,0x10u));
        assert(p.held_frames==2u && p.early_press==0u);
        /* This function does not reset event_indicator: separate consumer. */
        assert(p.event_indicator==1u);
        assert(!spider_sdl3_engine_latch_reference_step(&p,0u));
        assert(p.held==0u && p.held_frames==0u && p.released_frames==1u);
        for(unsigned i=0;i<12u;i++)
          assert(!spider_sdl3_engine_latch_reference_step(&p,0u));
        assert(spider_sdl3_engine_latch_reference_step(&p,0x20u));
        assert(p.early_press==0u && p.held_frames==1u);
        assert(!spider_sdl3_engine_latch_reference_step(0,0u));
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"latch"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(src),"-o",str(exe)],check=True,text=True,capture_output=True)
    subprocess.run([str(exe)],check=True)


def test_reference_unwired_no_native_memory_access():
    src=(ROOT/"sdl3_generic_engine_latch_reference.h").read_text(
        encoding="utf-8")
    assert "0x00479540" in src
    assert "0x004796A0" in src
    for forbidden in ("windows.h","dinput.h","GetDeviceState",
                      "VirtualProtect","WriteProcessMemory","SendInput"):
        assert forbidden not in src
    for name in ("spiderman2k1fix.c","sdl3_gameplay_live.c",
                 "sdl3_universal_gameplay.c"):
        assert '#include "sdl3_generic_engine_latch_reference.h"' not in (
            ROOT/name).read_text(encoding="utf-8")
