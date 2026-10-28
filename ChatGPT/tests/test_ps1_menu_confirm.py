"""Host-compiled PS1 confirmation policy: Cross only, preserve legacy and owner."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
PAD=(ROOT/"gamepad_ui_profile.c").read_text()
HEADER=(ROOT/"ps1_menu_confirm.h").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()

def test_runtime_uses_pure_policy_and_leaves_3d_native_inputs_unchanged():
    assert '#include "ps1_menu_confirm.h"' in PAD
    assert "ps1_preview_active || late_ds4_menu_only,(unsigned)confirm)" in PAD
    assert "spider_ps1_menu_confirm_edges(pressed,(unsigned)previous_buttons,effective_confirm)" in PAD
    assert "spider_ps1_menu_confirm_unowned(confirm_edges,ownership_block)" in PAD
    assert 'begin_scancode(0x1Cu,"Return");' in PAD
    assert 'ps1_preview_active && ps1_options_skip_requested' in PAD
    assert "ControllerPS1OptionsSkip=1" not in CFG
    assert "PS1 preview: Cross is the ONLY synthetic menu confirmation" in PAD
    for forbidden in ("SendInput(", "Sleep(", "VirtualProtect(", "WriteProcessMemory(",
                      "GetTickCount(", "fprintf("):
        assert forbidden not in HEADER

def test_compiled_cross_only_and_legacy_modes(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"menu_confirm.c"
    binary=tmp_path/"menu_confirm"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "ps1_menu_confirm.h"
        #define MASK(p,legacy) spider_ps1_menu_confirm_mask(p,legacy)
        #define EDGE(cur,prev,allowed) spider_ps1_menu_confirm_edges(cur,prev,allowed)
        #define READY(edges,owner) spider_ps1_menu_confirm_unowned(edges,owner)
        int main(void) {
          unsigned mask;
          /* Legacy settings remain backwards compatible outside preview. */
          assert(MASK(0,1)==1u);
          assert(MASK(0,2)==2u);
          assert(MASK(0,3)==3u);
          assert(MASK(0,0)==0u);
          /* PS1 always uses Cross, even if the old INI asks for Options. */
          assert(MASK(1,0)==1u);
          assert(MASK(1,1)==1u);
          assert(MASK(1,2)==1u);
          assert(MASK(1,3)==1u);
          mask=MASK(1,3);
          assert(!EDGE(2,0,mask)); /* only OPTIONS pressed */
          assert(!EDGE(2,2,mask)); /* held OPTIONS */
          assert(EDGE(1,0,mask)==1u); /* Cross pressed */
          assert(!EDGE(1,1,mask)); /* held Cross */
          assert(!EDGE(3,1,mask)); /* OPTIONS joins held Cross */
          assert(EDGE(3,2,mask)==1u); /* Cross joins held OPTIONS */
          assert(EDGE(1,0,mask)==1u); /* release and re-press Cross */
          assert(EDGE(3,0,MASK(0,3))==3u); /* legacy Both */
          assert(EDGE(3,1,MASK(0,3))==2u); /* legacy OPTIONS new */
          assert(EDGE(3,2,MASK(0,3))==1u); /* legacy Cross new */
          /* Transition ownership blocks old-session Cross, not new input. */
          assert(!READY(1,1));
          assert(READY(2,1)==2u);
          assert(READY(3,1)==2u);
          assert(READY(1,0)==1u);
          assert(!READY(0,0));
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
