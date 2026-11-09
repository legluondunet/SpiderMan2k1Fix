"""Retail Spider-Man Save Game: renderer remains 3D while menu shell owns input.

Evidence: user's scrn0112.png + F9 tuple in 20261108-231804 log.
This tests a conservative *candidate*, NOT a source-proven unique Save UI.
Hardware (South on save-slot and YES, Pause, Bink, gameplay) still required.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("cc") or shutil.which("gcc")),
                    reason="C99 compiler unavailable")
def test_native_save_shell_candidate_requires_full_tuple_and_two_frames(tmp_path):
    src=tmp_path/"native_shell.c"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_universal_policy.h"
      #define MATCH(m,g,b,p,r,t,i,pc,mc,d,tr) \
        spider_sdl3_native_shell_menu_candidate( \
          m,g,b,p,r,t,i,pc,mc,d,tr)
      int main(void) {
        unsigned frames=0;
        /* Exact seven scalar values seen with real SAVE GAME screenshot. */
        assert(MATCH(0,1,0,0,3u,0x14u,1u,0u,1u,0u,0u));
        assert(MATCH(0,1,0,0,3u,0x14u,1u,0u,0xdeadbeefu,0u,0u));
        /* Unrelated gameplay must never route from menu pointer alone. */
        assert(!MATCH(0,1,0,0,0u,0x14u,1u,0u,1u,0u,0u));
        assert(!MATCH(0,1,0,0,2u,0x14u,1u,0u,1u,0u,0u));
        assert(!MATCH(0,1,0,0,3u,0x15u,1u,0u,1u,0u,0u));
        assert(!MATCH(0,1,0,0,3u,0x14u,0u,0u,1u,0u,0u));
        assert(!MATCH(0,1,0,0,3u,0x14u,1u,1u,1u,0u,0u));
        assert(!MATCH(0,1,0,0,3u,0x14u,1u,0u,0u,0u,0u));
        assert(!MATCH(0,1,0,0,3u,0x14u,1u,0u,1u,1u,0u));
        assert(!MATCH(0,1,0,0,3u,0x14u,1u,0u,1u,0u,1u));
        /* Native renderer/menu has already routed; no special override. */
        assert(!MATCH(1,1,0,0,3u,0x14u,1u,0u,1u,0u,0u));
        assert(!MATCH(0,0,0,0,3u,0x14u,1u,0u,1u,0u,0u));
        assert(!MATCH(0,1,1,0,3u,0x14u,1u,0u,1u,0u,0u));
        assert(!MATCH(0,1,0,1,3u,0x14u,1u,0u,1u,0u,0u));
        /* Transient one-frame candidate cannot redirect real gameplay. */
        assert(!spider_sdl3_native_shell_menu_stable(&frames,1));
        assert(frames==1);
        assert(spider_sdl3_native_shell_menu_stable(&frames,1));
        assert(frames==2);
        assert(spider_sdl3_native_shell_menu_stable(&frames,1));
        assert(frames==2); /* no overflow */
        assert(!spider_sdl3_native_shell_menu_stable(&frames,0));
        assert(frames==0);
        assert(!spider_sdl3_native_shell_menu_stable(&frames,1));
        assert(spider_sdl3_native_shell_menu_stable(&frames,1));
        assert(!spider_sdl3_native_shell_menu_stable(NULL,1));
        /* Original Bink / Pause routing still wins unconditionally. */
        assert(spider_sdl3_universal_route_context(0,1,1,0)
               ==SPIDER_UNIVERSAL_CTX_BINK);
        assert(spider_sdl3_universal_route_context(0,1,0,1)
               ==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE);
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"native_shell"
    subprocess.run([shutil.which("cc") or shutil.which("gcc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)


def test_shell_candidate_is_read_only_and_does_not_hook_input():
    src=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    policy=(ROOT/"sdl3_universal_policy.h").read_text(encoding="utf-8")
    fn=src.split("static int native_shell_menu_context(",1)[1].split(
        "static void pulse(",1)[0]
    assert "gameover_probe_read_u32(" in fn
    for addr in ("0x0060CFA4u","0x0054A510u","0x006B78F0u",
                 "0x00682950u","0x00682948u",
                 "0x0068293Cu","0x0054D38Cu"):
        assert addr in fn
    assert "spider_sdl3_native_shell_menu_candidate(" in fn
    assert "spider_sdl3_native_shell_menu_stable(" in fn
    assert "if(routed!=native_shell_routed)" in fn
    assert "send_scan(" not in fn
    assert "InterlockedCompareExchangePointer(" not in fn
    assert "VirtualProtect(" not in fn
    onflip=src.split("void SpiderSDL3Universal_OnFlip(",1)[1]
    assert "native_shell_menu=native_shell_menu_context(" in onflip
    assert onflip.index("if(focus_release.frames_remaining) {") < (
        onflip.index("native_shell_menu=native_shell_menu_context("))
    assert onflip.index("native_shell_menu=native_shell_menu_context(") < (
        onflip.index("if(focus_dir.phase &&\n     ("))
    assert "!=SPIDER_UNIVERSAL_CTX_3D || native_shell_menu" in onflip
    assert "if(native_shell_menu && context==SPIDER_UNIVERSAL_CTX_3D)" in onflip
    assert onflip.index("if(native_shell_menu && context==SPIDER_UNIVERSAL_CTX_3D)") < (
        onflip.index("gameover_probe_snapshot("))
    assert "context=SPIDER_UNIVERSAL_CTX_MENU;" in onflip
    assert "if(context!=previous_context)" in onflip
    assert "if(context!=SPIDER_UNIVERSAL_CTX_3D)" in onflip
    assert "menu_confirm_hold_flips=1u;" in onflip
    assert "menu_arm=0;menu_previous=0;" in onflip
    assert "native_shell_consecutive=0;" in src
    assert "native_shell_routed=0;" in src
    assert "request==3u && task==0x14u && input_active==1u" in policy
    assert "menu_ctx!=0u && !dispatcher && !transition" in policy
    assert "SDL3-NATIVE-SHELL: %s 3D-renderer shell-like menu route" in src
    assert 'pulse(0x1cu,"native GAME OVER Return confirmation")' in src
