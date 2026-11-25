"""Portable hardware-independent activity-owner regression tests.

SDL3 controller selection is single player: Start is never a special
claim key; the first freshly active pad wins, with no drift/held takeover.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("cc") or shutil.which("gcc")),
                    reason="C compiler unavailable")
def test_activity_owner_neutral_guard_hotplug_focus_and_simultaneous(tmp_path):
    src=tmp_path/"activity.c"
    exe=tmp_path/"activity"
    src.write_text(textwrap.dedent(r"""
    #include <assert.h>
    #include "sdl3_activity_owner.h"
    int main(void) {
      SpiderSDL3ActivityOwners state={0};
      SpiderSDL3ControllerSample neutral={0},a={0},b={0},drift={0};
      const uint32_t pair[]={11u,22u},one[]={22u},new_pair[]={22u,33u};
      spider_sdl3_activity_sync(&state,pair,2u);
      assert(state.count==2u);
      a.buttons=1u<<0; /* South held before startup => must not claim */
      assert(!spider_sdl3_activity_edge(&state,11u,&a));
      assert(!spider_sdl3_activity_edge(&state,11u,&a));
      assert(!spider_sdl3_activity_edge(&state,11u,&neutral));
      assert(spider_sdl3_activity_edge(&state,11u,&a));
      assert(!spider_sdl3_activity_edge(&state,11u,&a));
      a.buttons=1u<<SPIDER_SDL3_BUTTON_START; /* Start is not activation */
      assert(!spider_sdl3_activity_edge(&state,11u,&neutral));
      assert(!spider_sdl3_activity_edge(&state,11u,&a));
      a.buttons=1u<<SPIDER_SDL3_BUTTON_GUIDE; /* Guide ignored too */
      assert(!spider_sdl3_activity_edge(&state,11u,&neutral));
      assert(!spider_sdl3_activity_edge(&state,11u,&a));
      drift.axis[SPIDER_SDL3_AXIS_LEFTX]=6000; /* physical drift */
      assert(!spider_sdl3_activity_edge(&state,22u,&neutral));
      assert(!spider_sdl3_activity_edge(&state,22u,&drift));
      drift.axis[SPIDER_SDL3_AXIS_LEFTX]=19000; /* deliberate motion */
      assert(spider_sdl3_activity_edge(&state,22u,&drift));
      assert(!spider_sdl3_activity_edge(&state,22u,&drift));
      drift.axis[SPIDER_SDL3_AXIS_LEFTX]=0;
      spider_sdl3_activity_focus_lost(&state);
      b.buttons=1u<<0;
      assert(!spider_sdl3_activity_edge(&state,22u,&b));
      assert(!spider_sdl3_activity_edge(&state,22u,&neutral));
      assert(spider_sdl3_activity_edge(&state,22u,&b));
      spider_sdl3_activity_sync(&state,one,1u);
      assert(state.count==1u && state.slots[0].id==22u);
      spider_sdl3_activity_sync(&state,new_pair,2u);
      assert(state.count==2u && state.slots[1].id==33u);
      assert(!spider_sdl3_activity_edge(&state,33u,&b));
      assert(!spider_sdl3_activity_edge(&state,33u,&neutral));
      assert(spider_sdl3_activity_edge(&state,33u,&b));
      spider_sdl3_activity_reset(&state);
      assert(state.count==0u);
      return 0;
    }
    """),encoding="utf-8")
    subprocess.run([shutil.which("cc") or shutil.which("gcc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(src),"-o",str(exe)],check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)
