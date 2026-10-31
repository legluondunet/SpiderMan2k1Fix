"""Selected SDL3 model uniqueness: two distinct pads may coexist safely.

This portable policy does NOT establish native Windows device identity on
real hardware, nor does it enable L2/POV action mappings.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="host C compiler unavailable")
def test_unique_selected_vid_pid_c99(tmp_path):
    src=tmp_path/"identity.c"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_gamepad_identity.h"
      int main(void) {
        SpiderSDL3Identity two[]={
          {11u,0x054cu,0x09ccu}, /* Sony DS4 */
          {22u,0x045eu,0x028eu}  /* wired Xbox 360 */
        };
        SpiderSDL3Identity identical[]={
          {11u,0x054cu,0x09ccu},
          {33u,0x054cu,0x09ccu}
        };
        assert(spider_sdl3_selected_model_unique(two,2,11,0x054c,0x09cc));
        assert(spider_sdl3_selected_model_unique(two,2,22,0x045e,0x028e));
        assert(spider_sdl3_selected_model_unique(two,1,11,0x054c,0x09cc));
        assert(!spider_sdl3_selected_model_unique(two,2,33,0x054c,0x09cc));
        assert(!spider_sdl3_selected_model_unique(two,2,11,0x045e,0x028e));
        assert(!spider_sdl3_selected_model_unique(identical,2,11,0x054c,0x09cc));
        assert(!spider_sdl3_selected_model_unique(identical,2,33,0x054c,0x09cc));
        identical[1].instance_id=11; /* duplicate SDL instance is ambiguous */
        assert(!spider_sdl3_selected_model_unique(identical,2,11,0x054c,0x09cc));
        identical[1].instance_id=0;
        assert(spider_sdl3_selected_model_unique(identical,2,11,0x054c,0x09cc));
        assert(!spider_sdl3_selected_model_unique(two,0,11,0x054c,0x09cc));
        assert(!spider_sdl3_selected_model_unique(two,2,11,0,0x09cc));
        assert(!spider_sdl3_selected_model_unique(two,2,0,0x054c,0x09cc));
        assert(!spider_sdl3_selected_model_unique(0,2,11,0x054c,0x09cc));
        assert(!spider_sdl3_selected_model_unique(two,17,11,0x054c,0x09cc));
        return 0;
      }
    """),encoding="utf-8")
    cc=shutil.which("gcc") or shutil.which("cc")
    exe=tmp_path/"identity"
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True,
                   capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)

def test_sdl3_multicontroller_live_route_requires_verified_unique_native_owner():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text()
    live=(ROOT/"sdl3_gameplay_live.c").read_text()
    native=(ROOT/"native_joystick_reconnect.c").read_text()
    ui=(ROOT/"gamepad_ui_profile.c").read_text()
    assert '#include "sdl3_gamepad_identity.h"' in probe
    assert "spider_sdl3_selected_model_unique(" in probe
    assert "open[i].vendor=sdl_vendor(pads[i].id);" in probe
    assert "open[i].product=sdl_product(pads[i].id);" in probe
    assert "SpiderSDL3Probe_MappedCount()==0u" in live
    assert "SpiderSDL3Probe_MappedCount()>0u" in live
    assert "SpiderSDL3Probe_MappedCount()!=1u" not in live
    assert "SpiderSDL3Probe_MappedCount()==1u" not in live
    assert "SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id)" in live
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in live
    assert "SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id)" in native
    assert "target.matches!=1u" in native # refuse duplicate DirectInput products
    assert "spider_sdl3_profile_neutral(&sample)" in live
    assert "if(applied)restore(" in live
    assert "spider_sdl3_context_menu_reset(&sdl3_context_bridge)" in ui
    assert "SpiderSDL3GameplayLive_OwnsPause()" in ui
    assert "SpiderSDL3GameplayLive_PreNativeOnFlip(game_hwnd);" in (
        ROOT/"spiderman2k1fix.c").read_text()

def test_l2_pov_still_fail_closed_without_physical_evidence():
    mapping=(ROOT/"sdl3_gameplay_native_map.h").read_text()
    assert "Sony L2's 3D native rgbButton index has NOT been" in mapping
    assert "if(b.index==SPIDER_SDL3_AXIS_RT){*out=7u;return 1;}" in mapping
    assert "SPIDER_SDL3_AXIS_LT){*out=6u" not in mapping
    assert "D-pad is POV" in mapping
