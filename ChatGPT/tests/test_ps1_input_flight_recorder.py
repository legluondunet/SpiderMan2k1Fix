"""Buffered PS1 button flight recorder: opt-in, no per-poll logging."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
PAD=(ROOT/"gamepad_ui_profile.c").read_text()
HEADER=(ROOT/"ps1_input_flight_recorder.h").read_text()
INI=(ROOT/"ini_validation.h").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_opt_in_preserves_existing_verified_guard_and_inputs():
    assert '"ControllerPS1InputFlightRecorder"' in INI
    assert 'GetPrivateProfileIntA("Input","ControllerPS1InputFlightRecorder",0,ini)==1' in PAD
    assert "ps1_preview_active && ps1_input_flight_recorder_requested &&" in PAD
    assert "(ps1_post_quit_guard_requested || ps1_transition_edge_ownership_requested)" in PAD
    assert "ps1_input_flight_recorder_requested &&" in PAD
    assert "ControllerPS1InputFlightRecorder=1" not in CFG
    assert "ps1_history_record(SPIDER_PS1_HISTORY_READ_OK" in PAD
    assert "ps1_history_record(SPIDER_PS1_HISTORY_FOCUS_LOST" in PAD
    assert "ps1_history_record(SPIDER_PS1_HISTORY_READ_FAILED" in PAD
    assert "ps1_history_record(SPIDER_PS1_HISTORY_DEVICE_MISSING" in PAD
    assert "ps1_history_dump();" in PAD
    assert PAD.index("ps1_history_dump();") > PAD.index(
        'POST-QUIT-GUARD suppressed synthetic Return')
    assert "begin_scancode(0x1Cu,\"Return\");" in PAD
    assert "SPIDER_PS1_HISTORY_CAPACITY 96u" in HEADER
    for forbidden in ("SendInput(", "Sleep(", "VirtualProtect(",
                      "WriteProcessMemory(", "fopen(", "fprintf("):
        assert forbidden not in HEADER


def test_pure_c99_history_ring_wraps_without_losing_order(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"history.c"
    binary=tmp_path/"history"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <stdint.h>
      #include "ps1_input_flight_recorder.h"
      int main(void) {
        SpiderPS1InputHistory h={0};
        SpiderPS1HistoryEntry e={0};
        const SpiderPS1HistoryEntry *a;
        unsigned i;
        assert(spider_ps1_history_count(&h)==0u);
        assert(spider_ps1_history_at(&h,0)==0);
        for(i=1;i<=100u;i++) {
          e.sequence=i;
          e.tick_ms=i*30u;
          e.previous_confirm=(i%3u)==0u;
          e.cross=(uint8_t)(i&1u);
          spider_ps1_history_push(&h,&e);
        }
        assert(spider_ps1_history_count(&h)==96u);
        assert(h.total==100u);
        assert(spider_ps1_history_at(&h,96)==0);
        for(i=0;i<96u;i++) {
          a=spider_ps1_history_at(&h,i);
          assert(a!=0);
          assert(a->sequence==i+5u);
          assert(a->tick_ms==(i+5u)*30u);
          assert(a->cross==((i+5u)&1u));
        }
        assert(spider_ps1_history_count(0)==0u);
        assert(spider_ps1_history_at(0,0)==0);
        spider_ps1_history_push(0,&e);
        spider_ps1_history_push(&h,0);
        assert(h.total==100u);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
