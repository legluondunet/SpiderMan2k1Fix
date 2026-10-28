"""A/B experiment: explicit input ownership at native Quit->main menu boundary."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
PAD=(ROOT/"gamepad_ui_profile.c").read_text()
HEADER=(ROOT/"ps1_transition_edge_ownership.h").read_text()
INI=(ROOT/"ini_validation.h").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_opt_in_owner_preserves_legacy_eight_flip_guard():
    assert '"ControllerPS1TransitionEdgeOwnership"' in INI
    assert 'GetPrivateProfileIntA("Input","ControllerPS1TransitionEdgeOwnership",0,ini)==1' in PAD
    assert "ControllerPS1TransitionEdgeOwnership=1" not in CFG
    assert 'ps1_transition_edge_ownership.h' in PAD
    assert "ps1_transition_edge_ownership_flip(" in PAD
    assert 'TRANSITION-OWNERSHIP consumed inherited Cross/Options' in PAD
    assert "ControllerPS1PostQuitGuard" in PAD
    assert "ps1_post_quit_guard_flip(" in PAD
    assert PAD.index("(confirm_edges & ownership_block)) {") < PAD.index("post_quit_block) {")
    assert "confirm_edges=(pressed & (unsigned)confirm) & ~(unsigned)previous_buttons;" in PAD
    assert "(confirm_edges & ~ownership_block)" in PAD
    assert "!(previous_buttons & (unsigned)confirm)" not in PAD
    assert "previous_buttons=(int)pressed;" in PAD
    assert 'begin_scancode(0x1Cu,"Return");' in PAD
    for forbidden in ("SendInput(", "Sleep(", "VirtualProtect(", "WriteProcessMemory(",
                      "GetTickCount(", "fprintf("):
        assert forbidden not in HEADER
    assert "native_request==7u" in HEADER


def test_native_quit_owner_consumes_only_inherited_press_until_release(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"edge_ownership.c"
    binary=tmp_path/"edge_ownership"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "ps1_transition_edge_ownership.h"
      #define APPLY(enabled,menu,game,movie,req,buttons) \
        spider_ps1_transition_edge_ownership_flip(&o,enabled,menu,game,movie,req,buttons)
      int main(void) {
        SpiderPS1TransitionEdgeOwnership o={0};
        /* New Game at startup: no request-7 ownership. */
        assert(!APPLY(1,1,0,0,0,1));
        /* Native 3D pause, Cross already used for original Quit/Yes. */
        assert(!APPLY(1,0,1,0,0,0));
        assert(!APPLY(1,0,1,0,0,1));
        assert(APPLY(1,1,0,0,7,1));
        /* Held Cross cannot replay, no fixed timeout. */
        for(unsigned i=0;i<128;i++)assert(APPLY(1,1,0,0,7,1));
        /* Release immediately re-arms. */
        assert(!APPLY(1,1,0,0,7,0));
        assert(!APPLY(1,1,0,0,7,1));
        /* The return without a held confirm must not swallow a later click. */
        assert(!APPLY(1,0,1,0,0,0));
        assert(!APPLY(1,1,0,0,7,0));
        assert(!APPLY(1,1,0,0,7,1));
        /* Different native transition: button is owned by new menu. */
        assert(!APPLY(1,0,1,0,0,0));
        assert(!APPLY(1,1,0,0,8,1));
        /* OPTIONS has independently owned release semantics. */
        assert(!APPLY(1,0,1,0,0,0));
        assert(APPLY(1,1,0,0,7,2));
        assert(!APPLY(1,1,0,0,7,1));
        assert(!APPLY(1,1,0,0,7,2));
        /* A held Cross is NOT allowed to mask a newly pressed OPTIONS.
         * The return value is the mask of old-owned bits, not a boolean. */
        assert(!APPLY(1,0,1,0,0,0));
        assert(APPLY(1,1,0,0,7,1)==1u);
        assert(APPLY(1,1,0,0,7,3)==1u);  /* new OPTIONS is unowned */
        assert(!APPLY(1,1,0,0,7,2));     /* Cross released, OPTIONS held */
        assert(!APPLY(1,1,0,0,7,3));     /* Cross is now a fresh press */
        /* Symmetrical test: inherited OPTIONS, new Cross. */
        assert(!APPLY(1,0,1,0,0,0));
        assert(APPLY(1,1,0,0,7,2)==2u);
        assert(APPLY(1,1,0,0,7,3)==2u);  /* new Cross is unowned */
        assert(!APPLY(1,1,0,0,7,1));     /* OPTIONS released */
        assert(!APPLY(1,1,0,0,7,3));     /* new OPTIONS */
        /* Both initially held: independently release one then the other. */
        assert(!APPLY(1,0,1,0,0,0));
        assert(APPLY(1,1,0,0,7,3)==3u);
        assert(APPLY(1,1,0,0,7,1)==1u);
        assert(!APPLY(1,1,0,0,7,2));
        /* Reset option does not leave stale protection armed. */
        assert(!APPLY(1,0,1,0,0,0));
        assert(APPLY(1,1,0,0,7,1));
        assert(!APPLY(0,1,0,0,7,1));
        assert(!APPLY(1,1,0,0,7,1));
        /* Native ownership cannot arm while Bink is active. */
        assert(!APPLY(1,0,1,1,0,1));
        assert(!APPLY(1,1,0,1,7,1));
        assert(!APPLY(1,1,0,0,7,1));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
