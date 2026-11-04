"""Portable, read-only decode of the game-owned native action state records."""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest
ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_native_game_record_snapshot_and_bounded_delta(tmp_path):
    source=tmp_path/"snapshot.c"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_native_latch_observer.h"
      int main(void) {
        uint8_t records[20][16]={{0}};
        SpiderNativeLatchSnapshot s={0};
        SpiderNativeLatchObserver o={0};
        records[3][0]=1; records[16][0]=1;
        records[3][1]=1; records[16][2]=1;
        spider_native_latch_decode(records,&s);
        assert(s.held_slots==((1u<<3)|(1u<<16)));
        assert(s.held_actions==0x10u); /* same bit, separate records */
        assert(s.event_slots==(1u<<3));
        assert(s.early_slots==(1u<<16));
        assert(spider_native_latch_observer_changed(&o,&s,2u));
        assert(!spider_native_latch_observer_changed(&o,&s,2u));
        assert(spider_native_latch_observer_changed(&o,&s,1u));
        records[3][0]=0;
        spider_native_latch_decode(records,&s);
        assert(s.held_slots==(1u<<16));
        assert(s.held_actions==0x10u);
        assert(spider_native_latch_observer_changed(&o,&s,1u));
        assert(o.emitted==3u);
        assert(!spider_native_latch_observer_changed(0,&s,1u));
        assert(!spider_native_latch_observer_changed(&o,0,1u));
        o.emitted=64u;
        assert(!spider_native_latch_observer_changed(&o,&s,9u));
        spider_native_latch_observer_reset(&o);
        assert(!o.emitted&&!o.have_previous);
        spider_native_latch_decode(0,&s);
        assert(s.held_slots==0&&s.held_actions==0);
        return 0;
      }
    """),encoding="utf-8")
    binary=tmp_path/"snapshot"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)

def test_observer_has_no_original_memory_access_and_no_runtime_enablement():
    h=(ROOT/"sdl3_native_latch_observer.h").read_text(encoding="utf-8")
    assert "spider_native_latch_decode" in h
    assert "spider_native_latch_observer_changed" in h
    assert "o->emitted>=64u" in h
    assert "persistent, NOT a one-frame press edge" in h
    for forbidden in ("windows.h","dinput.h","WriteProcessMemory",
                      "VirtualProtect","SendInput","0x00661100"):
        assert forbidden not in h
    for runtime in ("spiderman2k1fix.c","sdl3_gamepad_probe.c"):
        assert '#include "sdl3_native_latch_observer.h"' not in (
            ROOT/runtime).read_text(encoding="utf-8")
