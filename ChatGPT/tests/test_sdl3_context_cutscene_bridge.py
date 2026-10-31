"""Guarded SDL3 Confirm/Skip to native scripted 3D cutscene request.

The pure state machine is host-compiled; real 3D hardware acceptance and
the separate Cross/Jump suppression remain outstanding.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]

def test_native_permission_and_selected_owner_are_required():
    pure=(ROOT/"sdl3_context_cutscene_bridge.h").read_text(encoding="utf-8")
    main=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    assert "SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE" in pure
    assert "!verified_native_owner || !native_script_skip_allowed" in pure
    assert "spider_sdl3_profile_neutral(sample)" in pure
    assert "spider_sdl3_context_binding_down(bind,sample" in pure
    assert "SPIDER_SDL3_CUTSCENE_PRESS" in pure
    assert "spider_sdl3_context_cutscene_reset(s)" in pure
    assert "sdl3_cutscene_context_on_flip(menu_2d,gameplay_3d,bink_active,native_pause_active);" in main
    fn=main.split("static void sdl3_cutscene_context_on_flip(",1)[1].split(
        "/* Early game HWND acquisition",1)[0]
    for text in ("gameplay_3d || menu_2d || bink || native_pause",
                 "gamepad_native_cutscene_skip(0,NULL)",
                 "SpiderSDL3Probe_SelectedSample(&id,&vid,&pid,&sample)",
                 "id!=sdl3_ui_instance",
                 "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)",
                 "SpiderSDL3Probe_SetupProfile(vid,pid)",
                 "gamepad_native_cutscene_skip(1,\"SDL3-CONFIRM-SKIP\")"):
        assert text in fn
    assert "spider_sdl3_context_cutscene_reset(&sdl3_cutscene_bridge)" in main
    assert "VirtualQuery((LPCVOID)(uintptr_t)owner,&mbi,sizeof(mbi))" in main
    assert "scene_context!=1u || skip_allowed!=1u || script_active!=1u" in main
    assert "script_ip==0u || *native_request!=0u" in main
    assert "*native_request=1u;" in main
    assert "if(!request)return 1;" in main
    for prohibited in ("SendInput(", "VirtualProtect(", "WriteProcessMemory(",
                       "IDirectInputDevice8_", "SDL_GetGamepads"):
        assert prohibited not in pure

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
