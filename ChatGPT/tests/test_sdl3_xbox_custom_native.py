"""Hardware-grounded Xbox 360 SDL3 3D customization (PR #80).

The supported 045E:028E physical DirectInput A/B/X/Y indices are 0..3;
LB/RB are the canonical 4/5. Native Swing must remain unbound 0x4000,
with the existing guarded SDL3 RT keyboard bridge preserving Swing.
Do not infer unsupported LT or POV indices from this test.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="host C99 compiler unavailable")
def test_xbox_360_swapped_b_x_y_a_face_buttons_produce_verified_native_table(tmp_path):
    src = tmp_path / "xbox_custom.c"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <string.h>
        #include "sdl3_gameplay_native_map.h"
        #include "native_joystick_swap_probe.h"
        int main(void) {
          SpiderSDL3ControllerProfile xbox,sony,invalid;
          uint32_t actual[SPIDER_PS1_BUTTON_COUNT],old[SPIDER_PS1_BUTTON_COUNT];
          static const uint32_t defaults[]={0,1,2,3,4,5,0x4000u};
          static const uint32_t swapped[]={3,2,1,0,4,5,0x4000u};
          assert(spider_sdl3_profile_default(&xbox,0x045eu,0x028eu));
          assert(spider_sdl3_native_xbox360_seven(&xbox,actual));
          assert(memcmp(actual,defaults,sizeof(defaults))==0);
          assert(memcmp(actual,spider_xbox360_buttons_sdl3_rt,sizeof(defaults))==0);
          /* B and X swap Kick/Punch: B=1, X=2. */
          assert(spider_sdl3_profile_assign(&xbox,SPIDER_SDL3_ACTION_KICK,
                                            spider_sdl3_bind(1,2)));
          /* Y and A swap Web/Jump: Y=3, A=0. */
          assert(spider_sdl3_profile_assign(&xbox,SPIDER_SDL3_ACTION_WEB,
                                            spider_sdl3_bind(1,0)));
          assert(spider_sdl3_profile_valid(&xbox));
          assert(spider_sdl3_native_xbox360_seven(&xbox,actual));
          assert(memcmp(actual,swapped,sizeof(swapped))==0);
          assert(spider_native_swap_preset(actual)==0u);
          /* Never invent a physical DI button for the RT Swing axis. */
          assert(actual[SPIDER_SDL3_ACTION_SWING]==0x4000u);
          /* Sony's verified mapping must remain independent. */
          assert(spider_sdl3_profile_default(&sony,0x054cu,0x09ccu));
          assert(spider_sdl3_native_ds4_seven(&sony,old));
          assert(memcmp(old,spider_ps1_buttons,sizeof(old))==0);
          /* Unsupported Xbox face binding: guide/Start/POV/L2 fail closed.
           * The portable profile accepts an LT Swing setting, but this
           * native bridge MUST refuse it until physically verified. */
          invalid=xbox;
          assert(spider_sdl3_profile_assign(&invalid,SPIDER_SDL3_ACTION_SWING,
                                            spider_sdl3_bind(2,SPIDER_SDL3_AXIS_LT)));
          assert(!spider_sdl3_native_xbox360_seven(&invalid,actual));
          invalid=xbox;
          assert(spider_sdl3_profile_assign(&invalid,SPIDER_SDL3_ACTION_JUMP,
                                            spider_sdl3_bind(1,11)));
          assert(!spider_sdl3_native_xbox360_seven(&invalid,actual));
          invalid=xbox;
          invalid.vendor=0x054cu; /* Sony model is not an Xbox */
          assert(!spider_sdl3_native_xbox360_seven(&invalid,actual));
          invalid=xbox;
          invalid.product=0x028fu; /* unverified variant */
          assert(!spider_sdl3_native_xbox360_seven(&invalid,actual));
          return 0;
        }
    """),encoding="utf-8")
    exe=tmp_path/"xbox_custom"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)


def test_xbox_custom_runtime_requires_owner_exact_table_and_swing_rt_preservation():
    live=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    assert "spider_sdl3_native_xbox360_seven(profile,desired)" in live
    assert "canonical=spider_xbox360_buttons_sdl3_rt;" in live
    assert "!same7(current,canonical)" in live
    assert "SpiderSDL3GameplayLive_PreNativeOnFlip(game_hwnd);" in (
        ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    assert "SpiderSDL3GameplayLive_VerifiedXboxSwingMap()" in ui
    assert "mapped[SPIDER_SDL3_ACTION_SWING]!=0x4000u" in live
    assert "!same7(current,mapped)" in live
    assert "!SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in live
    assert "if(xbox360)return;" in live
    assert "SpiderNativeReconnect_CanonicalXboxMapping()" in ui
    assert "spider_native_reconnect_canonical_mapping(SPIDER_RECONNECT_XBOX360)" in native
    assert "SDL3-3D-LIVE: APPLIED verified Xbox360" in live
    assert "REFUSED Xbox360 profile" in live
