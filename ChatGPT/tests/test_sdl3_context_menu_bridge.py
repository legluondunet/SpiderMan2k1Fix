"""Opt-in SDL3 menu/movie input bridge tests (portable policy and Win32 wiring).

No synthetic input is used in these host tests; Wine/Windows gameplay
validation is required before removing the experimental INI guard.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT = Path(__file__).resolve().parents[2]


def test_sdl3_context_menu_bridge_native_gating_and_edges(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"sdl3_context.c"
    exe=tmp_path/"sdl3_context"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_context_menu_bridge.h"
      #define MENU SPIDER_SDL3_CTX_MENU
      #define VIDEO SPIDER_SDL3_CTX_VIDEO
      #define NO SPIDER_SDL3_CTX_NO_EDGE
      #define WAIT SPIDER_SDL3_CTX_WAIT_NEUTRAL
      #define C SPIDER_SDL3_CTX_CONFIRM_EDGE
      #define B SPIDER_SDL3_CTX_BACK_EDGE
      #define REF SPIDER_SDL3_CTX_REFUSED
      #define STEP(P,S,I,V,ID,MODE,OK) spider_sdl3_context_menu_step(\
        &state,P,S,I,V,ID,MODE,OK)
      int main(void) {
        SpiderSDL3ContextMenuBridge state={0};
        SpiderSDL3ControllerProfile sony,xbox,invalid;
        SpiderSDL3ControllerSample s={0};
        assert(spider_sdl3_profile_default(&sony,0x054c,0x09cc));
        assert(spider_sdl3_profile_default(&xbox,0x045e,0x028e));
        /* No native owner, unknown context, wrong product, or absent id. */
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,0)==REF);
        assert(STEP(&sony,&s,1,0x054c,0x09cc,0,1)==REF);
        assert(STEP(&sony,&s,1,0x054c,0x028e,MENU,1)==REF);
        assert(STEP(&sony,&s,0,0x054c,0x09cc,MENU,1)==REF);
        assert(!state.armed);
        /* Initial held confirm is quarantined until a neutral frame. */
        s.buttons=1u;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==WAIT);
        s.buttons=0;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==WAIT);
        assert(state.armed);
        s.buttons=1u;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==C);
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==NO);
        s.buttons=0;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==NO);
        s.buttons=2u;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==B);
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==NO);
        /* Simultaneous confirm/back does not generate contradictory keys. */
        s.buttons=0;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==NO);
        s.buttons=3u;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,MENU,1)==NO);
        /* 2D menu -> Bink requires a fresh neutral; a held button is not
           inherited. Legacy v2 video mapping can differ from menu. */
        assert(spider_sdl3_profile_assign_context(&sony,
          SPIDER_SDL3_CONTEXT_SKIP_VIDEO,
          spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,3)));
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==WAIT);
        s.buttons=0;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==WAIT);
        s.buttons=(1u<<3);
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==C);
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==NO);
        s.buttons=0;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==NO);
        /* New SDL instance must never reuse the previous identity's
           stored press or prior neutral acceptance. */
        s.buttons=1u;
        assert(STEP(&sony,&s,2,0x054c,0x09cc,MENU,1)==WAIT);
        assert(!state.armed);
        /* SDL3 selection / native owner mismatch disarms immediately. */
        assert(STEP(&sony,&s,2,0x054c,0x09cc,MENU,0)==REF);
        s.buttons=0;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==WAIT);
        /* A profile edit while in the UI must force a neutral rearm. */
        assert(spider_sdl3_profile_assign_context(&xbox,
          SPIDER_SDL3_CONTEXT_CONFIRM,
          spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_LT)));
        s.axis[SPIDER_SDL3_AXIS_LT]=20000;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==WAIT);
        s.axis[SPIDER_SDL3_AXIS_LT]=0;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==WAIT);
        s.axis[SPIDER_SDL3_AXIS_LT]=15000;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==C);
        s.axis[SPIDER_SDL3_AXIS_LT]=6000;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==NO);
        s.axis[SPIDER_SDL3_AXIS_LT]=4999;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==NO);
        s.axis[SPIDER_SDL3_AXIS_LT]=15000;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==C);
        s.axis[SPIDER_SDL3_AXIS_LT]=-1;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==REF);
        /* Mutating the profile's identity or invalid reserved button can
           never result in live synthetic input. */
        invalid=xbox;
        invalid.context[SPIDER_SDL3_CONTEXT_CONFIRM]=
          spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,6); /* START */
        memset(&s,0,sizeof(s));
        assert(STEP(&invalid,&s,3,0x045e,0x028e,MENU,1)==REF);
        assert(!state.armed);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
                    str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_live_context_bridge_is_explicitly_opted_in_and_single_owner():
    source=(ROOT/"gamepad_ui_profile.c").read_text()
    ini=(ROOT/"ini_validation.h").read_text()
    header=(ROOT/"sdl3_context_menu_bridge.h").read_text()
    main=(ROOT/"spiderman2k1fix.c").read_text()
    assert '"ExperimentalSDL3ContextBridge"' in ini
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3ContextBridge",0,ini)==1' in source
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3NativeOwner",0,ini)==1' in source
    assert 'SpiderSDL3Probe_NativeOwnerEnabled()' in source
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in source
    assert "id==sdl3_ui_instance" in source
    assert "spider_sdl3_context_menu_step(&sdl3_context_bridge" in source
    assert "spider_sdl3_profile_neutral(sample)" in header
    assert "SPIDER_SDL3_CONTEXT_SKIP_VIDEO" in header
    assert "SPIDER_SDL3_CONTEXT_CONFIRM" in header
    assert "SPIDER_SDL3_CONTEXT_NEXT_STILL" not in header
    assert "SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE" not in header
    assert 'begin_scancode(0x1Cu,"SDL3 profile Confirm/Skip")' in source
    assert 'begin_scancode(0x01u,"SDL3 profile Back/Cancel")' in source
    assert "sdl3_context_bridge_requested && SpiderSDL3Probe_NativeOwnerEnabled()" in source
    assert "bink_active || (menu_2d && !gameplay_3d)" in source
    assert "spider_sdl3_context_menu_reset(&sdl3_context_bridge);" in source
    assert "SpiderSDL3Probe_OnFlip(count,game_hwnd);" in main
    assert "SpiderGamepadUI_OnFlip(game_hwnd," in main
    assert main.index("SpiderSDL3Probe_OnFlip(count,game_hwnd);") < main.index("SpiderGamepadUI_OnFlip(game_hwnd,")
