"""Pure-C99 research preview of SDL3 semantic input -> original game masks.

No runtime hook, no native owner, no executable supplied to CI.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_semantic_actions_and_dpad_stick_preview(tmp_path):
    src = tmp_path / "generic_preview.c"
    src.write_text(textwrap.dedent(r"""
       #include <assert.h>
       #include <string.h>
       #include "sdl3_generic_engine_preview.h"
       static int get(const SpiderSDL3ControllerProfile *profile,
                      const SpiderSDL3ControllerSample *s,
                      uint32_t old,uint32_t *new_action) {
         uint32_t mask=0;
         assert(spider_sdl3_generic_engine_preview(
             profile,s,old,&mask,new_action));
         return (int)mask;
       }
       int main(void) {
         SpiderSDL3ControllerProfile ds3,xbox;
         SpiderSDL3ControllerSample s={0};
         uint32_t held=0,mask=0;
         assert(spider_sdl3_profile_default(&ds3,0x054cu,0x0268u));
         assert(spider_sdl3_profile_default(&xbox,0x045eu,0x028eu));
         assert(get(&ds3,&s,0,&held)==0);
         s.buttons=1u<<0; /* SDL South -> Jump */
         assert(get(&ds3,&s,0,&held)==0x10);
         s.buttons=1u<<3; /* North -> Web */
         assert(get(&ds3,&s,0,&held)==0x80);
         s.buttons=1u<<4; /* optional Pause never enters native 7 actions */
         assert(get(&ds3,&s,0,&held)==0);
         s.buttons=0u;s.axis[5]=12000;
         assert(get(&ds3,&s,0,&held)==0x400);
         s.axis[5]=6000; /* release hysteresis */
         assert(get(&ds3,&s,held,&held)==0x400);
         s.axis[5]=4999;
         assert(get(&ds3,&s,held,&held)==0);
         s.axis[5]=0;
         s.axis[0]=18000; s.axis[1]=-18000;
         assert(get(&ds3,&s,0,&held)==0x9); /* right + up */
         s.buttons=(1u<<11)|(1u<<13); /* D-pad up + left overrides stick */
         assert(get(&ds3,&s,0,&held)==0x5);
         s.buttons=(1u<<11)|(1u<<12); /* opposite D-pad cancels vertical */
         assert(get(&ds3,&s,0,&held)==0x8); /* stick right remains */
         s.buttons=0u;s.axis[0]=0;s.axis[1]=0;
         /* User's Xbox swaps B/X and Y/A without hardware-specific
          * raw DirectInput indices: all semantic actions are remapped. */
         assert(spider_sdl3_profile_assign(&xbox,SPIDER_SDL3_ACTION_KICK,
                                            spider_sdl3_bind(1,2)));
         assert(spider_sdl3_profile_assign(&xbox,SPIDER_SDL3_ACTION_WEB,
                                            spider_sdl3_bind(1,0)));
         s.buttons=1u<<0;
         assert(get(&xbox,&s,0,&held)==0x80); /* Xbox A => Web */
         s.buttons=1u<<3;
         assert(get(&xbox,&s,0,&held)==0x10); /* Xbox Y => Jump */
         s.buttons=1u<<1;
         assert(get(&xbox,&s,0,&held)==0x40); /* Xbox B => Punch */
         s.buttons=1u<<2;
         assert(get(&xbox,&s,0,&held)==0x20); /* Xbox X => Kick */
         assert(!spider_sdl3_generic_engine_preview(
             &xbox,&s,0,0,&held));
         s.axis[5]=-1; /* SDL trigger never negative in valid sample */
         assert(!spider_sdl3_generic_engine_preview(
             &xbox,&s,0,&mask,&held));
         assert(mask==0 && held==0);
         return 0;
       }
    """),encoding="utf-8")
    executable=tmp_path/"generic_preview"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(executable)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(executable)],check=True)


def test_engine_preview_is_read_only_unwired_and_no_native_owner():
    source=(ROOT/"sdl3_generic_engine_preview.h").read_text(encoding="utf-8")
    assert "0x0050A2F9..0x0050A32D" in source
    assert "SPIDER_SDL3_ENGINE_DPAD_UP=11u" in source
    assert "SPIDER_SDL3_ENGINE_FIRST_ACTION=4u" in source
    assert "(actions&0x7fu)" in source
    assert "spider_sdl3_profile_actions" in source
    for forbidden in ("windows.h","dinput.h","SDL_OpenGamepad",
                      "VirtualProtect","SendInput","GetDeviceState"):
        assert forbidden not in source
    assert '#include "sdl3_generic_engine_preview.h"' not in (
        ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    assert '#include "sdl3_generic_engine_preview.h"' not in (
        ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
