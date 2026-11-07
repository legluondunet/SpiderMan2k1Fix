"""DS3 054C:0268 discovery is not proof of verified game-owned DI mapping.

Keep the known Sony DS4 and wired Xbox paths unchanged. The DS3 is
SDL3-mapped but no native action-table/axis/button correspondence has
been measured on Windows 11: raw SDL trace only, no guessed remap.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 host compiler required")
def test_ps3_is_not_a_known_verified_native_owner(tmp_path):
    src=tmp_path/"ps3_policy.c"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "native_joystick_reconnect_policy.h"
        int main(void) {
          /* SDL3 may report a PS3 gamepad. The original 2001 game may
             only be rebound after exact DirectInput hardware mapping. */
          assert(spider_reconnect_kind(0x054c,0x0268)==SPIDER_RECONNECT_NONE);
          assert(spider_reconnect_kind(0x054c,0x09cc)==SPIDER_RECONNECT_DS4);
          assert(spider_reconnect_kind(0x045e,0x028e)==SPIDER_RECONNECT_XBOX360);
          return 0;
        }
    """),encoding="utf-8")
    exe=tmp_path/"ps3_policy"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)


def test_ds3_probe_is_bounded_read_only_and_preserves_xbox_ds4_owner():
    s=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    start=s.index("void SpiderSDL3Probe_OnFlip(")
    flip=s[start:]
    assert "#include \"native_joystick_reconnect_policy.h\"" in s
    assert "vid==0x054cu && pid==0x0268u && ps3_trace_lines<48u" in flip
    assert "SDL3-PS3-PROBE: id=%lu VID:PID=054C:0268" in flip
    assert "buttons=0x%06lX changed=0x%06lX START=%u" in flip
    assert "spider_reconnect_kind(vid,pid)==SPIDER_RECONNECT_NONE" in flip
    assert "selection.active=previous;" in flip
    assert "SDL3-OWNER: START REFUSED unsupported native controller" in flip
    # Generic selection of unsupported pads is read-only, distinct from
    # the pre-existing verified native controller ownership path.
    assert "spider_reconnect_kind(vendor,product)==SPIDER_RECONNECT_NONE" in s
    assert "SDL3-GENERIC: SELECTED explicit modal Start" in s
    assert "SpiderSDL3Probe_GenericSelectedSample(" in s
    assert "SpiderNativeReconnect_" not in (
        ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    live=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
    universal=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert "old fixed native action-map backend retired" in live
    assert "SDL3-3D-LIVE: APPLIED verified" not in live
    assert "SpiderSDL3Probe_SelectedMappedSample(" in universal
    assert "spider_sdl3_universal_neutral(&sample)" in universal
