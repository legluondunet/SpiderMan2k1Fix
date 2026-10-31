"""Guarded SDL3 Confirm/Skip to native scripted 3D cutscene request.

The pure state machine is host-compiled; real 3D hardware acceptance and
the separate Cross/Jump suppression remain outstanding.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]

def test_legacy_cutscene_policy_is_preserved_but_no_longer_connected():
    # Keep the old pure model/header for compatibility with existing v2
    # profiles. Natural original game-owned Cross now skips 3D scenes;
    # never write a second SDL3 request for the same physical button.
    pure=(ROOT/"sdl3_context_cutscene_bridge.h").read_text(encoding="utf-8")
    main=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    assert "SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE" in pure
    assert "!verified_native_owner || !native_script_skip_allowed" in pure
    assert "spider_sdl3_profile_neutral(sample)" in pure
    assert "sdl3_cutscene_context_on_flip(" not in main
    assert "spider_sdl3_context_cutscene_step(" not in main
    assert "SDL3-CONFIRM-SKIP" not in main
    assert "PS1-3D-OPTIONS" in main  # legacy explicit opt-in remains
    assert "!SpiderSDL3Probe_NativeOwnerEnabled()" in main
    assert "*native_request=1u;" in main
    assert "gamepad_native_cutscene_skip(1,\"PS1-3D-OPTIONS\")" in main


def test_cutscene_policy_compiles_and_gates_profile_edges(tmp_path):
    cc=shutil.which("gcc") or shutil.which("cc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"cutscene.c"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_context_cutscene_bridge.h"
      #define STEP(pad,id,ok,allowed) spider_sdl3_context_cutscene_step(\
          &state,&profile,&pad,id,0x054c,0x09cc,ok,allowed)
      int main(void) {
        SpiderSDL3ContextCutsceneBridge state={0};
        SpiderSDL3ControllerProfile profile;
        SpiderSDL3ControllerSample sample={0};
        assert(spider_sdl3_profile_default(&profile,0x054c,0x09cc));
        /* Unified Confirm / Skip -> Square/West, same saved profile. */
        assert(spider_sdl3_profile_assign_confirm_skip(&profile,
                   spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,2)));
        sample.buttons=(1u<<2);
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_WAIT);
        sample.buttons=0;
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_WAIT);
        sample.buttons=(1u<<0); /* Cross/Jump is NOT an SDL3 skip edge */
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_NO_EDGE);
        sample.buttons=(1u<<2);
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_PRESS);
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_NO_EDGE);
        sample.buttons=0;
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_NO_EDGE);
        /* The native CutSceneSkipAllow gate disappears: never keep a
         * previously armed input edge alive across 3D gameplay. */
        sample.buttons=(1u<<2);
        assert(STEP(sample,1,1,0)==SPIDER_SDL3_CUTSCENE_REFUSED);
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_WAIT);
        sample.buttons=0;
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_WAIT);
        sample.buttons=(1u<<2);
        assert(STEP(sample,1,0,1)==SPIDER_SDL3_CUTSCENE_REFUSED);
        sample.buttons=0;
        assert(STEP(sample,1,1,1)==SPIDER_SDL3_CUTSCENE_WAIT);
        sample.buttons=(1u<<2);
        assert(STEP(sample,2,1,1)==SPIDER_SDL3_CUTSCENE_WAIT); /* new pad */
        sample.buttons=0;
        assert(STEP(sample,2,1,1)==SPIDER_SDL3_CUTSCENE_WAIT);
        sample.buttons=(1u<<2);
        assert(STEP(sample,2,1,1)==SPIDER_SDL3_CUTSCENE_PRESS);
        /* Absent optional binding never skips. */
        spider_sdl3_context_cutscene_reset(&state);
        assert(spider_sdl3_profile_assign_context(&profile,
                 SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE,
                 spider_sdl3_bind(SPIDER_SDL3_BIND_NONE,0)));
        sample.buttons=0;
        assert(STEP(sample,2,1,1)==SPIDER_SDL3_CUTSCENE_REFUSED);
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"cutscene"
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],
                   capture_output=True,text=True,check=True)
    subprocess.run([str(exe)],check=True)
