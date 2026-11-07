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
def test_any_mapped_sdl3_model_is_allowed_only_when_unique(tmp_path):
    src=tmp_path/"sdl3_identity.c"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_gamepad_identity.h"
        int main(void) {
          SpiderSDL3Identity none[1]={{0}};
          SpiderSDL3Identity sony[2]={{42,0x054c,0x0268},{0,0,0}};
          SpiderSDL3Identity duplicate[2]={{42,0x054c,0x0268},{43,0x054c,0x0268}};
          assert(!spider_sdl3_selected_model_unique(none,0,42,0x054c,0x0268));
          assert(spider_sdl3_selected_model_unique(sony,1,42,0x054c,0x0268));
          assert(!spider_sdl3_selected_model_unique(duplicate,2,42,0x054c,0x0268));
          assert(!spider_sdl3_selected_model_unique(sony,1,43,0x054c,0x0268));
          return 0;
        }
    """),encoding="utf-8")
    exe=tmp_path/"sdl3_identity"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)


def test_ps3_probe_is_passive_and_does_not_create_native_owner():
    s=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    flip=s.split("void SpiderSDL3Probe_OnFlip(",1)[1]
    assert "vid==0x054cu && pid==0x0268u && ps3_trace_lines<48u" in flip
    assert "SDL3-PS3-PROBE: id=%lu VID:PID=054C:0268" in flip
    assert "buttons=0x%06lX changed=0x%06lX START=%u" in flip
    assert "selection.active=previous;" in flip
    assert "spider_sdl3_selection_start_edge(" in flip
    assert "SpiderSDL3Probe_GenericSelectedSample(" in s
    assert "spider_sdl3_selected_model_unique(" in s
    assert "spider_reconnect_kind(" not in s
    assert "SpiderNativeReconnect_" not in s
    assert "SDL3-UNIVERSAL: START selected generic" in flip
    assert "SDL3-GENERIC: SELECTED explicit modal Start" in s
    universal=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert "SpiderSDL3Probe_SelectedMappedSample(" in universal
    assert "spider_sdl3_universal_neutral(&sample)" in universal

