"""Natural South/Cross/A confirmation policy, backwards-compatible v2 storage."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_fixed_south_and_east_without_rewriting_saved_v2(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"fixed.c"
    exe=tmp_path/"fixed"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_controller_profile_ini.h"
      #include "sdl3_context_menu_bridge.h"
      #define STEP(mode) spider_sdl3_context_menu_step(\
        &state,&profile,&sample,1,0x054c,0x09cc,mode,1)
      int main(void) {
        SpiderSDL3ControllerProfile profile,loaded,old;
        SpiderSDL3ContextMenuBridge state={0};
        SpiderSDL3ControllerSample sample={0};
        SpiderSDL3Binding south=spider_sdl3_bind(1,0);
        SpiderSDL3Binding east=spider_sdl3_bind(1,1);
        SpiderSDL3Binding square=spider_sdl3_bind(1,2);
        char out[SPIDER_SDL3_PROFILE_INI_MAX+1];
        size_t n=0;
        assert(SPIDER_SDL3_ACTION_COUNT==8);
        assert(SPIDER_SDL3_CONTEXT_COUNT==5); /* v2 storage retained */
        assert(SPIDER_SDL3_SETUP_ACTION_COUNT==13);
        assert(SPIDER_SDL3_VISIBLE_ACTION_COUNT==8); /* 3D semantics only */
        assert(spider_sdl3_profile_default(&profile,0x054c,0x09cc));
        /* An old Square Confirm/Skip remains on disk but cannot take over
           the game's physical South/Cross/A Return/skip action. */
        assert(spider_sdl3_profile_assign_confirm_skip(&profile,square));
        old=profile;
        assert(spider_sdl3_ini_encode(&profile,out,sizeof(out),&n));
        assert(spider_sdl3_ini_decode(out,n,0x054c,0x09cc,&loaded));
        assert(!memcmp(&loaded,&old,sizeof(old)));
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_WAIT_NEUTRAL);
        sample.buttons=(1u<<2); /* formerly configured Square */
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_NO_EDGE);
        sample.buttons=1u; /* physical Cross */
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_CONFIRM_EDGE);
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_NO_EDGE);
        sample.buttons=0;
        assert(STEP(SPIDER_SDL3_CTX_VIDEO)==SPIDER_SDL3_CTX_WAIT_NEUTRAL);
        sample.buttons=(1u<<2);
        assert(STEP(SPIDER_SDL3_CTX_VIDEO)==SPIDER_SDL3_CTX_NO_EDGE);
        sample.buttons=1u;
        assert(STEP(SPIDER_SDL3_CTX_VIDEO)==SPIDER_SDL3_CTX_CONFIRM_EDGE);
        /* Neither Confirm nor Back may be customized through the
           visible eight-action setup, regardless of candidate button. */
        assert(!spider_sdl3_profile_assign_visible(&profile,8,south));
        assert(!spider_sdl3_profile_assign_visible(&profile,8,east));
        assert(!spider_sdl3_profile_assign_visible(&profile,9,east));
        assert(!spider_sdl3_profile_assign_visible(&profile,8,
          spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,6)));
        assert(spider_sdl3_profile_valid(&profile));
        /* Even an old v2 with Back=South must never make one South
           press trigger both Return and Escape: Back is always East. */
        assert(spider_sdl3_profile_assign_context(&profile,
          SPIDER_SDL3_CONTEXT_BACK,south));
        spider_sdl3_context_menu_reset(&state);
        sample.buttons=0;
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_WAIT_NEUTRAL);
        sample.buttons=1u;
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_CONFIRM_EDGE);
        sample.buttons=0;
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_NO_EDGE);
        sample.buttons=(1u<<1);
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_BACK_EDGE);
        sample.buttons=0;
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_NO_EDGE);
        /* A different legacy custom Back=West is ignored too. */
        assert(spider_sdl3_profile_assign_context(&profile,
          SPIDER_SDL3_CONTEXT_BACK,square));
        sample.buttons=(1u<<2);
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_NO_EDGE);
        sample.buttons=(1u<<1);
        assert(STEP(SPIDER_SDL3_CTX_MENU)==SPIDER_SDL3_CTX_BACK_EDGE);
        assert(spider_sdl3_binding_equal(profile.actions[0],south));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_wizard_eight_3d_rows_and_no_live_native_input_mutation():
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    model=(ROOT/"sdl3_controller_profile.h").read_text(encoding="utf-8")
    ini=(ROOT/"sdl3_controller_profile_ini.h").read_text(encoding="utf-8")
    core=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    assert "for(i=0;i<SPIDER_SDL3_VISIBLE_ACTION_COUNT;i++)" in ui
    assert "SPIDER_SDL3_VISIBLE_ACTION_COUNT=SPIDER_SDL3_ACTION_COUNT," in model
    assert "spider_sdl3_profile_shared_confirm_binding(p,&shared)" not in ui
    assert "spider_sdl3_profile_assign_visible(&trial,ui->action,candidate)" in ui
    assert "Cross/A Confirm, Circle/B Back fixed" in ui
    assert "Back / Cancel" not in ui
    assert "MIXED (Capture to unify)" not in ui
    assert '"[MenuBindings]' in ini and 'Version=2' in ini
    assert "SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE" in model  # old v2 codec
    assert "spider_sdl3_context_cutscene_step(" not in core
    assert "sdl3_cutscene_context_on_flip(" not in core
    assert 'begin_scancode(0x1Cu,"SDL3 physical Cross/A Confirm/Skip")' in core
    for denied in ("SendInput(", "VirtualProtect(", "WriteProcessMemory(",
                   "IDirectInputDevice8_", "CreateThread("):
        assert denied not in ui
