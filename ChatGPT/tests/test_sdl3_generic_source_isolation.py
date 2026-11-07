"""Source separation contract on the verified game's keyboard + DirectInput path.

Research-only: does not authorize generic input ownership or patch game code.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_original_keyboard_plus_one_controller_only(tmp_path):
    src=tmp_path/"source_preview.c"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_generic_source_isolation.h"
      int main(void) {
        SpiderSDL3GenericSourceInput x={0};
        SpiderSDL3GenericSourceResult out={0};
        x.original_keyboard_held=0x80001001u;
        x.original_native_di_held=0x20u; /* original controller Kick */
        x.normalized_sdl3_held=0x40u; /* selected SDL3 Punch */
        assert(!spider_sdl3_generic_source_preview(&x,&out));
        assert(out.held==0x80001021u);
        assert(out.source==SPIDER_GENERIC_SOURCE_ORIGINAL);
        assert(out.controller_contribution==0x20u);
        assert(out.keyboard_retained==0x80001001u);
        assert(out.owner_instance_id==0u);
        /* Every single requirement is independently mandatory. */
        x.user_opted_in=1;
        x.exact_exe_verified=1;
        x.callsite_verified=1;
        x.foreground=1;
        x.playable_3d=1;
        x.sdl3_identity_verified=1;
        x.profile_verified=1;
        x.sampled_after_neutral=1;
        x.owner_instance_id=17u;
        assert(!spider_sdl3_generic_source_preview(&x,&out));
        assert(out.held==0x80001021u); /* native still NOT isolated */
        x.old_di_source_isolated=1;
        assert(spider_sdl3_generic_source_preview(&x,&out));
        assert(out.source==SPIDER_GENERIC_SOURCE_PREVIEW);
        assert(out.owner_instance_id==17u);
        assert(out.held==0x80001041u);
        assert((out.held & 0x20u)==0u); /* no native controller leak */
        assert((out.held & 0x40u)==0x40u); /* normalized controller */
        assert((out.held & 0x80001001u)==0x80001001u); /* keyboard preserved */
        #define REFUSE(field) do {\
          x.field=0;\
          assert(!spider_sdl3_generic_source_preview(&x,&out));\
          assert(out.held==0x80001021u);\
          x.field=1;\
        } while(0)
        REFUSE(user_opted_in);
        REFUSE(exact_exe_verified);
        REFUSE(callsite_verified);
        REFUSE(foreground);
        REFUSE(playable_3d);
        REFUSE(sdl3_identity_verified);
        REFUSE(profile_verified);
        REFUSE(sampled_after_neutral);
        REFUSE(old_di_source_isolated);
        x.native_menu_or_bink_or_pause=1;
        assert(!spider_sdl3_generic_source_preview(&x,&out));
        assert(out.held==0x80001021u);
        x.native_menu_or_bink_or_pause=0;
        x.owner_instance_id=0;
        assert(!spider_sdl3_generic_source_preview(&x,&out));
        x.owner_instance_id=17u;
        x.normalized_sdl3_held=0x80000000u; /* unknown injection flag */
        assert(!spider_sdl3_generic_source_preview(&x,&out));
        x.normalized_sdl3_held=0x7ffu;
        assert(spider_sdl3_generic_source_preview(&x,&out));
        assert(out.held==(0x80001001u|0x7ffu));
        assert(!spider_sdl3_generic_source_preview(0,&out));
        assert(out.held==0u && out.owner_instance_id==0u);
        assert(!spider_sdl3_generic_source_preview(&x,0));
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"source_preview"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],
                   capture_output=True,text=True,check=True)
    subprocess.run([str(exe)],check=True)


def test_source_policy_is_sandboxed_and_wont_rewire_existing_controller():
    src=(ROOT/"sdl3_generic_source_isolation.h").read_text(encoding="utf-8")
    for proof in ("0x0050A1AC","0x0050A29E","0x0050A2AD",
                  "0x0050A2B5","0x0050A371","old_di_source_isolated"):
        assert proof in src
    for unsafe in ("windows.h","dinput.h","VirtualProtect",
                   "WriteProcessMemory","SendInput","GetDeviceState",
                   "GetDeviceData"):
        assert unsafe not in src
    for runtime in ("spiderman2k1fix.c","sdl3_gameplay_live.c",
                    "sdl3_universal_gameplay.c"):
        assert '#include "sdl3_generic_source_isolation.h"' not in (
            ROOT/runtime).read_text(encoding="utf-8")
