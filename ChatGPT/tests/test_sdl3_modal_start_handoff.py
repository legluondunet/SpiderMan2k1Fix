"""Modal-only SDL3 Start handoff must never alter original DirectInput.

Regression: DS4 and Xbox connected; Xbox selected by Start in gameplay,
panel initially Xbox; Start on DS4 should select DS4 within modal even
though normal Flip-based gamepad selection is suspended; manual R1 remains.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_modal_selection_neutral_start_and_manual_handoff_c99(tmp_path):
    src=tmp_path/"modal_start.c"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_gamepad_selection.h"
        int main(void) {
          SpiderSDL3Selection modal={0};
          uint32_t ids[]={1u,2u}; /* DS4=1, Xbox=2 */
          spider_sdl3_selection_sync(&modal,ids,2);
          modal.active=2u; /* Xbox is displayed on initial open */
          assert(!spider_sdl3_selection_start(&modal,1u,1)); /* held */
          assert(modal.active==2u);
          assert(!spider_sdl3_selection_start(&modal,1u,0)); /* release arms */
          assert(spider_sdl3_selection_start(&modal,1u,1)); /* fresh DS4 */
          assert(modal.active==1u);
          assert(!spider_sdl3_selection_start(&modal,1u,1)); /* held */
          modal.active=2u; /* manual R1 selected Xbox profile */
          assert(!spider_sdl3_selection_start(&modal,1u,0));
          assert(spider_sdl3_selection_start(&modal,1u,1));
          assert(modal.active==1u);
          spider_sdl3_selection_focus_lost(&modal);
          assert(!spider_sdl3_selection_start(&modal,2u,1));
          assert(!spider_sdl3_selection_start(&modal,2u,0));
          assert(spider_sdl3_selection_start(&modal,2u,1));
          assert(modal.active==2u);
          ids[0]=0; /* DS4 disconnected */
          spider_sdl3_selection_sync(&modal,&ids[1],1);
          assert(modal.active==2u);
          return 0;
        }
    """),encoding="utf-8")
    binary=tmp_path/"modal_start"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(src),"-o",str(binary)],check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)


def test_modal_start_routes_only_to_setup_and_preserves_original_owner():
    modal=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    hdr=(ROOT/"sdl3_gamepad_probe.h").read_text(encoding="utf-8")
    read=modal.split("static int modal_start_handoff(",1)[1].split(
        "static void sample_device(",1)[0]
    sample=modal.split("static void sample_device(",1)[1].split(
        "static int handle_setup_keyboard(",1)[0]
    assert "#include \"sdl3_gamepad_selection.h\"" in modal
    assert "SpiderSDL3Selection modal_start;" in modal
    assert "spider_sdl3_selection_sync(&ui->modal_start,opened_ids,n);" in modal
    assert "ui->modal_start.active=id;" in modal
    assert "spider_sdl3_selection_start(&ui->modal_start,id," in read
    assert "SpiderSDL3Probe_SetupRead(i,&id,&vid,&pid," in read
    assert "claims!=1u || ui->capture.active || ui->close_pending" in read
    assert "SendMessageA(ui->pads,CB_SETCURSEL,next,0);" in read
    assert "select_slot(ui,next);" in read
    assert "if(modal_start_handoff(ui))return;" in sample
    assert "spider_sdl3_selection_focus_lost(&ui->modal_start);" in sample
    assert "SpiderSDL3Probe_SetupModalReturn();" in modal
    assert "void SpiderSDL3Probe_SetupModalReturn(void)" in probe
    assert "void SpiderSDL3Probe_SetupModalReturn(void);" in hdr
    assert "spider_sdl3_selection_focus_lost(&selection);" in probe.split(
        "void SpiderSDL3Probe_SetupModalReturn(void)",1)[1].split(
        "SpiderSDL3ControllerProfile *SpiderSDL3Probe_SetupProfile(",1)[0]
    for forbidden in ("SpiderNativeReconnect_", "VirtualProtect(",
                      "SendInput(", "WritePrivateProfileString", "CreateThread("):
        assert forbidden not in read
    assert "SDL3-SETUP: MODAL-START switched setup-only" in modal
    assert "SDL3-PROBE: SETUP RETURN Start takeover disarmed" in probe
