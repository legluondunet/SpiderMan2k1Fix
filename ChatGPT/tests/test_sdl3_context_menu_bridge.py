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
        /* 2D menu -> Bink requires fresh neutral. Legacy v2 SkipVideo
           must NOT change the natural South/Cross/A confirmation. */
        assert(spider_sdl3_profile_assign_context(&sony,
          SPIDER_SDL3_CONTEXT_SKIP_VIDEO,
          spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,3)));
        assert(spider_sdl3_profile_assign_context(&sony,
          SPIDER_SDL3_CONTEXT_BACK,
          spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,3)));
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==WAIT);
        s.buttons=0;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==WAIT);
        s.buttons=(1u<<3); /* old saved Back/Skip: ignored */
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==NO);
        s.buttons=(1u<<1); /* fixed East/Circle/B */
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==B);
        s.buttons=0;
        assert(STEP(&sony,&s,1,0x054c,0x09cc,VIDEO,1)==NO);
        s.buttons=1u;
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
        /* Even migrated profiles with LT Confirm must not produce
           Return from LT: South/A stays the only confirmation. */
        assert(spider_sdl3_profile_assign_context(&xbox,
          SPIDER_SDL3_CONTEXT_CONFIRM,
          spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_LT)));
        s.axis[SPIDER_SDL3_AXIS_LT]=20000;
        /* Stored v2 Confirm changes no longer alter live binding or
           rearm the already neutral-selected South button. */
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==NO);
        s.axis[SPIDER_SDL3_AXIS_LT]=0;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==NO);
        s.axis[SPIDER_SDL3_AXIS_LT]=15000;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==NO);
        s.axis[SPIDER_SDL3_AXIS_LT]=0;
        s.buttons=1u;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==C);
        s.buttons=0;
        assert(STEP(&xbox,&s,3,0x045e,0x028e,MENU,1)==NO);
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


def test_legacy_context_bridge_is_not_a_second_input_owner():
    """The original SDL3 context policy is portable; live control comes
    exclusively from the universal SDL3 profile/menu adapter."""
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    universal=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    core=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    assert "spider_sdl3_context_menu_step(" not in ui
    assert "begin_scancode(" not in ui
    assert "SpiderSDL3Probe_SelectedMappedSample(" in universal
    assert "SPIDER_UNIVERSAL_CTX_MENU" in universal
    assert "SPIDER_UNIVERSAL_CTX_BINK" in universal or "SPIDER_UNIVERSAL_CTX_MOVIE" in universal
    assert "spider_sdl3_universal_neutral(&sample)" in universal
    assert "SpiderSDL3Probe_OnFlip(count,game_hwnd);" in core
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in core
    assert "SDL3-UNIVERSAL: START selected generic" in probe
