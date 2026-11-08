"""Universal SDL3 instance identity: duplicate-model-safe, native DI untouched.

C99 policy checks are deliberately independent of hardware. Real Wine/Windows
input, mirrored virtual devices and post-disconnect KEYUP require game tests.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT = Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_duplicate_model_instance_owner_and_simultaneous_start(tmp_path):
    src=tmp_path/"same_model.c"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_gamepad_identity.h"
      #include "sdl3_gamepad_selection.h"
      int main(void) {
        SpiderSDL3Identity pads[]={
          {11u,0x045eu,0x028eu},  /* Xbox-compatible EasySMX white */
          {22u,0x045eu,0x028eu},  /* second Xbox-compatible pad */
          {33u,0x054cu,0x0268u}   /* PS3 */
        };
        SpiderSDL3Selection selected={0};
        SpiderSDL3StartClaims claims={0};
        uint32_t ids[]={11u,22u,33u};
        /* Native DI model identity is unchanged and MUST reject twins. */
        assert(!spider_sdl3_selected_model_unique(pads,3,11u,0x045e,0x028e));
        assert(!spider_sdl3_selected_model_unique(pads,3,22u,0x045e,0x028e));
        /* Universal SDL3 keyboard-only sampling accepts either exact ID. */
        assert(spider_sdl3_selected_instance_valid(pads,3,11u,0x045e,0x028e));
        assert(spider_sdl3_selected_instance_valid(pads,3,22u,0x045e,0x028e));
        assert(spider_sdl3_selected_instance_valid(pads,3,33u,0x054c,0x0268));
        assert(!spider_sdl3_selected_instance_valid(pads,3,22u,0x054c,0x0268));
        assert(!spider_sdl3_selected_instance_valid(pads,3,44u,0x045e,0x028e));
        assert(!spider_sdl3_selected_instance_valid(pads,3,22u,0u,0x028e));
        assert(!spider_sdl3_selected_instance_valid(pads,3,0u,0x045e,0x028e));
        assert(!spider_sdl3_selected_instance_valid(0,3,22u,0x045e,0x028e));
        assert(!spider_sdl3_selected_instance_valid(pads,17,22u,0x045e,0x028e));
        /* Invalid duplicated instance ID must never authorize a sample. */
        pads[2].instance_id=22u;
        assert(!spider_sdl3_selected_instance_valid(pads,3,11u,0x045e,0x028e));
        assert(!spider_sdl3_selected_instance_valid(pads,3,22u,0x045e,0x028e));
        pads[2].instance_id=33u;
        /* Disconnect/replug gets a NEW ID; old owner cannot resurface. */
        pads[0].instance_id=0u;
        assert(!spider_sdl3_selected_instance_valid(pads,3,11u,0x045e,0x028e));
        assert(spider_sdl3_selected_instance_valid(pads,3,22u,0x045e,0x028e));
        pads[0].instance_id=44u;
        assert(spider_sdl3_selected_instance_valid(pads,3,44u,0x045e,0x028e));
        assert(!spider_sdl3_selected_instance_valid(pads,3,11u,0x045e,0x028e));
        /* Never pick first enumerated Start: two new edges refuse handoff. */
        spider_sdl3_selection_sync(&selected,ids,3u);
        assert(selected.active==0u);
        assert(!spider_sdl3_selection_start_edge(&selected,11u,0));
        assert(!spider_sdl3_selection_start_edge(&selected,22u,0));
        assert(spider_sdl3_selection_start_edge(&selected,11u,1));
        spider_sdl3_start_claim_add(&claims,11u,0x045e,0x028e);
        assert(spider_sdl3_selection_start_edge(&selected,22u,1));
        spider_sdl3_start_claim_add(&claims,22u,0x045e,0x028e);
        assert(claims.count==2u && claims.instance_id==0u);
        /* Start remains consumed: no repeat until fresh release+press. */
        assert(!spider_sdl3_selection_start_edge(&selected,11u,1));
        assert(!spider_sdl3_selection_start_edge(&selected,22u,1));
        claims=(SpiderSDL3StartClaims){0};
        assert(!spider_sdl3_selection_start_edge(&selected,11u,0));
        assert(spider_sdl3_selection_start_edge(&selected,11u,1));
        spider_sdl3_start_claim_add(&claims,11u,0x045e,0x028e);
        assert(claims.count==1u && claims.instance_id==11u);
        spider_sdl3_selection_focus_lost(&selected);
        assert(!spider_sdl3_selection_start_edge(&selected,22u,1));
        assert(!spider_sdl3_selection_start_edge(&selected,22u,0));
        assert(spider_sdl3_selection_start_edge(&selected,22u,1));
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"same_model"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),"-std=c99",
                    "-Wall","-Wextra","-Werror","-I",str(ROOT),str(src),
                    "-o",str(exe)],check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)


def test_duplicate_models_use_instance_only_on_universal_path():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    identity=(ROOT/"sdl3_gamepad_identity.h").read_text(encoding="utf-8")
    universal=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    native=probe.split("int SpiderSDL3Probe_SelectedIdentity(",1)[1].split(
        "static int spider_raw_local_path(",1)[0]
    generic=probe.split("int SpiderSDL3Probe_GenericSelectedSample(",1)[1].split(
        "/* SDL3 exposes each trigger",1)[0]
    modal=probe.split("if(setup_owner_pending) {",1)[1].split(
        "sdl3_trace_native_owner(now);",1)[0]
    loop=probe.split("void SpiderSDL3Probe_OnFlip(",1)[1]
    assert "spider_sdl3_selected_model_unique(" in native
    assert "spider_sdl3_selected_instance_valid(" not in native
    assert "spider_sdl3_selected_instance_valid(" in generic
    assert "spider_sdl3_selected_model_unique(" in generic
    assert "spider_sdl3_selected_instance_valid(" in modal
    assert "spider_sdl3_selected_model_unique(" in modal
    assert "start_claims.count==1u" in loop
    assert "spider_sdl3_start_claim_add(&start_claims,pads[i].id," in loop
    assert "generic_selected_id=start_claims.instance_id;" in loop
    assert "selected-id=%lu" in universal
    assert "if(active && selected_id!=id)restore_original(" in universal
    assert "release_all()" in universal
    assert "spider_sdl3_universal_neutral(&sample)" in universal
    assert "spider_sdl3_selected_instance_valid(" in identity
    assert "CreateDevice(" not in (ROOT/"native_joystick_reconnect.c").read_text()
