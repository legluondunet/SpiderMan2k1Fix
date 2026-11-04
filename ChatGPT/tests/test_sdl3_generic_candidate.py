"""Phase-0 model-independent SDL3 candidate selector (no game ownership).

This test MUST NOT imply DS3 or Steam Deck gameplay compatibility. Only
the portable read-only candidate contract is tested. Windows/Proton
hardware and authenticating the native game's input reader come later.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_generic_candidate_by_sdl_instance_only_and_fail_closed(tmp_path):
    source = tmp_path / "generic_candidate.c"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <stdint.h>
        #include "sdl3_generic_candidate.h"

        static void frame3(SpiderSDL3GenericCandidate *p,
                           int a,int b,int c) {
          spider_sdl3_generic_begin(p);
          spider_sdl3_generic_start(p,101u,a); /* PS3 VID 054C:0268 */
          spider_sdl3_generic_start(p,202u,b); /* Steam virtual (VID/PID unknown) */
          spider_sdl3_generic_start(p,303u,c); /* another same-model DS3 */
          (void)spider_sdl3_generic_end(p);
        }

        int main(void) {
          SpiderSDL3GenericCandidate p={0};
          uint32_t devices[]={101u,202u,303u};
          uint32_t two[]={202u,303u};
          uint32_t single[]={202u};

          spider_sdl3_generic_discover(&p,devices,3u);
          assert(p.candidate_id==0u); /* never auto-capture on discovery */
          frame3(&p,0,0,0); /* neutral sample arms every new device */
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,202u,1);
          assert(spider_sdl3_generic_end(&p));
          assert(p.candidate_id==202u);
          /* Repeated held Start cannot create a fresh takeover. */
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,202u,1);
          assert(!spider_sdl3_generic_end(&p));
          assert(p.candidate_id==202u);
          /* Two new Start edges in one frame must NOT pick the last
           * device according to SDL enumeration order. */
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,303u,1);
          spider_sdl3_generic_start(&p,101u,1);
          assert(!spider_sdl3_generic_end(&p));
          assert(p.candidate_id==202u);
          /* After simultaneous claims, another HELD sample is ignored. */
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,101u,1);
          spider_sdl3_generic_start(&p,303u,1);
          assert(!spider_sdl3_generic_end(&p));
          frame3(&p,0,0,0); /* release/neutral */
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,101u,1);
          assert(spider_sdl3_generic_end(&p));
          assert(p.candidate_id==101u); /* PS3 can be CANDIDATE only */

          /* Focus loss while Start held disarms all claims. */
          spider_sdl3_generic_focus_lost(&p);
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,202u,1);
          assert(!spider_sdl3_generic_end(&p));
          assert(p.candidate_id==101u);
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,202u,0);
          assert(!spider_sdl3_generic_end(&p));
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,202u,1);
          assert(spider_sdl3_generic_end(&p));
          assert(p.candidate_id==202u);

          /* Unknown/unopened instance can never claim. */
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,555u,0);
          spider_sdl3_generic_start(&p,555u,1);
          assert(!spider_sdl3_generic_end(&p));
          assert(p.candidate_id==202u);

          /* Selected controller unplugged: revoke on rescan, no auto
           * transfer to remaining same-model or Steam devices. */
          spider_sdl3_generic_discover(&p,(uint32_t[]){101u,303u},2u);
          assert(p.candidate_id==0u);
          spider_sdl3_generic_discover(&p,two,2u);
          assert(p.candidate_id==0u);
          spider_sdl3_generic_discover(&p,single,1u);
          assert(p.candidate_id==0u);
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,202u,1); /* still HELD */
          assert(!spider_sdl3_generic_end(&p));
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,202u,0);
          assert(!spider_sdl3_generic_end(&p));
          spider_sdl3_generic_begin(&p);
          spider_sdl3_generic_start(&p,202u,1);
          assert(spider_sdl3_generic_end(&p));
          assert(p.candidate_id==202u);
          /* Do not leak tentative ownership to the original game. */
          assert(p.claim_id==0u && p.frame_open==0u);
          return 0;
        }
    """), encoding="utf-8")
    exe = tmp_path / "generic_candidate"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT), str(source), "-o", str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)


def test_policy_does_not_modify_runtime_input_or_assume_native_ds3_layout():
    h=(ROOT/"sdl3_generic_candidate.h").read_text(encoding="utf-8")
    doc=(ROOT/"docs/references/SDL3_GENERIC_BRIDGE_RESEARCH.md").read_text(
        encoding="utf-8")
    original=(ROOT/"native_joystick_reconnect_policy.h").read_text(
        encoding="utf-8")
    assert "spider_sdl3_selection_start_edge(" in h
    assert "p->candidate_id=p->claim_id;" in h
    assert "p->claims" in h
    assert "SpiderSDL3Selection selection;" in h
    assert "DirectInput" in doc
    assert "NEVER" in doc or "Never" in doc
    assert "spider_reconnect_kind" in original
    assert "0x0268u" not in original
    for unsafe in ("windows.h","dinput.h","VirtualProtect","WriteProcessMemory",
                   "SendInput","IDirectInputDevice8","SDL_OpenGamepad",
                   "SpiderNativeReconnect_"):
        assert unsafe not in h
    # Phase 0 policy is not wired to runtime; preserve the proven PR #80.
    assert '#include "sdl3_generic_candidate.h"' not in (
        ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
