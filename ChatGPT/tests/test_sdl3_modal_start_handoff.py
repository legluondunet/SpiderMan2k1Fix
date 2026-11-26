"""Modal-only Start handoff defers the original DI owner until game Flip.

Regression: DS4 and Xbox connected; Xbox selected by Start in gameplay,
panel initially Xbox; Start on DS4 should select DS4 within modal even
though normal Flip-based gamepad selection is suspended; manual R1 remains.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_modal_selection_neutral_start_and_manual_handoff_c99(tmp_path):
    src=tmp_path/"modal_start.c"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_gamepad_selection.h"
        int main(void) {
          SpiderSDL3Selection modal={0};
          uint32_t ids[]={1u,2u}; /* DS4=1, Xbox=2 */
          spider_sdl3_selection_sync(&modal,ids,2);
          modal.active=2u; /* Xbox is displayed on initial open */
          assert(!spider_sdl3_selection_start(&modal,1u,1)); /* held */
          assert(modal.active==2u);
          assert(!spider_sdl3_selection_start(&modal,1u,0)); /* release arms */
          assert(spider_sdl3_selection_start(&modal,1u,1)); /* fresh DS4 */
          assert(modal.active==1u);
          assert(!spider_sdl3_selection_start(&modal,1u,1)); /* held */
          modal.active=2u; /* manual R1 selected Xbox profile */
          assert(!spider_sdl3_selection_start(&modal,1u,0));
          assert(spider_sdl3_selection_start(&modal,1u,1));
          assert(modal.active==1u);
          spider_sdl3_selection_focus_lost(&modal);
          assert(!spider_sdl3_selection_start(&modal,2u,1));
          assert(!spider_sdl3_selection_start(&modal,2u,0));
          assert(spider_sdl3_selection_start(&modal,2u,1));
          assert(modal.active==2u);
          ids[0]=0; /* DS4 disconnected */
          spider_sdl3_selection_sync(&modal,&ids[1],1);
          assert(modal.active==2u);
          return 0;
        }
    """),encoding="utf-8")
    binary=tmp_path/"modal_start"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(src),"-o",str(binary)],check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_modal_start_same_displayed_and_simultaneous_press_policy_c99(tmp_path):
    """L1/R1 browse then Start on current pad must request it for the game.

    Simultaneous fresh Start presses must be consumed yet not change the
    selected modal profile or schedule an unverified game handoff.
    """
    src=tmp_path/"modal_edges.c"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_gamepad_selection.h"
        int main(void) {
          SpiderSDL3Selection s={0};
          uint32_t ids[]={1u,2u};
          unsigned claims=0;
          uint32_t previous;
          spider_sdl3_selection_sync(&s,ids,2);
          s.active=2u; /* L1/R1 browsed Xbox; game still owns DS4 */
          assert(!spider_sdl3_selection_start_edge(&s,2u,1)); /* held */
          assert(!spider_sdl3_selection_start_edge(&s,2u,0)); /* arm */
          assert(spider_sdl3_selection_start_edge(&s,2u,1));  /* same row */
          assert(s.active==2u);
          assert(!spider_sdl3_selection_start_edge(&s,2u,1));
          /* Gameplay selection API still returns 0 when owner unchanged. */
          assert(!spider_sdl3_selection_start(&s,2u,0));
          assert(!spider_sdl3_selection_start(&s,2u,1));
          assert(s.active==2u);
          spider_sdl3_selection_focus_lost(&s);
          assert(!spider_sdl3_selection_start_edge(&s,1u,0));
          assert(!spider_sdl3_selection_start_edge(&s,2u,0));
          previous=s.active;
          claims+=(unsigned)spider_sdl3_selection_start_edge(&s,1u,1);
          claims+=(unsigned)spider_sdl3_selection_start_edge(&s,2u,1);
          assert(claims==2u);
          if(claims!=1u)s.active=previous;
          assert(s.active==2u); /* simultaneous requests refused */
          assert(!spider_sdl3_selection_start_edge(&s,1u,1));
          assert(!spider_sdl3_selection_start_edge(&s,2u,1));
          assert(!spider_sdl3_selection_start_edge(&s,1u,0));
          assert(spider_sdl3_selection_start_edge(&s,1u,1));
          assert(s.active==1u);
          return 0;
        }
    """),encoding="utf-8")
    binary=tmp_path/"modal_edges"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(src),"-o",str(binary)],check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_rescan_arms_new_reconnected_pad_from_verified_neutral_sample(tmp_path):
    """After hotplug/Alt+Tab, don't lose Start to an unarmed first timer."""
    source=tmp_path/"reconnect_neutral.c"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_gamepad_selection.h"
        int main(void) {
          SpiderSDL3Selection modal={0};
          uint32_t two[]={2u,3u}; /* Xbox=2, freshly reconnected DS4=3 */
          spider_sdl3_selection_sync(&modal,two,2);
          modal.active=3u; /* user browsed L1/R1 to DS4 */
          /* Scan saw Start released: safely arm BEFORE the first timer. */
          assert(!spider_sdl3_selection_start_edge(&modal,3u,0));
          assert(modal.slots[1].start_armed==1u);
          assert(spider_sdl3_selection_start_edge(&modal,3u,1));
          assert(modal.active==3u); /* fresh Start on displayed pad */
          /* Focus loss disarms held Start, not the current selection. */
          spider_sdl3_selection_focus_lost(&modal);
          assert(!spider_sdl3_selection_start_edge(&modal,3u,1));
          assert(!spider_sdl3_selection_start_edge(&modal,3u,0));
          assert(spider_sdl3_selection_start_edge(&modal,3u,1));
          /* A newly discovered pad with Start already HELD must fail
           * closed until a real released sample has been observed. */
          uint32_t ids[]={2u,4u};
          spider_sdl3_selection_sync(&modal,ids,2);
          assert(!spider_sdl3_selection_start_edge(&modal,4u,1));
          assert(!spider_sdl3_selection_start_edge(&modal,4u,0));
          assert(spider_sdl3_selection_start_edge(&modal,4u,1));
          return 0;
        }
    """),encoding="utf-8")
    binary=tmp_path/"reconnect_neutral"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(source),"-o",str(binary)],check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)


