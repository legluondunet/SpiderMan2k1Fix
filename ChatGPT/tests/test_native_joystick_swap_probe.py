"""Hot-swap native controller handoff: passive table fingerprint only."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
HEADER=(ROOT/"native_joystick_swap_probe.h").read_text(encoding="utf-8")
NATIVE=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
CFG=(ROOT/"ini_validation.h").read_text(encoding="utf-8")


def test_c99_native_action_table_fingerprint_canonical_and_custom(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    source=tmp_path/"swap_probe.c"
    binary=tmp_path/"swap_probe"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <stdint.h>
      #include <string.h>
      #include "native_joystick_swap_probe.h"
      static void set(uint8_t *b,unsigned at,uint32_t v) {
        spider_ps1_write_word(b,at,v);
      }
      int main(void) {
        uint8_t raw[SPIDER_NATIVE_SWAP_SLOTS*8u]={0};
        uint32_t found[SPIDER_PS1_BUTTON_COUNT]={0};
        unsigned i;
        for(i=0;i<SPIDER_NATIVE_SWAP_SLOTS;i++) {
          set(raw,i*8u,spider_native_swap_masks[i]);
          set(raw,i*8u+4u,0x4000u);
        }
        set(raw,13u*8u+4u,0u);
        set(raw,11u*8u+4u,11u);
        for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
          set(raw,(4u+i)*8u+4u,spider_ps1_buttons[i]);
        assert(spider_native_swap_read(raw,sizeof(raw),found));
        assert(spider_native_swap_preset(found)==1);
        for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
          set(raw,(4u+i)*8u+4u,spider_xbox360_buttons[i]);
        assert(spider_native_swap_read(raw,sizeof(raw),found));
        assert(spider_native_swap_preset(found)==2);
        assert(!spider_native_swap_read(raw,sizeof(raw)-1u,found));
        assert(!spider_native_swap_read(NULL,sizeof(raw),found));
        assert(!spider_native_swap_read(raw,sizeof(raw),NULL));
        set(raw,4u*8u+4u,8u); /* custom mapping is safe for observing */
        assert(spider_native_swap_read(raw,sizeof(raw),found));
        assert(!spider_native_swap_preset(found));
        set(raw,4u*8u+4u,200u); /* impossible >127 native button */
        assert(!spider_native_swap_read(raw,sizeof(raw),found));
        set(raw,4u*8u+4u,0u);
        set(raw,8u*2u,0x123u); /* mismatched action identifier */
        assert(!spider_native_swap_read(raw,sizeof(raw),found));
        set(raw,8u*2u,spider_native_swap_masks[2u]);
        set(raw,0u*8u+4u,0u); /* original unused must stay none */
        assert(!spider_native_swap_read(raw,sizeof(raw),found));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_no_live_original_binding_writes_in_hot_swap_probe():
    assert '#include "native_joystick_swap_probe.h"' in NATIVE
    assert "SPIDER_NATIVE_ACTION_TABLE 0x00568408u" in NATIVE
    assert '"NativeJoystickSwapProbe"' in CFG
    assert 'GetPrivateProfileIntA("Debug",' in NATIVE
    assert '"NativeJoystickSwapProbe",0,ini)==1' in NATIVE
    assert "static void observe_native_swap_table(" in NATIVE
    assert 'observe_native_swap_table("startup",swap_last_kind,swap_last_kind);' in NATIVE
    assert 'observe_native_swap_table("rebound",swap_last_kind,list.kind);' in NATIVE
    assert "swap_last_kind=list.kind;" in NATIVE
    assert "original native action mapping; no unsafe runtime patch attempted" in NATIVE
    observer=NATIVE.split("static void observe_native_swap_table(",1)[1].split(
      "static void close_candidate(",1)[0]
    assert "spider_native_swap_read(table," in observer
    assert "spider_native_swap_preset(buttons)" in observer
    assert "VirtualQuery(table,&mbi,sizeof(mbi))" in observer
    assert "mbi.State==MEM_COMMIT" in observer
    assert "swap_probe_reports>=24u" in observer
    for forbidden in ("WriteProcessMemory(", "VirtualProtect(", "SendInput(",
                      "InterlockedExchange(", "spider_ps1_write_word(",
                      "IDirectInputDevice8_", "memcpy(table", "memset(table"):
      assert forbidden not in observer
    assert "no runtime mapping writes" in NATIVE
