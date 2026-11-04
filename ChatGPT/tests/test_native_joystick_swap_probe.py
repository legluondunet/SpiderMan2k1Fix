"""Read-only retail 14-action table audit, with NO vendor button presets."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
HEADER=(ROOT/"native_joystick_swap_probe.h").read_text(encoding="utf-8")
NATIVE=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")


def test_c99_native_table_integrity_does_not_assume_xbox_or_ds4(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    source=tmp_path/"native_table.c"
    binary=tmp_path/"native_table"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <stdint.h>
      #include "native_joystick_swap_probe.h"
      static void set(uint8_t *b,unsigned at,uint32_t v) {
        b[at]=(uint8_t)v; b[at+1u]=(uint8_t)(v>>8);
        b[at+2u]=(uint8_t)(v>>16); b[at+3u]=(uint8_t)(v>>24);
      }
      int main(void) {
        uint8_t raw[SPIDER_NATIVE_SWAP_SLOTS*8u]={0};
        uint32_t found[SPIDER_NATIVE_GAMEPLAY_ACTIONS]={0};
        unsigned i;
        for(i=0;i<SPIDER_NATIVE_SWAP_SLOTS;i++) {
          set(raw,i*8u,spider_native_swap_masks[i]);
          set(raw,i*8u+4u,0x4000u);
        }
        set(raw,13u*8u+4u,0u);
        for(i=0;i<SPIDER_NATIVE_GAMEPLAY_ACTIONS;i++)
          set(raw,(SPIDER_NATIVE_SWAP_FIRST_ACTION+i)*8u+4u,13u+i);
        assert(spider_native_swap_read(raw,sizeof(raw),found));
        for(i=0;i<SPIDER_NATIVE_GAMEPLAY_ACTIONS;i++)
          assert(found[i]==13u+i);
        assert(!spider_native_swap_read(raw,sizeof(raw)-1u,found));
        assert(!spider_native_swap_read(NULL,sizeof(raw),found));
        assert(!spider_native_swap_read(raw,sizeof(raw),NULL));
        set(raw,4u*8u+4u,200u);
        assert(!spider_native_swap_read(raw,sizeof(raw),found));
        set(raw,4u*8u+4u,13u);
        set(raw,16u,0x123u);
        assert(!spider_native_swap_read(raw,sizeof(raw),found));
        set(raw,16u,spider_native_swap_masks[2u]);
        set(raw,4u,0u); /* original unused entry must stay unbound */
        assert(!spider_native_swap_read(raw,sizeof(raw),found));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_reconnect_stays_read_only_and_does_not_assume_physical_preset():
    assert "spider_native_swap_read(" in HEADER
    assert "spider_native_swap_preset(" not in HEADER
    assert "spider_ps1_buttons" not in HEADER
    assert "spider_xbox360_buttons" not in HEADER
    observe=NATIVE.split("static void observe_native_swap_table(",1)[1].split(
        "int SpiderNativeReconnect_CanonicalXboxMapping(",1)[0]
    assert "VirtualQuery(table,&mbi,sizeof(mbi))" in observe
    assert "mbi.State==MEM_COMMIT" in observe
    assert "spider_native_swap_read(table," in observe
    assert "swap_probe_reports>=24u" in observe
    for forbidden in ("WriteProcessMemory(", "VirtualProtect(", "SendInput(",
                      "InterlockedExchange(", "IDirectInputDevice8_", "memcpy(table"):
        assert forbidden not in observe
    assert "swap_mapping_enabled=0;" in NATIVE
