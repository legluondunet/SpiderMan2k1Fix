"""PS1-only, opt-in guard against synthetic Enter during native request-7 return."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
PAD=(ROOT/"gamepad_ui_profile.c").read_text()
INI=(ROOT/"ini_validation.h").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()
HEADER=(ROOT/"ps1_post_quit_guard.h").read_text()


def test_opt_in_and_only_synthetic_menu_confirm_is_suppressed():
    assert '"ControllerPS1PostQuitGuard"' in INI
    assert 'GetPrivateProfileIntA("Input","ControllerPS1PostQuitGuard",0,ini)==1' in PAD
    assert 'if(ps1_post_quit_guard_requested &&' in PAD
    assert '(ps1_preview_active || sdl3_live_sony))' in PAD
    assert "ControllerPS1PostQuitGuard=1" not in CFG
    assert '#include "ps1_post_quit_guard.h"' in PAD
    assert '0x0060CFA4u' in PAD
    assert "post_quit_block=spider_ps1_post_quit_guard_flip(" in PAD
    assert 'post_quit_block) {' in PAD
    assert 'POST-QUIT-GUARD suppressed synthetic Return' in PAD
    assert "previous_buttons=(int)pressed;" in PAD
    assert 'begin_scancode(0x1Cu,"Return");' in PAD
    assert "SPIDER_PS1_POST_QUIT_SETTLE_FLIPS 8u" in HEADER
    assert "if(native_request==7u)" in HEADER
    assert "if(!enabled)" in HEADER
    assert "SendInput(" not in HEADER
    assert "Sleep(" not in HEADER
    assert "VirtualProtect(" not in HEADER
    assert "WriteProcessMemory(" not in HEADER


def test_compiled_guard_only_blocks_initial_verified_return(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"post_quit_guard.c"
    binary=tmp_path/"post_quit_guard"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "ps1_post_quit_guard.h"
        int main(void) {
          SpiderPS1PostQuitGuard guard={0};
          unsigned frame;
          /* Normal main menu or Bink must not be blocked. */
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,0));
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,1,0,1,0));
          /* Simulated playable 3D followed by verified native Quit (7). */
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,0,1,0,0));
          for(frame=0;frame<SPIDER_PS1_POST_QUIT_SETTLE_FLIPS;frame++)
            assert(spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,7));
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,7));
          /* No replay on subsequent menu frames even if request 7 persists. */
          for(frame=0;frame<32;frame++)
            assert(!spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,7));
          /* Only request 7 can arm it. */
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,0,1,0,0));
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,8));
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,7));
          /* Disabling experiment cancels any queued suppression. */
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,0,1,0,0));
          assert(spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,7));
          assert(!spider_ps1_post_quit_guard_flip(&guard,0,1,0,0,7));
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,7));
          /* A new native quit after returning to 3D is protected again. */
          assert(!spider_ps1_post_quit_guard_flip(&guard,1,0,1,0,0));
          assert(spider_ps1_post_quit_guard_flip(&guard,1,1,0,0,7));
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
