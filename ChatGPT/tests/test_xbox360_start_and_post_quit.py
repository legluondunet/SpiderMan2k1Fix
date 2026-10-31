"""Xbox Start is inert and native request-7 cannot replay A into New Game."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
UI=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
POLICY=(ROOT/"xbox360_menu_policy.h").read_text(encoding="utf-8")
QUIT=(ROOT/"ps1_transition_edge_ownership.h").read_text(encoding="utf-8")
COMMON=(ROOT/"native_menu_return_gate.h").read_text(encoding="utf-8")
SETTLE=(ROOT/"ps1_post_quit_guard.h").read_text(encoding="utf-8")


def test_xbox_start_is_not_a_synthetic_menu_or_3d_action():
    # The original game input is untouched; this verifies only our adapter.
    action=POLICY.split("static inline unsigned spider_xbox360_menu_edges(",1)[1]
    assert "SPIDER_XBOX360_START_DI_BUTTON)))" not in action
    assert "SPIDER_XBOX360_A_DI_BUTTON" in action
    branch=UI.split("if(detected_kind==SPIDER_RECONNECT_XBOX360) {",1)[1].split(
        'pressed=(state.rgbButtons[1]',1)[0]
    assert "int pause_requested=(fresh & (1u<<SPIDER_XBOX360_BACK_DI_BUTTON))!=0;" in branch
    assert "unsigned pause_button_index=SPIDER_XBOX360_BACK_DI_BUTTON;" in branch
    assert 'GAMEPAD-XBOX360: 3D MENU Back -> Escape' in branch
    assert 'Back/Start -> Escape' not in branch
    assert "Start does not synthesize any key" in branch


def test_exit_guard_is_automatically_enabled_for_active_xbox_preview():
    assert "static SpiderNativeMenuReturnGate xbox_transition_ownership={0};" in UI
    assert "spider_native_menu_return_gate_flip(" in UI
    assert "spider_native_menu_return_gate_flip(" in COMMON
    assert "static SpiderPS1PostQuitGuard xbox_post_quit_guard={0};" in UI
    assert "(xbox360_preview_active && detected_kind==SPIDER_RECONNECT_XBOX360)" in UI
    assert "native_request=(unsigned)*(const volatile DWORD *)(uintptr_t)0x0060CFA4u;" in UI
    assert "spider_ps1_transition_edge_ownership_flip(" in UI
    assert "&xbox_transition_ownership," in UI
    assert "xbox360_preview_active || sdl3_live_xbox," in UI
    assert "SpiderNativeReconnect_CanonicalXboxMapping()" in UI
    assert "held & (1u<<SPIDER_XBOX360_A_DI_BUTTON)" in UI
    assert "spider_ps1_post_quit_guard_flip(" in UI
    assert "&xbox_post_quit_guard,xbox360_preview_active || sdl3_live_xbox," in UI
    assert "POST-QUIT-GUARD suppressed inherited/too-early synthetic A->Return" in UI
    assert "ui_allowed && action==SPIDER_XBOX360_ENTER &&" in UI
    assert "(inherited_a || post_quit_block)" in UI
    assert "POST-QUIT RETURN native_request=7" in UI
    assert "xbox_previous_mask=held;" in UI


def test_c99_guard_blocks_stale_A_until_release_then_allows_repress(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"test_quit.c"
    exe=tmp_path/"test_quit"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <stdint.h>
      #include "xbox360_menu_policy.h"
      #include "ps1_transition_edge_ownership.h"
      #include "ps1_post_quit_guard.h"
      int main(void) {
        SpiderPS1TransitionEdgeOwnership o={0};
        SpiderPS1PostQuitGuard g={0};
        unsigned inherited,block,i;
        const uint32_t a=1u<<SPIDER_XBOX360_A_DI_BUTTON;
        /* Xbox Start is NEVER mapped by our synthetic UI bridge. */
        assert(spider_xbox360_menu_edges(1u<<7,0)==0);
        assert(spider_xbox360_menu_edges(a,0)==SPIDER_XBOX360_ENTER);
        assert(spider_xbox360_menu_edges(1u<<6,0)==SPIDER_XBOX360_BACK);
        /* Native 3D exit request occurs after selecting Quit in pause. */
        assert(!spider_ps1_transition_edge_ownership_flip(&o,1,0,1,0,0,a));
        assert(!spider_ps1_post_quit_guard_flip(&g,1,0,1,0,0));
        inherited=spider_ps1_transition_edge_ownership_flip(&o,1,1,0,0,7,a);
        block=spider_ps1_post_quit_guard_flip(&g,1,1,0,0,7);
        assert(inherited==a && block);
        for(i=1;i<8;i++) {
          inherited=spider_ps1_transition_edge_ownership_flip(&o,1,1,0,0,7,a);
          block=spider_ps1_post_quit_guard_flip(&g,1,1,0,0,7);
          assert(inherited==a && block);
        }
        assert(spider_ps1_transition_edge_ownership_flip(&o,1,1,0,0,7,a)==a);
        assert(!spider_ps1_post_quit_guard_flip(&g,1,1,0,0,7));
        /* Physical A release ends ownership; subsequent press is fresh. */
        assert(!spider_ps1_transition_edge_ownership_flip(&o,1,1,0,0,7,0));
        assert(!spider_ps1_transition_edge_ownership_flip(&o,1,1,0,0,7,a));
        assert(spider_xbox360_menu_edges(a,0)==SPIDER_XBOX360_ENTER);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
