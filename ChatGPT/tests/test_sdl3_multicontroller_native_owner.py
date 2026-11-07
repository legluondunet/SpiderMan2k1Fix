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

def test_sdl3_multicontroller_owner_is_explicit_generic_not_native_preset():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    assert '#include "sdl3_gamepad_identity.h"' in probe
    assert "spider_sdl3_selected_model_unique(" in probe
    assert "open[i].vendor=sdl_vendor(pads[i].id);" in probe
    assert "open[i].product=sdl_product(pads[i].id);" in probe
    assert "SpiderSDL3Probe_SelectedMappedSample(" in backend
    assert "spider_sdl3_universal_neutral(&sample)" in backend
    assert "SDL3-UNIVERSAL: START selected generic" in probe
    assert "CreateDevice(" not in native
    assert "swap_native_gameplay_mapping(" not in native
    assert "spider_sdl3_context_menu_reset(&sdl3_context_bridge)" not in ui
    assert "begin_scancode(" not in ui
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in (
        ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")


def test_unknown_physical_button_layout_never_reaches_native_action_table():
    policy=(ROOT/"sdl3_universal_policy.h").read_text(encoding="utf-8")
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert not (ROOT/"sdl3_gameplay_native_map.h").exists()
    assert "spider_sdl3_universal_step(" in backend
    assert "spider_sdl3_universal_scan_safe(" in policy
    assert "SPIDER_UNIVERSAL_DPAD_UP" in policy
    assert "SPIDER_UNIVERSAL_DPAD_LEFT" in policy
    assert "SDL3-UNIVERSAL: REFUSED no valid normalized profile" in backend
    assert "SpiderSDL3Probe_SelectedMappedSample(" in backend
