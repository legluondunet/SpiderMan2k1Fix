"""Source-grounded first 20 native action latch destinations and masks."""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_original_engine_20_action_latches_are_16_bytes_apart(tmp_path):
    source=tmp_path/"layout.c"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_original_action_layout.h"
      int main(void) {
        const uint32_t mask[20]={
          0x80,0x40,0x20,0x10,0x100,0,0x200,0x400,
          4,8,1,2,0,0,0x1000,0,0x10,0x80,0x40,0x20
        };
        assert(SPIDER_ORIGINAL_INPUT_STATE_COUNT==20u);
        assert(SPIDER_ORIGINAL_INPUT_GLOBAL_COUNTER==0x0066129Cu);
        for(unsigned i=0;i<20u;i++) {
          assert(spider_original_action_latch_address(i)==
                 0x00661100u+16u*i);
          assert(spider_original_action_latch_mask(i)==mask[i]);
          assert(spider_original_action_latch_held(0xFFFFu,i)==
                 (mask[i]!=0u));
          assert(!spider_original_action_latch_held(0u,i));
        }
        assert(spider_original_action_latch_address(0)==0x00661100u);
        assert(spider_original_action_latch_address(19)==0x00661230u);
        assert(spider_original_action_latch_address(20)==0u);
        assert(spider_original_action_latch_mask(20)==0u);
        assert(!spider_original_action_latch_held(0xFFFFu,20u));
        assert(spider_original_action_latch_held(0x10u,3u));
        assert(spider_original_action_latch_held(0x10u,16u));
        assert(!spider_original_action_latch_held(0x10u,4u));
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"layout"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(exe)],
                   check=True,text=True,capture_output=True)
    subprocess.run([str(exe)],check=True)

def test_layout_is_research_only_and_silent():
    source=(ROOT/"sdl3_original_action_layout.h").read_text(
        encoding="utf-8")
    assert "0x00505720" in source
    assert "0x00479540" in source
    assert "No DirectInput hook" in source
    for ban in ("windows.h","dinput.h","VirtualProtect",
                "WriteProcessMemory","SendInput"):
        assert ban not in source
    for p in ("spiderman2k1fix.c","sdl3_gamepad_probe.c",
              "sdl3_gameplay_live.c"):
        assert '#include "sdl3_original_action_layout.h"' not in (
            ROOT/p).read_text(encoding="utf-8")
