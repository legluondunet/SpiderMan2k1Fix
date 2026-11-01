"""Modal-only Start handoff defers the original DI owner until game Flip.

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


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_modal_start_same_displayed_and_simultaneous_press_policy_c99(tmp_path):
    """L1/R1 browse then Start on current pad must request it for the game.

    Simultaneous fresh Start presses must be consumed yet not change the
    selected modal profile or schedule an unverified game handoff.
    """
    src=tmp_path/"modal_edges.c"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_gamepad_selection.h"
        int main(void) {
          SpiderSDL3Selection s={0};
          uint32_t ids[]={1u,2u};
          unsigned claims=0;
          uint32_t previous;
          spider_sdl3_selection_sync(&s,ids,2);
          s.active=2u; /* L1/R1 browsed Xbox; game still owns DS4 */
          assert(!spider_sdl3_selection_start_edge(&s,2u,1)); /* held */
          assert(!spider_sdl3_selection_start_edge(&s,2u,0)); /* arm */
          assert(spider_sdl3_selection_start_edge(&s,2u,1));  /* same row */
          assert(s.active==2u);
          assert(!spider_sdl3_selection_start_edge(&s,2u,1));
          /* Gameplay selection API still returns 0 when owner unchanged. */
          assert(!spider_sdl3_selection_start(&s,2u,0));
          assert(!spider_sdl3_selection_start(&s,2u,1));
          assert(s.active==2u);
          spider_sdl3_selection_focus_lost(&s);
          assert(!spider_sdl3_selection_start_edge(&s,1u,0));
          assert(!spider_sdl3_selection_start_edge(&s,2u,0));
          previous=s.active;
          claims+=(unsigned)spider_sdl3_selection_start_edge(&s,1u,1);
          claims+=(unsigned)spider_sdl3_selection_start_edge(&s,2u,1);
          assert(claims==2u);
          if(claims!=1u)s.active=previous;
          assert(s.active==2u); /* simultaneous requests refused */
          assert(!spider_sdl3_selection_start_edge(&s,1u,1));
          assert(!spider_sdl3_selection_start_edge(&s,2u,1));
          assert(!spider_sdl3_selection_start_edge(&s,1u,0));
          assert(spider_sdl3_selection_start_edge(&s,1u,1));
          assert(s.active==1u);
          return 0;
        }
    """),encoding="utf-8")
    binary=tmp_path/"modal_edges"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(src),"-o",str(binary)],check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_rescan_arms_new_reconnected_pad_from_verified_neutral_sample(tmp_path):
    """After hotplug/Alt+Tab, don't lose Start to an unarmed first timer."""
    source=tmp_path/"reconnect_neutral.c"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_gamepad_selection.h"
        int main(void) {
          SpiderSDL3Selection modal={0};
          uint32_t two[]={2u,3u}; /* Xbox=2, freshly reconnected DS4=3 */
          spider_sdl3_selection_sync(&modal,two,2);
          modal.active=3u; /* user browsed L1/R1 to DS4 */
          /* Scan saw Start released: safely arm BEFORE the first timer. */
          assert(!spider_sdl3_selection_start_edge(&modal,3u,0));
          assert(modal.slots[1].start_armed==1u);
          assert(spider_sdl3_selection_start_edge(&modal,3u,1));
          assert(modal.active==3u); /* fresh Start on displayed pad */
          /* Focus loss disarms held Start, not the current selection. */
          spider_sdl3_selection_focus_lost(&modal);
          assert(!spider_sdl3_selection_start_edge(&modal,3u,1));
          assert(!spider_sdl3_selection_start_edge(&modal,3u,0));
          assert(spider_sdl3_selection_start_edge(&modal,3u,1));
          /* A newly discovered pad with Start already HELD must fail
           * closed until a real released sample has been observed. */
          uint32_t ids[]={2u,4u};
          spider_sdl3_selection_sync(&modal,ids,2);
          assert(!spider_sdl3_selection_start_edge(&modal,4u,1));
          assert(!spider_sdl3_selection_start_edge(&modal,4u,0));
          assert(spider_sdl3_selection_start_edge(&modal,4u,1));
          return 0;
        }
    """),encoding="utf-8")
    binary=tmp_path/"reconnect_neutral"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(source),"-o",str(binary)],check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)


def test_modal_rescan_arming_and_traces_are_neutral_gated_and_capped():
    modal=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    assert "start_released_at_scan[i]=" in modal
    assert "sample.buttons&(1u<<SPIDER_SDL3_BUTTON_START)" in modal
    assert "if(opened_ids[i] && start_released_at_scan[i])" in modal
    assert "spider_sdl3_selection_start_edge(&ui->modal_start,opened_ids[i],0);" in modal
    assert "SDL3-SETUP: START-SAMPLE" in modal
    assert "SDL3-SETUP: PROFILE-BROWSE L1/R1" in modal
    assert "SDL3-SETUP: START-REFUSED simultaneous claims" in modal
    assert "ui->start_trace_count<32u" in modal
    assert "ui->start_owner_requested=1u;" in modal


def test_pad_close_transfers_displayed_profile_only_after_destroy():
    """Closing with DS4 Circle after R1 browse must not strand DS4 in menus.

    Keyboard Esc/mouse Close must keep previous native owner; the safety
    gate rejects a disconnected/ambiguous DS4 before native ownership.
    """
    modal=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    close=modal.split("static void setup_close_from_pad(",1)[1].split(
        "static SpiderSDL3ControllerProfile *current_profile(",1)[0]
    loop=modal.split("void SpiderSDL3ControllerSetup_Open(",1)[1]
    assert "if(!ui || ui->close_pending)return;" in close
    assert "if(ui->instance && ui->vid && ui->pid && !ui->capture.active)" in close
    assert "ui->start_owner_id=ui->instance;" in close
    assert "ui->start_owner_vid=ui->vid;" in close
    assert "ui->start_owner_pid=ui->pid;" in close
    assert "ui->start_owner_requested=1u;" in close
    assert "SDL3-SETUP: PAD-CLOSE GAME-OWNER requested" in close
    assert "setup_close_from_pad(ui," in modal
    assert "spider_sdl3_profile_neutral(&s)" in modal
    assert "PostMessageA(ui->hwnd,WM_CLOSE,0,0);" in modal
    assert "ui.start_owner_requested && !has_quit && restore_allowed &&" in loop
    assert "final_foreground==owner ? ui.start_owner_id:0u," in loop
    flip=probe.split("void SpiderSDL3Probe_OnFlip(",1)[1]
    assert "spider_sdl3_selected_model_unique(opened,num_pads," in flip
    assert "SDL3-PROBE: SETUP RETURN COMMITTED verified modal owner" in flip
    assert "SDL3-PROBE: SETUP RETURN REFUSED stale/ambiguous" in flip
    assert "SpiderNativeReconnect_" not in close
    assert "SendInput(" not in close
    assert "VirtualProtect(" not in close
    # Browsing never requests a handoff. A gamepad-initiated close does.
    select=modal.split("static void select_slot(",1)[1].split(
        "static void refresh_pads(",1)[0]
    assert "ui->start_owner_requested" not in select
    keyboard=modal.split("static int handle_setup_keyboard(",1)[1].split(
        "static LRESULT CALLBACK setup_wndproc(",1)[0]
    assert "setup_close_from_pad(" not in keyboard
    assert "L1/R1: browse pad" in modal
    assert "Circle/B: close + use this pad" in modal


def test_modal_start_defers_original_owner_until_next_verified_game_flip():
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
    assert "spider_sdl3_selection_start_edge(&ui->modal_start,id," in read
    assert "uint32_t before;" in read
    assert "ui->modal_start.active=before;" in read
    assert "if(next!=ui->slot)" in read
    assert "if(next==ui->slot)return 0;" not in read
    assert "SpiderSDL3Probe_SetupRead(i,&id,&vid,&pid," in read
    assert "claims!=1u || ui->capture.active || ui->close_pending" in read
    assert "SendMessageA(ui->pads,CB_SETCURSEL,next,0);" in read
    assert "select_slot(ui,next);" in read
    assert "if(!ui->test_mode && modal_start_handoff(ui))return;" in sample
    assert "spider_sdl3_selection_focus_lost(&ui->modal_start);" in sample
    assert "ui->start_owner_requested=1u;" in read
    assert "ui->start_owner_id=ui->instance;" in read
    assert "ui->start_owner_vid=ui->vid;" in read
    assert "ui->start_owner_pid=ui->pid;" in read
    assert "SpiderSDL3Probe_SetupModalReturn(" in modal
    assert "ui.start_owner_requested && !has_quit && restore_allowed &&" in modal
    assert "final_foreground==owner ? ui.start_owner_id:0u," in modal
    assert "void SpiderSDL3Probe_SetupModalReturn(uint32_t requested_id," in probe
    assert "void SpiderSDL3Probe_SetupModalReturn(uint32_t requested_id," in hdr
    handler=probe.split("void SpiderSDL3Probe_SetupModalReturn(",1)[1].split(
        "SpiderSDL3ControllerProfile *SpiderSDL3Probe_SetupProfile(",1)[0]
    assert "spider_sdl3_selection_focus_lost(&selection);" in handler
    assert "setup_owner_pending=requested_id;" in handler
    assert "setup_owner_vendor=vendor;" in handler
    assert "setup_owner_product=product;" in handler
    assert "selection.active=requested_id;" not in handler
    flip=probe.split("void SpiderSDL3Probe_OnFlip(",1)[1]
    assert "if(setup_owner_pending)" in flip
    assert "spider_sdl3_selected_model_unique(opened,num_pads," in flip
    assert "selection.active=requested_id;" in flip
    assert 'SDL3-PROBE: SETUP RETURN COMMITTED verified modal owner' in flip
    assert 'SDL3-PROBE: SETUP RETURN REFUSED stale/ambiguous' in flip
    assert flip.index("if(setup_owner_pending)")<flip.index(
        "for(i=0;i<num_pads;i++)if(pads[i].handle) {")
    # Manual L1/R1 browsing selects a profile but never requests transfer.
    select=modal.split("static void select_slot(",1)[1].split(
        "static void refresh_pads(",1)[0]
    assert "start_owner_requested" not in select
    for forbidden in ("SpiderNativeReconnect_", "VirtualProtect(",
                      "SendInput(", "WritePrivateProfileString", "CreateThread("):
        assert forbidden not in read
    assert "SDL3-SETUP: MODAL-START switched setup-only" in modal
    assert "SDL3-PROBE: SETUP RETURN Start takeover disarmed" in probe
