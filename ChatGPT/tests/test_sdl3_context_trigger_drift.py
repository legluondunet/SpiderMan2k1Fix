"""Issue #60: trigger idle drift must not permanently disarm menus or Pause.

Source evidence: SpiderMan2k1Fix(20261109-164033).log on Windows 10,
SDL3 logical Xbox One S pad 045E:02E0. LT frequently 6000..8500 after
Bink and throughout native Pause, while original strict neutral is <5000.
At the same time fresh trigger activation requires >=11000.
The hardware reporter's visible behavior is not proven fixed by this unit test.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("cc") or shutil.which("gcc")),
                    reason="C99 compiler unavailable")
def test_strict_owner_acquisition_but_live_context_rearms_below_press_threshold(tmp_path):
    source = tmp_path / "context_neutral.c"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_universal_policy.h"
      int main(void) {
        SpiderSDL3ControllerSample s={0};
        SpiderSDL3UniversalState st={0};
        SpiderSDL3ControllerProfile p={0};
        uint32_t desired=0xffffu;
        unsigned pause=1u;
        assert(spider_sdl3_profile_default(&p,0x045eu,0x02e0u));
        assert(spider_sdl3_universal_owner_neutral(&s));
        assert(spider_sdl3_universal_neutral(&s));

        /* Reproduce idle LT from the exact Win10 issue #60 log. */
        s.axis[SPIDER_SDL3_AXIS_LT]=8352;
        assert(!spider_sdl3_profile_neutral(&s));
        assert(!spider_sdl3_universal_owner_neutral(&s));
        assert(spider_sdl3_universal_neutral(&s));
        assert(spider_sdl3_universal_step(&st,&p,&s,1u,0,
                                         &desired,&pause)==0);
        assert(st.armed && !desired && !pause);
        s.buttons=1u<<0;
        assert(!spider_sdl3_universal_neutral(&s));
        assert(spider_sdl3_universal_step(&st,&p,&s,1u,0,
                                         &desired,&pause)==1);
        assert(desired & (1u<<SPIDER_UNIVERSAL_JUMP));
        s.buttons=0;
        assert(spider_sdl3_universal_step(&st,&p,&s,1u,0,
                                         &desired,&pause)==1);
        assert(!desired && !pause);

        /* A partial trigger below its first-press threshold is safe.
         * Hysteresis after a real press (>=11000) must remain intact. */
        s.axis[SPIDER_SDL3_AXIS_LT]=0;
        s.axis[SPIDER_SDL3_AXIS_RT]=10999;
        assert(spider_sdl3_universal_neutral(&s));
        assert(spider_sdl3_universal_step(&st,&p,&s,1u,0,
                                         &desired,&pause)==1);
        assert(!desired);
        s.axis[SPIDER_SDL3_AXIS_RT]=11000;
        assert(!spider_sdl3_universal_neutral(&s));
        assert(spider_sdl3_universal_step(&st,&p,&s,1u,0,
                                         &desired,&pause)==1);
        assert(desired & (1u<<SPIDER_UNIVERSAL_SWING));
        s.axis[SPIDER_SDL3_AXIS_RT]=7000;
        assert(spider_sdl3_universal_neutral(&s));
        assert(spider_sdl3_universal_step(&st,&p,&s,1u,0,
                                         &desired,&pause)==1);
        assert(desired & (1u<<SPIDER_UNIVERSAL_SWING));
        s.axis[SPIDER_SDL3_AXIS_RT]=4999;
        assert(spider_sdl3_universal_owner_neutral(&s));
        assert(spider_sdl3_universal_step(&st,&p,&s,1u,0,
                                         &desired,&pause)==1);
        assert(!desired);

        for(int i=0;i<2;i++) {
          spider_sdl3_universal_reset(&st);
          s.axis[SPIDER_SDL3_AXIS_RT]=i?32767:11000;
          assert(!spider_sdl3_universal_neutral(&s));
          assert(spider_sdl3_universal_step(&st,&p,&s,1u,0,
                                           &desired,&pause)==0);
          assert(!st.armed && !desired && !pause);
        }
        s.axis[SPIDER_SDL3_AXIS_RT]=0;
        s.axis[SPIDER_SDL3_AXIS_LT]=-1;
        assert(!spider_sdl3_universal_neutral(&s));
        s.axis[SPIDER_SDL3_AXIS_LT]=0;
        s.axis[SPIDER_SDL3_AXIS_LEFTX]=12000;
        assert(!spider_sdl3_universal_neutral(&s));
        s.axis[SPIDER_SDL3_AXIS_LEFTX]=0;
        s.buttons=1u<<SPIDER_SDL3_BUTTON_START;
        assert(!spider_sdl3_universal_neutral(&s));
        return 0;
      }
    """), encoding="utf-8")
    binary = tmp_path / "context_neutral"
    subprocess.run([shutil.which("cc") or shutil.which("gcc"),
                    "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT), str(source), "-o", str(binary)],
                   check=True, capture_output=True, text=True)
    subprocess.run([str(binary)], check=True)


def test_original_owner_gates_remain_strict_after_drift_fix():
    src = (ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    policy = (ROOT/"sdl3_universal_policy.h").read_text(encoding="utf-8")
    body = src.split("void SpiderSDL3Universal_OnFlip(",1)[1]
    assert "if(native_unclaimed_guard && !spider_sdl3_universal_owner_neutral(&sample))" in body
    assert "if(!suspended && !spider_sdl3_universal_owner_neutral(&sample))" in body
    assert "spider_sdl3_profile_neutral(sample)" in policy
    assert "sample->axis[SPIDER_SDL3_AXIS_LT]>=11000" in policy
    assert "sample->axis[SPIDER_SDL3_AXIS_RT]>=11000" in policy
    assert "if(spider_sdl3_universal_neutral(&sample)) {" in body
    assert "if(spider_sdl3_universal_neutral(sample))state->armed=1;" in policy
    assert "menu_arm=0;menu_previous=0;" in body
